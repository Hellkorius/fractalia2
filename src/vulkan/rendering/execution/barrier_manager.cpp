#include "barrier_manager.h"
#include "../frame_graph_node_base.h"
#include "../../core/vulkan_context.h"
#include "../../core/vulkan_function_loader.h"
#include "../resources/resource_manager.h"
#include <algorithm>

namespace FrameGraphExecution {

void BarrierManager::initialize(const VulkanContext* context) {
    context_ = context;
}

void BarrierManager::analyzeBarrierRequirements(const std::vector<FrameGraphTypes::NodeId>& executionOrder,
                                                 const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes) {
    // Clear previous tracking
    resourceWriteTracking_.clear();
    
    // Single pass analysis - O(n) complexity
    for (auto nodeId : executionOrder) {
        auto nodeIt = nodes.find(nodeId);
        if (nodeIt == nodes.end()) continue;
        
        auto& node = nodeIt->second;
        
        // Track resource writes for dependency analysis
        auto outputs = node->getOutputs();
        for (const auto& output : outputs) {
            resourceWriteTracking_[output.resourceId] = {nodeId, output.stage, output.access};
        }
    }
}

void BarrierManager::createOptimalBarrierBatches(const std::vector<FrameGraphTypes::NodeId>& executionOrder,
                                                  const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes) {
    barrierBatches_.clear();
    
    // Analyze ALL nodes for dependency barriers (compute-to-compute, compute-to-graphics, graphics-to-compute)
    for (auto nodeId : executionOrder) {
        auto nodeIt = nodes.find(nodeId);
        if (nodeIt == nodes.end()) continue;
        
        auto& node = nodeIt->second;
        auto inputs = node->getInputs();
        
        for (const auto& input : inputs) {
            auto writeIt = resourceWriteTracking_.find(input.resourceId);
            if (writeIt != resourceWriteTracking_.end()) {
                auto& writeInfo = writeIt->second;
                auto writerNodeIt = nodes.find(writeInfo.writerNode);
                
                if (writerNodeIt != nodes.end()) {
                    // Check if barrier is needed for any queue transition or access pattern change
                    bool needsBarrier = false;
                    
                    // Compute-to-graphics: Always needs barrier for queue family transfer
                    if (writerNodeIt->second->needsComputeQueue() && node->needsGraphicsQueue()) {
                        needsBarrier = true;
                    }
                    // Compute-to-compute: Needs barrier for write-after-write or read-after-write hazards
                    else if (writerNodeIt->second->needsComputeQueue() && node->needsComputeQueue()) {
                        needsBarrier = (writeInfo.access != ResourceAccess::Read || input.access != ResourceAccess::Read);
                    }
                    // Graphics-to-compute: Needs barrier for queue family transfer
                    else if (writerNodeIt->second->needsGraphicsQueue() && node->needsComputeQueue()) {
                        needsBarrier = true;
                    }
                    // Graphics-to-graphics: Needs barrier for write-after-write hazards
                    else if (writerNodeIt->second->needsGraphicsQueue() && node->needsGraphicsQueue()) {
                        needsBarrier = (writeInfo.access != ResourceAccess::Read);
                    }
                    
                    if (needsBarrier) {
                        addResourceBarrier(input.resourceId, nodeId, writeInfo.stage, input.stage, 
                                         writeInfo.access, input.access);
                    }
                }
            }
        }
    }
}

void BarrierManager::insertBarriersForNode(FrameGraphTypes::NodeId nodeId, VkCommandBuffer commandBuffer, 
                                           bool& computeExecuted, bool nodeNeedsGraphics) {
    // Insert barriers for ALL transition types, not just compute-to-graphics
    // Find barriers targeting this node
    for (const auto& batch : barrierBatches_) {
        if (batch.targetNodeId == nodeId) {
            insertBarrierBatch(batch, commandBuffer);
        }
    }
}

void BarrierManager::setResourceAccessors(std::function<const FrameGraphResources::FrameGraphBuffer*(FrameGraphTypes::ResourceId)> getBuffer,
                                           std::function<const FrameGraphResources::FrameGraphImage*(FrameGraphTypes::ResourceId)> getImage) {
    getBufferResource_ = getBuffer;
    getImageResource_ = getImage;
}

void BarrierManager::reset() {
    barrierBatches_.clear();
    resourceWriteTracking_.clear();
}

void BarrierManager::addResourceBarrier(FrameGraphTypes::ResourceId resourceId, FrameGraphTypes::NodeId targetNode,
                                       PipelineStage srcStage, PipelineStage dstStage,
                                       ResourceAccess srcAccess, ResourceAccess dstAccess) {
    
    // Find or create barrier batch for this target node
    auto batchIt = std::find_if(barrierBatches_.begin(), barrierBatches_.end(),
        [targetNode](const NodeBarrierInfo& batch) { return batch.targetNodeId == targetNode; });
    
    if (batchIt == barrierBatches_.end()) {
        barrierBatches_.emplace_back();
        batchIt = barrierBatches_.end() - 1;
        batchIt->targetNodeId = targetNode;
    }
    
    // Add resource barrier to the batch with deduplication within the same batch
    if (getBufferResource_) {
        const FrameGraphResources::FrameGraphBuffer* buffer = getBufferResource_(resourceId);
        if (buffer) {
            VkBufferMemoryBarrier2 barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            barrier.srcStageMask = convertPipelineStage2(srcStage);
            barrier.srcAccessMask = convertAccess2(srcAccess, srcStage);
            barrier.dstStageMask = convertPipelineStage2(dstStage);
            barrier.dstAccessMask = convertAccess2(dstAccess, dstStage);
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.buffer = buffer->buffer.get();
            barrier.offset = 0;
            barrier.size = VK_WHOLE_SIZE;
            
            // Check for duplicate barriers within the same batch
            bool isDuplicate = false;
            for (const auto& existing : batchIt->bufferBarriers) {
                if (existing.buffer == barrier.buffer &&
                    existing.srcStageMask == barrier.srcStageMask &&
                    existing.srcAccessMask == barrier.srcAccessMask &&
                    existing.dstStageMask == barrier.dstStageMask &&
                    existing.dstAccessMask == barrier.dstAccessMask &&
                    existing.offset == barrier.offset &&
                    existing.size == barrier.size) {
                    isDuplicate = true;
                    break;
                }
            }
            
            if (!isDuplicate) {
                batchIt->bufferBarriers.push_back(barrier);
            }
            return;
        }
    }
    
    if (getImageResource_) {
        const FrameGraphResources::FrameGraphImage* image = getImageResource_(resourceId);
        if (image) {
            VkImageMemoryBarrier2 barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            barrier.srcStageMask = convertPipelineStage2(srcStage);
            barrier.srcAccessMask = convertAccess2(srcAccess, srcStage);
            barrier.dstStageMask = convertPipelineStage2(dstStage);
            barrier.dstAccessMask = convertAccess2(dstAccess, dstStage);
            barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image->image.get();
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            
            // Check for duplicate image barriers within the same batch
            bool isDuplicate = false;
            for (const auto& existing : batchIt->imageBarriers) {
                if (existing.image == barrier.image &&
                    existing.srcStageMask == barrier.srcStageMask &&
                    existing.srcAccessMask == barrier.srcAccessMask &&
                    existing.dstStageMask == barrier.dstStageMask &&
                    existing.dstAccessMask == barrier.dstAccessMask &&
                    existing.oldLayout == barrier.oldLayout &&
                    existing.newLayout == barrier.newLayout &&
                    existing.subresourceRange.aspectMask == barrier.subresourceRange.aspectMask &&
                    existing.subresourceRange.baseMipLevel == barrier.subresourceRange.baseMipLevel &&
                    existing.subresourceRange.levelCount == barrier.subresourceRange.levelCount) {
                    isDuplicate = true;
                    break;
                }
            }
            
            if (!isDuplicate) {
                batchIt->imageBarriers.push_back(barrier);
            }
        }
    }
}

FrameGraphTypes::NodeId BarrierManager::findNextGraphicsNode(FrameGraphTypes::NodeId fromNode,
                                                              const std::vector<FrameGraphTypes::NodeId>& executionOrder,
                                                              const std::unordered_map<FrameGraphTypes::NodeId, std::unique_ptr<FrameGraphNode>>& nodes) const {
    auto it = std::find(executionOrder.begin(), executionOrder.end(), fromNode);
    if (it == executionOrder.end()) return 0;
    
    for (++it; it != executionOrder.end(); ++it) {
        auto nodeIt = nodes.find(*it);
        if (nodeIt != nodes.end() && nodeIt->second->needsGraphicsQueue()) {
            return *it;
        }
    }
    return 0;
}

void BarrierManager::insertBarrierBatch(const NodeBarrierInfo& batch, VkCommandBuffer commandBuffer) {
    if (batch.bufferBarriers.empty() && batch.imageBarriers.empty()) return;
    
    const auto& vk = context_->getLoader();
    
    // Use Synchronization2 with unified VkDependencyInfo
    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.bufferMemoryBarrierCount = static_cast<uint32_t>(batch.bufferBarriers.size());
    dependencyInfo.pBufferMemoryBarriers = batch.bufferBarriers.data();
    dependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(batch.imageBarriers.size());
    dependencyInfo.pImageMemoryBarriers = batch.imageBarriers.data();
    
    vk.vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}

VkAccessFlags BarrierManager::convertAccess(ResourceAccess access, PipelineStage stage) const {
    switch (access) {
        case ResourceAccess::Read: 
            if (stage == PipelineStage::VertexShader) {
                return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
            }
            return VK_ACCESS_SHADER_READ_BIT;
        case ResourceAccess::Write: 
            return VK_ACCESS_SHADER_WRITE_BIT;
        case ResourceAccess::ReadWrite: 
            return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        default: 
            return 0;
    }
}

VkPipelineStageFlags BarrierManager::convertPipelineStage(PipelineStage stage) const {
    switch (stage) {
        case PipelineStage::ComputeShader:
            return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        case PipelineStage::VertexShader:
            return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        case PipelineStage::FragmentShader:
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        case PipelineStage::ColorAttachment:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case PipelineStage::DepthAttachment:
            return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        case PipelineStage::Transfer:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        default:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
}

// Synchronization2 conversion methods with enhanced pipeline stage flags
VkAccessFlags2 BarrierManager::convertAccess2(ResourceAccess access, PipelineStage stage) const {
    switch (access) {
        case ResourceAccess::Read: 
            if (stage == PipelineStage::VertexShader) {
                return VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT;
            }
            return VK_ACCESS_2_SHADER_READ_BIT;
        case ResourceAccess::Write: 
            return VK_ACCESS_2_SHADER_WRITE_BIT;
        case ResourceAccess::ReadWrite: 
            return VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
        default: 
            return VK_ACCESS_2_NONE;
    }
}

VkPipelineStageFlags2 BarrierManager::convertPipelineStage2(PipelineStage stage) const {
    switch (stage) {
        case PipelineStage::ComputeShader:
            return VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        case PipelineStage::VertexShader:
            return VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
        case PipelineStage::FragmentShader:
            return VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        case PipelineStage::ColorAttachment:
            return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        case PipelineStage::DepthAttachment:
            return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        case PipelineStage::Transfer:
            return VK_PIPELINE_STAGE_2_COPY_BIT;
        default:
            return VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    }
}

} // namespace FrameGraphExecution
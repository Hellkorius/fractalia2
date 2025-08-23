#include "compute_dispatcher.h"
#include <iostream>
#include <cmath>

namespace {
    // Helper functions to convert legacy synchronization flags to Synchronization2
    VkPipelineStageFlags2 convertLegacyPipelineStage(VkPipelineStageFlags legacyStage) {
        switch (legacyStage) {
            case VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:
                return VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            case VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT:
                return VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
            case VK_PIPELINE_STAGE_VERTEX_INPUT_BIT:
                return VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
            case VK_PIPELINE_STAGE_VERTEX_SHADER_BIT:
                return VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
            case VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:
                return VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            case VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT:
                return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
            case VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT:
                return VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            case VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT:
                return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            case VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT:
                return VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            case VK_PIPELINE_STAGE_TRANSFER_BIT:
                return VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            case VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT:
                return VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            case VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT:
                return VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
            case VK_PIPELINE_STAGE_ALL_COMMANDS_BIT:
                return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            default:
                return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT; // Conservative fallback
        }
    }
    
    VkAccessFlags2 convertLegacyAccessFlags(VkAccessFlags legacyAccess) {
        VkAccessFlags2 result = 0;
        
        if (legacyAccess & VK_ACCESS_INDIRECT_COMMAND_READ_BIT)
            result |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
        if (legacyAccess & VK_ACCESS_INDEX_READ_BIT)
            result |= VK_ACCESS_2_INDEX_READ_BIT;
        if (legacyAccess & VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)
            result |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
        if (legacyAccess & VK_ACCESS_UNIFORM_READ_BIT)
            result |= VK_ACCESS_2_UNIFORM_READ_BIT;
        if (legacyAccess & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT)
            result |= VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT;
        if (legacyAccess & VK_ACCESS_SHADER_READ_BIT)
            result |= VK_ACCESS_2_SHADER_READ_BIT;
        if (legacyAccess & VK_ACCESS_SHADER_WRITE_BIT)
            result |= VK_ACCESS_2_SHADER_WRITE_BIT;
        if (legacyAccess & VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
            result |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
        if (legacyAccess & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
            result |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        if (legacyAccess & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)
            result |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        if (legacyAccess & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
            result |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        if (legacyAccess & VK_ACCESS_TRANSFER_READ_BIT)
            result |= VK_ACCESS_2_TRANSFER_READ_BIT;
        if (legacyAccess & VK_ACCESS_TRANSFER_WRITE_BIT)
            result |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
        if (legacyAccess & VK_ACCESS_HOST_READ_BIT)
            result |= VK_ACCESS_2_HOST_READ_BIT;
        if (legacyAccess & VK_ACCESS_HOST_WRITE_BIT)
            result |= VK_ACCESS_2_HOST_WRITE_BIT;
        if (legacyAccess & VK_ACCESS_MEMORY_READ_BIT)
            result |= VK_ACCESS_2_MEMORY_READ_BIT;
        if (legacyAccess & VK_ACCESS_MEMORY_WRITE_BIT)
            result |= VK_ACCESS_2_MEMORY_WRITE_BIT;
            
        return result;
    }
}

ComputeDispatcher::ComputeDispatcher(VulkanContext* ctx) : VulkanManagerBase(ctx) {}

void ComputeDispatcher::dispatch(VkCommandBuffer commandBuffer, const ComputeDispatch& dispatch) {
    stats_.dispatchesThisFrame++;
    stats_.totalDispatches++;
    
    if (!validateDispatch(dispatch)) {
        return;
    }
    
    cmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dispatch.pipeline);
    
    if (!dispatch.descriptorSets.empty()) {
        cmdBindDescriptorSets(
            commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dispatch.layout,
            0, static_cast<uint32_t>(dispatch.descriptorSets.size()),
            dispatch.descriptorSets.data(), 0, nullptr);
    }
    
    if (dispatch.pushConstantData && dispatch.pushConstantSize > 0) {
        cmdPushConstants(
            commandBuffer, dispatch.layout, dispatch.pushConstantStages,
            0, dispatch.pushConstantSize, dispatch.pushConstantData);
    }
    
    if (!dispatch.memoryBarriers.empty() || !dispatch.bufferBarriers.empty() || !dispatch.imageBarriers.empty()) {
        insertOptimalBarriers(commandBuffer, dispatch.bufferBarriers, dispatch.imageBarriers);
    }
    
    cmdDispatch(commandBuffer, 
               dispatch.groupCountX, 
               dispatch.groupCountY, 
               dispatch.groupCountZ);
}

void ComputeDispatcher::dispatchIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset) {
    stats_.dispatchesThisFrame++;
    stats_.totalDispatches++;
    
    cmdDispatchIndirect(commandBuffer, buffer, offset);
}

void ComputeDispatcher::dispatchBuffer(VkCommandBuffer commandBuffer, 
                                     VkPipeline pipeline, VkPipelineLayout layout,
                                     uint32_t elementCount, const glm::uvec3& workgroupSize,
                                     const std::vector<VkDescriptorSet>& descriptorSets,
                                     const void* pushConstants, uint32_t pushConstantSize) {
    ComputeDispatch dispatch{};
    dispatch.pipeline = pipeline;
    dispatch.layout = layout;
    dispatch.descriptorSets = descriptorSets;
    dispatch.pushConstantData = pushConstants;
    dispatch.pushConstantSize = pushConstantSize;
    
    dispatch.calculateOptimalDispatch(elementCount, workgroupSize);
    
    this->dispatch(commandBuffer, dispatch);
}

void ComputeDispatcher::dispatchImage(VkCommandBuffer commandBuffer,
                                    VkPipeline pipeline, VkPipelineLayout layout,
                                    uint32_t width, uint32_t height, const glm::uvec3& workgroupSize,
                                    const std::vector<VkDescriptorSet>& descriptorSets,
                                    const void* pushConstants, uint32_t pushConstantSize) {
    ComputeDispatch dispatch{};
    dispatch.pipeline = pipeline;
    dispatch.layout = layout;
    dispatch.descriptorSets = descriptorSets;
    dispatch.pushConstantData = pushConstants;
    dispatch.pushConstantSize = pushConstantSize;
    
    dispatch.groupCountX = (width + workgroupSize.x - 1) / workgroupSize.x;
    dispatch.groupCountY = (height + workgroupSize.y - 1) / workgroupSize.y;
    dispatch.groupCountZ = 1;
    
    this->dispatch(commandBuffer, dispatch);
}

void ComputeDispatcher::insertOptimalBarriers(VkCommandBuffer commandBuffer,
                                             const std::vector<VkBufferMemoryBarrier>& bufferBarriers,
                                             const std::vector<VkImageMemoryBarrier>& imageBarriers,
                                             VkPipelineStageFlags srcStage,
                                             VkPipelineStageFlags dstStage) {
    if (bufferBarriers.empty() && imageBarriers.empty()) {
        return;
    }
    
    auto optimizedBufferBarriers = optimizeBufferBarriers(bufferBarriers);
    
    // Convert legacy barriers to Synchronization2
    std::vector<VkBufferMemoryBarrier2> bufferBarriers2;
    bufferBarriers2.reserve(optimizedBufferBarriers.size());
    for (const auto& barrier : optimizedBufferBarriers) {
        VkBufferMemoryBarrier2 barrier2{};
        barrier2.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
        barrier2.srcStageMask = convertLegacyPipelineStage(srcStage);
        barrier2.srcAccessMask = convertLegacyAccessFlags(barrier.srcAccessMask);
        barrier2.dstStageMask = convertLegacyPipelineStage(dstStage);
        barrier2.dstAccessMask = convertLegacyAccessFlags(barrier.dstAccessMask);
        barrier2.srcQueueFamilyIndex = barrier.srcQueueFamilyIndex;
        barrier2.dstQueueFamilyIndex = barrier.dstQueueFamilyIndex;
        barrier2.buffer = barrier.buffer;
        barrier2.offset = barrier.offset;
        barrier2.size = barrier.size;
        bufferBarriers2.push_back(barrier2);
    }
    
    std::vector<VkImageMemoryBarrier2> imageBarriers2;
    imageBarriers2.reserve(imageBarriers.size());
    for (const auto& barrier : imageBarriers) {
        VkImageMemoryBarrier2 barrier2{};
        barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier2.srcStageMask = convertLegacyPipelineStage(srcStage);
        barrier2.srcAccessMask = convertLegacyAccessFlags(barrier.srcAccessMask);
        barrier2.dstStageMask = convertLegacyPipelineStage(dstStage);
        barrier2.dstAccessMask = convertLegacyAccessFlags(barrier.dstAccessMask);
        barrier2.srcQueueFamilyIndex = barrier.srcQueueFamilyIndex;
        barrier2.dstQueueFamilyIndex = barrier.dstQueueFamilyIndex;
        barrier2.image = barrier.image;
        barrier2.subresourceRange = barrier.subresourceRange;
        barrier2.oldLayout = barrier.oldLayout;
        barrier2.newLayout = barrier.newLayout;
        imageBarriers2.push_back(barrier2);
    }
    
    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.bufferMemoryBarrierCount = static_cast<uint32_t>(bufferBarriers2.size());
    dependencyInfo.pBufferMemoryBarriers = bufferBarriers2.empty() ? nullptr : bufferBarriers2.data();
    dependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(imageBarriers2.size());
    dependencyInfo.pImageMemoryBarriers = imageBarriers2.empty() ? nullptr : imageBarriers2.data();
    
    loader->vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}

void ComputeDispatcher::resetFrameStats() {
    stats_.dispatchesThisFrame = 0;
}

bool ComputeDispatcher::validateDispatch(const ComputeDispatch& dispatch) const {
    if (dispatch.pipeline == VK_NULL_HANDLE) {
        std::cerr << "ComputeDispatcher: Invalid pipeline handle" << std::endl;
        return false;
    }
    if (dispatch.layout == VK_NULL_HANDLE) {
        std::cerr << "ComputeDispatcher: Invalid pipeline layout handle" << std::endl;
        return false;
    }
    if (dispatch.groupCountX == 0 || dispatch.groupCountY == 0 || dispatch.groupCountZ == 0) {
        std::cerr << "ComputeDispatcher: Invalid dispatch size: " 
                  << dispatch.groupCountX << "x" << dispatch.groupCountY << "x" << dispatch.groupCountZ << std::endl;
        return false;
    }
    return true;
}

std::vector<VkBufferMemoryBarrier> ComputeDispatcher::optimizeBufferBarriers(
    const std::vector<VkBufferMemoryBarrier>& barriers) const {
    if (barriers.empty()) {
        return {};
    }
    
    if (barriers.size() == 1) {
        return {barriers[0]};
    }
    
    std::vector<VkBufferMemoryBarrier> result;
    result.reserve(barriers.size());
    for (const auto& barrier : barriers) {
        result.push_back(barrier);
    }
    return result;
}
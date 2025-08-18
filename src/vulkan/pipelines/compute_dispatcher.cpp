#include "compute_dispatcher.h"
#include <iostream>
#include <cmath>

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
    
    cmdPipelineBarrier(
        commandBuffer,
        srcStage, dstStage,
        0,
        0, nullptr,
        static_cast<uint32_t>(optimizedBufferBarriers.size()), optimizedBufferBarriers.data(),
        static_cast<uint32_t>(imageBarriers.size()), imageBarriers.data()
    );
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
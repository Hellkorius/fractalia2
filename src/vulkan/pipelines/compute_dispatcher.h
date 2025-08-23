#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include <glm/glm.hpp>
#include "../core/vulkan_context.h"

#include "compute_pipeline_types.h"

class ComputeDispatcher {
public:
    explicit ComputeDispatcher(VulkanContext* ctx);
    ~ComputeDispatcher() = default;

    struct DispatchStats {
        uint32_t dispatchesThisFrame = 0;
        uint64_t totalDispatches = 0;
    };

    void dispatch(VkCommandBuffer commandBuffer, const ComputeDispatch& dispatch);
    void dispatchIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset);
    
    void dispatchBuffer(VkCommandBuffer commandBuffer, 
                       VkPipeline pipeline, VkPipelineLayout layout,
                       uint32_t elementCount, const glm::uvec3& workgroupSize,
                       const std::vector<VkDescriptorSet>& descriptorSets,
                       const void* pushConstants = nullptr, uint32_t pushConstantSize = 0);
    
    void dispatchImage(VkCommandBuffer commandBuffer,
                      VkPipeline pipeline, VkPipelineLayout layout,
                      uint32_t width, uint32_t height, const glm::uvec3& workgroupSize,
                      const std::vector<VkDescriptorSet>& descriptorSets,
                      const void* pushConstants = nullptr, uint32_t pushConstantSize = 0);
    
    void insertOptimalBarriers(VkCommandBuffer commandBuffer,
                              const std::vector<VkBufferMemoryBarrier>& bufferBarriers,
                              const std::vector<VkImageMemoryBarrier>& imageBarriers,
                              VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    
    DispatchStats getStats() const { return stats_; }
    void resetFrameStats();

private:
    VulkanContext* context;
    mutable DispatchStats stats_;
    
    bool validateDispatch(const ComputeDispatch& dispatch) const;
    std::vector<VkBufferMemoryBarrier> optimizeBufferBarriers(const std::vector<VkBufferMemoryBarrier>& barriers) const;
};
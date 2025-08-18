#pragma once

#include "vulkan_context.h"
#include "vulkan_function_loader.h"
#include <vulkan/vulkan.h>

// Base class for Vulkan managers to reduce repetitive function loading patterns
// Provides cached references and convenience wrappers for common Vulkan operations
class VulkanManagerBase {
protected:
    VulkanContext* context;
    
    // Cached references for performance (updated when context changes)
    const VulkanFunctionLoader* loader;
    VkDevice device;
    
    explicit VulkanManagerBase(VulkanContext* ctx) : context(ctx) {
        updateCachedReferences();
    }
    
    // Update cached references when context changes
    void updateCachedReferences() {
        loader = &context->getLoader();
        device = context->getDevice();
    }
    
    // Pipeline management wrappers
    VkResult createGraphicsPipelines(VkPipelineCache pipelineCache, uint32_t createInfoCount,
                                   const VkGraphicsPipelineCreateInfo* pCreateInfos, VkPipeline* pPipelines) {
        return loader->vkCreateGraphicsPipelines(device, pipelineCache, createInfoCount, pCreateInfos, nullptr, pPipelines);
    }
    
    VkResult createComputePipelines(VkPipelineCache pipelineCache, uint32_t createInfoCount,
                                  const VkComputePipelineCreateInfo* pCreateInfos, VkPipeline* pPipelines) {
        return loader->vkCreateComputePipelines(device, pipelineCache, createInfoCount, pCreateInfos, nullptr, pPipelines);
    }
    
    void destroyPipeline(VkPipeline pipeline) {
        loader->vkDestroyPipeline(device, pipeline, nullptr);
    }
    
    VkResult vkCreatePipelineLayoutWrapper(const VkPipelineLayoutCreateInfo* pCreateInfo, VkPipelineLayout* pPipelineLayout) {
        return loader->vkCreatePipelineLayout(device, pCreateInfo, nullptr, pPipelineLayout);
    }
    
    void destroyPipelineLayout(VkPipelineLayout pipelineLayout) {
        loader->vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    }
    
    VkResult createPipelineCache(const VkPipelineCacheCreateInfo* pCreateInfo, VkPipelineCache* pPipelineCache) {
        return loader->vkCreatePipelineCache(device, pCreateInfo, nullptr, pPipelineCache);
    }
    
    void destroyPipelineCache(VkPipelineCache pipelineCache) {
        loader->vkDestroyPipelineCache(device, pipelineCache, nullptr);
    }
    
    VkResult vkCreateRenderPassWrapper(const VkRenderPassCreateInfo* pCreateInfo, VkRenderPass* pRenderPass) {
        return loader->vkCreateRenderPass(device, pCreateInfo, nullptr, pRenderPass);
    }
    
    void destroyRenderPass(VkRenderPass renderPass) {
        loader->vkDestroyRenderPass(device, renderPass, nullptr);
    }
    
    void destroyShaderModule(VkShaderModule shaderModule) {
        loader->vkDestroyShaderModule(device, shaderModule, nullptr);
    }
    
    // Command buffer wrappers
    void cmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) {
        loader->vkCmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
    }
    
    void cmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
                             VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount,
                             const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount,
                             const uint32_t* pDynamicOffsets) {
        loader->vkCmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet, 
                                      descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
    }
    
    void cmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags,
                        uint32_t offset, uint32_t size, const void* pValues) {
        loader->vkCmdPushConstants(commandBuffer, layout, stageFlags, offset, size, pValues);
    }
    
    void cmdDispatch(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
        loader->vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
    }
    
    void cmdDispatchIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset) {
        loader->vkCmdDispatchIndirect(commandBuffer, buffer, offset);
    }
    
    void cmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
                          VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers,
                          uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers,
                          uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) {
        loader->vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags,
                                   memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers,
                                   imageMemoryBarrierCount, pImageMemoryBarriers);
    }
};
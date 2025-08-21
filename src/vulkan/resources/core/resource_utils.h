#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include "../../core/vulkan_raii.h"

class VulkanFunctionLoader;

// Resource management utility functions for descriptor sets, pools, and resource allocation
// Eliminates code duplication within the resources subsystem and related areas
class ResourceUtils {
public:
    ResourceUtils() = delete;
    
    // Descriptor pool utilities
    static VkDescriptorPool createDescriptorPool(VkDevice device,
                                                const VulkanFunctionLoader& loader,
                                                const std::vector<VkDescriptorPoolSize>& poolSizes,
                                                uint32_t maxSets,
                                                VkDescriptorPoolCreateFlags flags = 0);
    
    static VkDescriptorPool createVariableDescriptorPool(VkDevice device,
                                                        const VulkanFunctionLoader& loader,
                                                        uint32_t uniformBuffers,
                                                        uint32_t storageBuffers,
                                                        uint32_t combinedImageSamplers,
                                                        uint32_t storageImages,
                                                        uint32_t maxSets);
    
    // Descriptor set utilities
    static VkResult allocateDescriptorSet(VkDevice device,
                                        const VulkanFunctionLoader& loader,
                                        VkDescriptorPool pool,
                                        VkDescriptorSetLayout layout,
                                        VkDescriptorSet& descriptorSet);
    
    static VkResult allocateDescriptorSets(VkDevice device,
                                         const VulkanFunctionLoader& loader,
                                         VkDescriptorPool pool,
                                         const std::vector<VkDescriptorSetLayout>& layouts,
                                         std::vector<VkDescriptorSet>& descriptorSets);
    
    // Descriptor update utilities
    static VkWriteDescriptorSet createBufferWrite(VkDescriptorSet dstSet,
                                                 uint32_t binding,
                                                 VkDescriptorType type,
                                                 const VkDescriptorBufferInfo* bufferInfo,
                                                 uint32_t descriptorCount = 1);
    
    static VkWriteDescriptorSet createImageWrite(VkDescriptorSet dstSet,
                                               uint32_t binding,
                                               VkDescriptorType type,
                                               const VkDescriptorImageInfo* imageInfo,
                                               uint32_t descriptorCount = 1);
    
    static void updateDescriptorSets(VkDevice device,
                                   const VulkanFunctionLoader& loader,
                                   const std::vector<VkWriteDescriptorSet>& writes);
    
    // Buffer creation utilities (more specialized than VulkanUtils)
    static VkResult createStagingBuffer(VkDevice device,
                                      VkPhysicalDevice physicalDevice,
                                      const VulkanFunctionLoader& loader,
                                      VkDeviceSize size,
                                      VkBuffer& buffer,
                                      VkDeviceMemory& memory,
                                      void** mappedData = nullptr);
    
    static VkResult createUniformBuffer(VkDevice device,
                                      VkPhysicalDevice physicalDevice,
                                      const VulkanFunctionLoader& loader,
                                      VkDeviceSize size,
                                      VkBuffer& buffer,
                                      VkDeviceMemory& memory);
    
    static VkResult createStorageBuffer(VkDevice device,
                                      VkPhysicalDevice physicalDevice,
                                      const VulkanFunctionLoader& loader,
                                      VkDeviceSize size,
                                      VkBufferUsageFlags additionalUsage,
                                      VkBuffer& buffer,
                                      VkDeviceMemory& memory);
    
    // Memory mapping utilities
    static VkResult mapBufferMemory(VkDevice device,
                                  const VulkanFunctionLoader& loader,
                                  VkDeviceMemory memory,
                                  VkDeviceSize offset,
                                  VkDeviceSize size,
                                  void** data);
    
    static void unmapBufferMemory(VkDevice device,
                                const VulkanFunctionLoader& loader,
                                VkDeviceMemory memory);
    
    static VkResult flushBufferMemory(VkDevice device,
                                    const VulkanFunctionLoader& loader,
                                    VkDeviceMemory memory,
                                    VkDeviceSize offset = 0,
                                    VkDeviceSize size = VK_WHOLE_SIZE);
    
    // Resource cleanup utilities
    static void destroyBuffer(VkDevice device,
                            const VulkanFunctionLoader& loader,
                            VkBuffer buffer,
                            VkDeviceMemory memory);
    
    static void destroyDescriptorPool(VkDevice device,
                                    const VulkanFunctionLoader& loader,
                                    VkDescriptorPool pool);
    
    // Descriptor pool size calculation utilities
    static std::vector<VkDescriptorPoolSize> calculatePoolSizes(
        const std::vector<VkDescriptorSetLayout>& layouts,
        const VulkanFunctionLoader& loader,
        uint32_t maxSets);
    
    static VkDescriptorPoolSize createPoolSize(VkDescriptorType type, uint32_t descriptorCount);
    
    // Buffer usage helper utilities
    static VkBufferUsageFlags getCommonStorageBufferUsage();
    static VkBufferUsageFlags getCommonUniformBufferUsage();
    static VkBufferUsageFlags getCommonStagingBufferUsage();
    
    // Memory property helpers
    static VkMemoryPropertyFlags getHostVisibleMemoryProperties();
    static VkMemoryPropertyFlags getDeviceLocalMemoryProperties();
    static VkMemoryPropertyFlags getHostCoherentMemoryProperties();
    
    // Error handling specific to resource operations
    static bool checkDescriptorResult(VkResult result, const char* operation);
    static bool checkBufferResult(VkResult result, const char* operation);
    
    // Debug utilities for resources
    static void setBufferDebugName(VkDevice device,
                                 const VulkanFunctionLoader& loader,
                                 VkBuffer buffer,
                                 const std::string& name);
    
    static void setDescriptorSetDebugName(VkDevice device,
                                        const VulkanFunctionLoader& loader,
                                        VkDescriptorSet descriptorSet,
                                        const std::string& name);
};
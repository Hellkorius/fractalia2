#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

class VulkanFunctionLoader;

// Consolidated utility functions for common Vulkan operations
// Eliminates code duplication across modules
class VulkanUtils {
public:
    VulkanUtils() = delete;
    
    // Memory management utilities
    static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, 
                                  const VulkanFunctionLoader& loader,
                                  uint32_t typeFilter, 
                                  VkMemoryPropertyFlags properties);
    
    // Buffer creation utilities
    static bool createBuffer(VkDevice device, 
                           const VulkanFunctionLoader& loader,
                           VkDeviceSize size,
                           VkBufferUsageFlags usage,
                           VkMemoryPropertyFlags properties,
                           VkBuffer& buffer,
                           VkDeviceMemory& bufferMemory);
    
    // Image creation utilities
    static bool createImage(VkDevice device,
                          VkPhysicalDevice physicalDevice,
                          const VulkanFunctionLoader& loader,
                          uint32_t width,
                          uint32_t height,
                          VkFormat format,
                          VkImageTiling tiling,
                          VkImageUsageFlags usage,
                          VkMemoryPropertyFlags properties,
                          VkImage& image,
                          VkDeviceMemory& imageMemory,
                          VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT);
    
    static VkImageView createImageView(VkDevice device,
                                     const VulkanFunctionLoader& loader,
                                     VkImage image,
                                     VkFormat format,
                                     VkImageAspectFlags aspectFlags);
    
    // File I/O utilities
    static std::vector<char> readFile(const std::string& filename);
    
    // Shader module utilities
    static VkShaderModule createShaderModule(VkDevice device,
                                            const VulkanFunctionLoader& loader,
                                            const std::vector<char>& code);
    
    // Command buffer utilities
    static VkCommandBuffer beginSingleTimeCommands(VkDevice device,
                                                  const VulkanFunctionLoader& loader,
                                                  VkCommandPool commandPool);
    
    static void endSingleTimeCommands(VkDevice device,
                                    const VulkanFunctionLoader& loader,
                                    VkQueue queue,
                                    VkCommandPool commandPool,
                                    VkCommandBuffer commandBuffer);
    
    // Image transition utilities (using Synchronization2)
    static void transitionImageLayout(VkDevice device,
                                    const VulkanFunctionLoader& loader,
                                    VkQueue queue,
                                    VkCommandPool commandPool,
                                    VkImage image,
                                    VkFormat format,
                                    VkImageLayout oldLayout,
                                    VkImageLayout newLayout);
    
    // Buffer copy utilities
    static void copyBuffer(VkDevice device,
                         const VulkanFunctionLoader& loader,
                         VkQueue queue,
                         VkCommandPool commandPool,
                         VkBuffer srcBuffer,
                         VkBuffer dstBuffer,
                         VkDeviceSize size);
    
    static void copyBufferToImage(VkDevice device,
                                const VulkanFunctionLoader& loader,
                                VkQueue queue,
                                VkCommandPool commandPool,
                                VkBuffer buffer,
                                VkImage image,
                                uint32_t width,
                                uint32_t height);
                                
    // Descriptor set utilities
    static void writeDescriptorSets(VkDevice device,
                                  const VulkanFunctionLoader& loader,
                                  VkDescriptorSet descriptorSet,
                                  const std::vector<VkDescriptorBufferInfo>& bufferInfos,
                                  VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    
    // Synchronization utilities (cross-domain: services, core, resources, monitoring)
    static VkFence createFence(VkDevice device,
                             const VulkanFunctionLoader& loader,
                             bool signaled = false);
    
    static VkSemaphore createSemaphore(VkDevice device,
                                     const VulkanFunctionLoader& loader);
    
    static void destroyFences(VkDevice device,
                            const VulkanFunctionLoader& loader,
                            const std::vector<VkFence>& fences);
    
    static void destroySemaphores(VkDevice device,
                                const VulkanFunctionLoader& loader,
                                const std::vector<VkSemaphore>& semaphores);
    
    static VkResult waitForFences(VkDevice device,
                                const VulkanFunctionLoader& loader,
                                const std::vector<VkFence>& fences,
                                bool waitAll = true,
                                uint64_t timeout = UINT64_MAX);
    
    // Command buffer utilities (cross-domain: services, core, resources, monitoring)
    static VkResult submitCommands(VkQueue queue,
                                 const VulkanFunctionLoader& loader,
                                 const std::vector<VkCommandBuffer>& commandBuffers,
                                 const std::vector<VkSemaphore>& waitSemaphores = {},
                                 const std::vector<VkPipelineStageFlags>& waitStages = {},
                                 const std::vector<VkSemaphore>& signalSemaphores = {},
                                 VkFence fence = VK_NULL_HANDLE);
    
    static VkResult allocateCommandBuffers(VkDevice device,
                                         const VulkanFunctionLoader& loader,
                                         VkCommandPool commandPool,
                                         uint32_t commandBufferCount,
                                         std::vector<VkCommandBuffer>& commandBuffers,
                                         VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    
    // Structure creation helpers (cross-domain utility functions)
    static VkSubmitInfo createSubmitInfo(const std::vector<VkCommandBuffer>& commandBuffers,
                                       const std::vector<VkSemaphore>& waitSemaphores = {},
                                       const std::vector<VkPipelineStageFlags>& waitStages = {},
                                       const std::vector<VkSemaphore>& signalSemaphores = {});
    
    static VkPresentInfoKHR createPresentInfo(const std::vector<VkSwapchainKHR>& swapchains,
                                            const std::vector<uint32_t>& imageIndices,
                                            const std::vector<VkSemaphore>& waitSemaphores = {});
    
    // Error handling utilities
    static bool checkVkResult(VkResult result, const char* operation);
    
    static void logVkResult(VkResult result, const char* operation);
};
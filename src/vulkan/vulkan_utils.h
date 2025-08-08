#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

// Forward declaration
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
                          VkDeviceMemory& imageMemory);
    
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
    
    // Image transition utilities (using vkCmdPipelineBarrier)
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
};
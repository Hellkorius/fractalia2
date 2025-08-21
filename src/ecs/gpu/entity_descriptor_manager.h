#pragma once

#include <vulkan/vulkan.h>

// Forward declarations
class VulkanContext;
class EntityBufferManager;
class ResourceCoordinator;

/**
 * EntityDescriptorManager - Isolated Vulkan descriptor management for entity rendering
 * 
 * Handles all descriptor set management for both compute and graphics pipelines:
 * - Descriptor set layout creation for compute (4 storage buffers) and graphics (1 uniform + 2 storage)
 * - Descriptor pool management with proper sizing for compute and graphics
 * - Descriptor set allocation and buffer binding updates
 * - Critical swapchain recreation support with proper descriptor set recreation
 * 
 * Extracted from GPUEntityManager to reduce complexity and isolate Vulkan boilerplate.
 */
class EntityDescriptorManager {
public:
    EntityDescriptorManager();
    ~EntityDescriptorManager();

    // Initialization and cleanup
    bool initialize(const VulkanContext& context, EntityBufferManager& bufferManager, ResourceCoordinator* resourceCoordinator = nullptr);
    void cleanup();

    // Descriptor set layout management
    bool createDescriptorSetLayouts();
    VkDescriptorSetLayout getComputeDescriptorSetLayout() const { return computeDescriptorSetLayout; }
    VkDescriptorSetLayout getGraphicsDescriptorSetLayout() const { return graphicsDescriptorSetLayout; }

    // Descriptor set management
    bool createComputeDescriptorSets(VkDescriptorSetLayout layout);
    bool createGraphicsDescriptorSets(VkDescriptorSetLayout layout);
    bool recreateComputeDescriptorSets(); // Critical for swapchain recreation
    
    // Descriptor set access
    VkDescriptorSet getComputeDescriptorSet() const { return computeDescriptorSet; }
    VkDescriptorSet getGraphicsDescriptorSet() const { return graphicsDescriptorSet; }

    // State queries
    bool isInitialized() const { return context != nullptr; }
    bool hasValidComputeDescriptorSet() const { return computeDescriptorSet != VK_NULL_HANDLE; }
    bool hasValidGraphicsDescriptorSet() const { return graphicsDescriptorSet != VK_NULL_HANDLE; }

private:
    // Dependencies
    const VulkanContext* context = nullptr;
    EntityBufferManager* bufferManager = nullptr;
    ResourceCoordinator* resourceCoordinator = nullptr;
    
    // Descriptor set layouts
    VkDescriptorSetLayout computeDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout graphicsDescriptorSetLayout = VK_NULL_HANDLE;
    
    // Descriptor pools
    VkDescriptorPool computeDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorPool graphicsDescriptorPool = VK_NULL_HANDLE;
    
    // Descriptor sets
    VkDescriptorSet computeDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet graphicsDescriptorSet = VK_NULL_HANDLE;
    
    // Helper methods
    bool createComputeDescriptorPool();
    bool createGraphicsDescriptorPool();
    bool updateComputeDescriptorSet();
    bool updateGraphicsDescriptorSet();
    
    // Cleanup helpers
    void cleanupDescriptorPools();
    void cleanupDescriptorSetLayouts();
};
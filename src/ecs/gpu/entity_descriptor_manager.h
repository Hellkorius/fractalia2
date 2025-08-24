#pragma once

#include "../../vulkan/resources/descriptors/descriptor_set_manager_base.h"
#include "../../vulkan/core/vulkan_raii.h"
#include "entity_descriptor_bindings.h"
#include "entity_buffer_types.h"
#include <vulkan/vulkan.h>

// Forward declarations
class EntityBufferManager;
class ResourceCoordinator;

/**
 * EntityDescriptorManager - Entity-specific descriptor set management (SRP)
 * 
 * Single Responsibility: Manage descriptor sets specifically for entity rendering pipeline
 * - Compute descriptors: Entity SoA buffers (velocity, movement params, runtime state, positions, colors, model matrices)
 * - Graphics descriptors: Entity rendering data (positions, movement params from ResourceCoordinator uniform buffer)
 * - Swapchain recreation support with descriptor set recreation
 * 
 * Uses composition with DescriptorSetManagerBase for shared functionality (DRY)
 * Uses DescriptorUpdateHelper for update operations (DRY)
 */
class EntityDescriptorManager : public DescriptorSetManagerBase {
public:
    EntityDescriptorManager();
    ~EntityDescriptorManager() override = default;

    // Entity-specific initialization
    bool initializeEntity(EntityBufferManager& bufferManager, ResourceCoordinator* resourceCoordinator = nullptr);

    // Descriptor set layout management (entity-specific responsibility)
    bool createDescriptorSetLayouts();
    VkDescriptorSetLayout getComputeDescriptorSetLayout() const { return computeDescriptorSetLayout; }
    VkDescriptorSetLayout getGraphicsDescriptorSetLayout() const { return graphicsDescriptorSetLayout; }

    // Descriptor set management
    bool createComputeDescriptorSets(VkDescriptorSetLayout layout);
    bool createGraphicsDescriptorSets(VkDescriptorSetLayout layout);
    
    // Descriptor set access
    VkDescriptorSet getComputeDescriptorSet() const { return computeDescriptorSet; }
    VkDescriptorSet getGraphicsDescriptorSet() const { return graphicsDescriptorSet; }

    // State queries (entity-specific)
    bool hasValidComputeDescriptorSet() const { return computeDescriptorSet != VK_NULL_HANDLE; }
    bool hasValidGraphicsDescriptorSet() const { return graphicsDescriptorSet != VK_NULL_HANDLE; }

    // Override from base class
    bool recreateDescriptorSets() override;
    
    // Vulkan 1.3 descriptor indexing support
    bool createIndexedDescriptorSetLayout();
    bool createIndexedDescriptorSet();
    bool updateIndexedDescriptorSet();
    VkDescriptorSetLayout getIndexedDescriptorSetLayout() const { return indexedDescriptorSetLayout; }
    VkDescriptorSet getIndexedDescriptorSet() const { return indexedDescriptorSet; }
    bool hasValidIndexedDescriptorSet() const { return indexedDescriptorSet != VK_NULL_HANDLE; }

protected:
    // Override base class template methods
    bool initializeSpecialized() override;
    void cleanupSpecialized() override;

private:
    // Entity-specific dependencies
    EntityBufferManager* bufferManager = nullptr;
    ResourceCoordinator* resourceCoordinator = nullptr;
    
    // Entity-specific descriptor set layouts
    VkDescriptorSetLayout computeDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout graphicsDescriptorSetLayout = VK_NULL_HANDLE;
    
    // Vulkan 1.3 descriptor indexing layout and set
    VkDescriptorSetLayout indexedDescriptorSetLayout = VK_NULL_HANDLE;
    vulkan_raii::DescriptorPool indexedDescriptorPool;
    VkDescriptorSet indexedDescriptorSet = VK_NULL_HANDLE;
    
    // Entity-specific descriptor pools (managed via composition)
    vulkan_raii::DescriptorPool computeDescriptorPool;
    vulkan_raii::DescriptorPool graphicsDescriptorPool;
    
    // Entity-specific descriptor sets
    VkDescriptorSet computeDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet graphicsDescriptorSet = VK_NULL_HANDLE;
    
    // Entity-specific helpers (SRP)
    bool createComputeDescriptorPool();
    bool createGraphicsDescriptorPool();
    bool updateComputeDescriptorSet();
    bool updateGraphicsDescriptorSet();
    bool recreateComputeDescriptorSets();
    bool recreateGraphicsDescriptorSets();
    
    // Entity-specific cleanup
    void cleanupDescriptorSetLayouts();
};
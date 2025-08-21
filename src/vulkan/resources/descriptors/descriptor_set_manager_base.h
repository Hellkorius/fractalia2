#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

class VulkanContext;
class DescriptorPoolManager;

/**
 * DescriptorSetManagerBase - Common interface and shared functionality (DRY principle)
 * 
 * Single Responsibility: Provide common descriptor set management patterns
 * - Standard lifecycle management (initialize, cleanup, recreate)
 * - Common state validation
 * - Shared pool management via composition
 * - Interface for specialized managers to implement
 */
class DescriptorSetManagerBase {
public:
    DescriptorSetManagerBase();
    virtual ~DescriptorSetManagerBase();

    // Common lifecycle
    virtual bool initialize(const VulkanContext& context);
    virtual void cleanup();
    virtual bool recreateDescriptorSets() = 0;

    // State queries
    bool isInitialized() const { return context != nullptr; }
    const VulkanContext* getContext() const { return context; }

protected:
    // Shared infrastructure
    const VulkanContext* context = nullptr;
    std::unique_ptr<DescriptorPoolManager> poolManager;

    // Template method pattern for specialized initialization
    virtual bool initializeSpecialized() = 0;
    virtual void cleanupSpecialized() = 0;

    // Shared validation
    bool validateContext() const;
    bool validatePool(VkDescriptorPool pool) const;
    bool validateDescriptorSet(VkDescriptorSet descriptorSet) const;

    // Pool management helpers
    bool createPoolManager();
    DescriptorPoolManager& getPoolManager() const { return *poolManager; }

private:
    // Non-copyable
    DescriptorSetManagerBase(const DescriptorSetManagerBase&) = delete;
    DescriptorSetManagerBase& operator=(const DescriptorSetManagerBase&) = delete;
};
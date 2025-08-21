#include "descriptor_set_manager_base.h"
#include "../managers/descriptor_pool_manager.h"
#include "../../core/vulkan_context.h"
#include <iostream>

DescriptorSetManagerBase::DescriptorSetManagerBase() {
}

DescriptorSetManagerBase::~DescriptorSetManagerBase() {
    cleanup();
}

bool DescriptorSetManagerBase::initialize(const VulkanContext& context) {
    this->context = &context;
    
    if (!createPoolManager()) {
        std::cerr << "DescriptorSetManagerBase: Failed to create pool manager" << std::endl;
        return false;
    }
    
    // Delegate to specialized initialization
    if (!initializeSpecialized()) {
        std::cerr << "DescriptorSetManagerBase: Specialized initialization failed" << std::endl;
        cleanup();
        return false;
    }
    
    return true;
}

void DescriptorSetManagerBase::cleanup() {
    if (!context) return;
    
    // Cleanup specialized resources first
    cleanupSpecialized();
    
    // Cleanup shared resources
    poolManager.reset();
    context = nullptr;
}

bool DescriptorSetManagerBase::createPoolManager() {
    if (!context) {
        std::cerr << "DescriptorSetManagerBase: Cannot create pool manager - no context" << std::endl;
        return false;
    }
    
    poolManager = std::make_unique<DescriptorPoolManager>();
    if (!poolManager->initialize(*context)) {
        std::cerr << "DescriptorSetManagerBase: Failed to initialize pool manager" << std::endl;
        poolManager.reset();
        return false;
    }
    
    return true;
}

bool DescriptorSetManagerBase::validateContext() const {
    if (!context) {
        std::cerr << "DescriptorSetManagerBase: Context not initialized" << std::endl;
        return false;
    }
    return true;
}

bool DescriptorSetManagerBase::validatePool(VkDescriptorPool pool) const {
    if (pool == VK_NULL_HANDLE) {
        std::cerr << "DescriptorSetManagerBase: Descriptor pool is VK_NULL_HANDLE" << std::endl;
        return false;
    }
    return true;
}

bool DescriptorSetManagerBase::validateDescriptorSet(VkDescriptorSet descriptorSet) const {
    if (descriptorSet == VK_NULL_HANDLE) {
        std::cerr << "DescriptorSetManagerBase: Descriptor set is VK_NULL_HANDLE" << std::endl;
        return false;
    }
    return true;
}
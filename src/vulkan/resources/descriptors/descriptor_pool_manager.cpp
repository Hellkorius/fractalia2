#include "descriptor_pool_manager.h"
#include "../../core/vulkan_context.h"
#include "../../core/vulkan_function_loader.h"
#include <vector>
#include <iostream>

DescriptorPoolManager::DescriptorPoolManager() {
}

DescriptorPoolManager::~DescriptorPoolManager() {
    cleanup();
}

bool DescriptorPoolManager::initialize(const VulkanContext& context) {
    this->context = &context;
    return true;
}

void DescriptorPoolManager::cleanup() {
    context = nullptr;
}

vulkan_raii::DescriptorPool DescriptorPoolManager::createDescriptorPool() {
    return createDescriptorPool(DescriptorPoolConfig{});
}

vulkan_raii::DescriptorPool DescriptorPoolManager::createDescriptorPool(const DescriptorPoolConfig& config) {
    std::vector<VkDescriptorPoolSize> poolSizes;
    
    if (config.uniformBuffers > 0) {
        poolSizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, config.uniformBuffers});
    }
    if (config.storageBuffers > 0) {
        poolSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, config.storageBuffers});
    }
    if (config.sampledImages > 0) {
        poolSizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, config.sampledImages});
    }
    if (config.storageImages > 0) {
        poolSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, config.storageImages});
    }
    if (config.samplers > 0) {
        poolSizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, config.samplers});
    }
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = config.allowFreeDescriptorSets ? VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT : 0;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = config.maxSets;
    
    return vulkan_raii::create_descriptor_pool(context, &poolInfo);
}

void DescriptorPoolManager::destroyDescriptorPool(VkDescriptorPool pool) {
    if (context && pool != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyDescriptorPool(context->getDevice(), pool, nullptr);
    }
}
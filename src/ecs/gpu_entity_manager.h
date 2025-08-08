#pragma once

#include "component.hpp"
#include "entity.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <queue>

// GPU entity structure matching compute shader layout
struct GPUEntity {
    glm::mat4 modelMatrix;        // 64 bytes - transform matrix
    glm::vec4 color;              // 16 bytes - RGBA color  
    glm::vec4 movementParams0;    // 16 bytes - amplitude, frequency, phase, timeOffset
    glm::vec4 movementParams1;    // 16 bytes - center.xyz, movementType
    glm::vec4 runtimeState;       // 16 bytes - totalTime, initialized, reserved, reserved
    
    GPUEntity() = default;
    
    // Create from ECS components
    static GPUEntity fromECS(const Transform& transform, const Renderable& renderable, const MovementPattern& pattern) {
        GPUEntity entity{};
        
        // Copy transform matrix
        entity.modelMatrix = transform.getMatrix();
        
        // Copy color
        entity.color = renderable.color;
        
        // Copy movement parameters
        entity.movementParams0 = glm::vec4(
            pattern.amplitude,
            pattern.frequency, 
            pattern.phase,
            pattern.timeOffset
        );
        
        entity.movementParams1 = glm::vec4(
            pattern.center.x,
            pattern.center.y, 
            pattern.center.z,
            static_cast<float>(pattern.type)
        );
        
        // Initialize runtime state
        entity.runtimeState = glm::vec4(
            pattern.totalTime,
            pattern.initialized ? 1.0f : 0.0f,
            0.0f, // reserved
            0.0f  // reserved
        );
        
        return entity;
    }
};

// Forward declaration
class VulkanContext;
class VulkanSync;

// Manages GPU entity storage and CPU->GPU handover
class GPUEntityManager {
public:
    GPUEntityManager();
    ~GPUEntityManager();
    
    bool initialize(VulkanContext* context, VulkanSync* sync);
    void cleanup();
    
    // Entity management
    void addEntity(const GPUEntity& entity);
    void addEntitiesFromECS(const std::vector<Entity>& entities);
    void uploadPendingEntities();
    void clearAllEntities();
    
    // GPU buffer management  
    VkBuffer getCurrentEntityBuffer() const { return entityBuffers[currentBufferIndex]; }
    VkBuffer getNextEntityBuffer() const { return entityBuffers[1 - currentBufferIndex]; }
    void swapBuffers() { currentBufferIndex = 1 - currentBufferIndex; }
    
    // Getters
    uint32_t getEntityCount() const { return activeEntityCount; }
    uint32_t getMaxEntities() const { return MAX_ENTITIES; }
    bool hasPendingUploads() const { return !pendingEntities.empty(); }
    
    // Descriptor set management
    VkDescriptorSetLayout getComputeDescriptorSetLayout() const { return computeDescriptorSetLayout; }
    VkDescriptorSet getCurrentComputeDescriptorSet() const { return computeDescriptorSets[currentBufferIndex]; }
    
    bool createComputeDescriptorSets();

private:
    static constexpr uint32_t MAX_ENTITIES = 131072; // 128k entities max (16MB / 128 bytes)
    static constexpr size_t ENTITY_BUFFER_SIZE = MAX_ENTITIES * sizeof(GPUEntity);
    
    VulkanContext* context = nullptr;
    VulkanSync* sync = nullptr;
    
    // Double-buffered storage for compute pipeline
    VkBuffer entityBuffers[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkDeviceMemory entityBufferMemory[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE}; 
    void* entityBufferMapped[2] = {nullptr, nullptr};
    uint32_t currentBufferIndex = 0;
    
    // Entity state
    uint32_t activeEntityCount = 0;
    std::vector<GPUEntity> pendingEntities;
    
    // Compute descriptor sets for storage buffers
    VkDescriptorPool computeDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout computeDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet computeDescriptorSets[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    
    // Function pointers
    PFN_vkCreateBuffer vkCreateBuffer = nullptr;
    PFN_vkDestroyBuffer vkDestroyBuffer = nullptr;
    PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements = nullptr;
    PFN_vkBindBufferMemory vkBindBufferMemory = nullptr;
    PFN_vkAllocateMemory vkAllocateMemory = nullptr;
    PFN_vkFreeMemory vkFreeMemory = nullptr;
    PFN_vkMapMemory vkMapMemory = nullptr;
    PFN_vkUnmapMemory vkUnmapMemory = nullptr;
    PFN_vkCreateDescriptorPool vkCreateDescriptorPool = nullptr;
    PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool = nullptr;
    PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout = nullptr;
    PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout = nullptr;
    PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets = nullptr;
    PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets = nullptr;
    PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties = nullptr;
    
    void loadFunctions();
    bool createEntityBuffers();
    bool createComputeDescriptorPool();
    bool createComputeDescriptorSetLayout();
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};
#pragma once

#include "entity.h"
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.h>
#include <cstdint>

// Forward declarations
class VulkanContext;
class VulkanSync;
class ResourceContext;

// Collision event data structure
struct CollisionEvent {
    uint32_t entityA;          // First entity ID
    uint32_t entityB;          // Second entity ID
    glm::vec3 contactPoint;    // World space contact point
    glm::vec3 normal;          // Collision normal (from A to B)
    float penetrationDepth;    // How much entities overlap
    uint32_t frameNumber;      // Frame when collision occurred
    
    CollisionEvent() = default;
    CollisionEvent(uint32_t a, uint32_t b, const glm::vec3& contact, const glm::vec3& norm, float depth, uint32_t frame)
        : entityA(a), entityB(b), contactPoint(contact), normal(norm), penetrationDepth(depth), frameNumber(frame) {}
};

// Manages collision events from GPU and provides interface to ECS
class CollisionEventManager {
public:
    CollisionEventManager();
    ~CollisionEventManager();
    
    // Initialization and cleanup
    bool initialize(const VulkanContext& context, VulkanSync* sync, ResourceContext* resourceContext);
    void cleanup();
    
    // Frame management
    void beginFrame(uint32_t frameNumber);
    void endFrame();
    
    // GPU data transfer
    bool transferFromGPU();
    
    // Event access
    const std::vector<CollisionEvent>& getFrameEvents() const { return frameEvents; }
    void clearFrameEvents();
    
    // ECS integration
    void processEvents(flecs::world& world);
    
    // Statistics
    uint32_t getCollisionCount() const { return static_cast<uint32_t>(frameEvents.size()); }
    uint32_t getMaxCollisionsPerFrame() const { return maxCollisionsPerFrame; }
    bool hasOverflow() const { return overflowDetected; }
    
    // Buffer management
    VkBuffer getCollisionBuffer() const;
    uint32_t getMaxCollisions() const { return MAX_COLLISIONS_PER_FRAME; }

private:
    static constexpr uint32_t MAX_COLLISIONS_PER_FRAME = 8192; // Maximum collisions to process per frame
    static constexpr size_t COLLISION_BUFFER_SIZE = MAX_COLLISIONS_PER_FRAME * sizeof(CollisionEvent);
    
    // Vulkan resources
    const VulkanContext* context = nullptr;
    VulkanSync* sync = nullptr;
    ResourceContext* resourceContext = nullptr;
    
    // GPU buffer for collision data
    VkBuffer collisionBuffer = VK_NULL_HANDLE;
    VkDeviceMemory collisionBufferMemory = VK_NULL_HANDLE;
    void* collisionBufferMapped = nullptr;
    
    // Staging buffer for GPU->CPU transfer
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
    void* stagingBufferMapped = nullptr;
    
    // Event storage
    std::vector<CollisionEvent> frameEvents;     // Events for current frame
    std::vector<CollisionEvent> stagingEvents;   // Events being transferred from GPU
    
    // Frame state
    uint32_t currentFrame = 0;
    bool frameActive = false;
    bool transferPending = false;
    
    // Statistics
    uint32_t maxCollisionsPerFrame = 0;
    uint32_t totalCollisionsProcessed = 0;
    bool overflowDetected = false;
    
    // Internal methods
    bool createBuffers();
    bool mapBuffers();
    void unmapBuffers();
    void destroyBuffers();
    
    // Data processing
    void processRawCollisionData();
    void validateCollisionData();
};
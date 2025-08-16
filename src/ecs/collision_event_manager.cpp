#include "collision_event_manager.h"
#include "../vulkan/vulkan_context.h"
#include "../vulkan/vulkan_sync.h"
#include "../vulkan/vulkan_function_loader.h"
#include "../vulkan/vulkan_utils.h"
#include "../vulkan/resource_context.h"
#include "component.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <cassert>

CollisionEventManager::CollisionEventManager() {
    frameEvents.reserve(MAX_COLLISIONS_PER_FRAME);
    stagingEvents.reserve(MAX_COLLISIONS_PER_FRAME);
}

CollisionEventManager::~CollisionEventManager() {
    cleanup();
}

bool CollisionEventManager::initialize(const VulkanContext& context, VulkanSync* sync, ResourceContext* resourceContext) {
    this->context = &context;
    this->sync = sync;
    this->resourceContext = resourceContext;
    
    if (!createBuffers()) {
        std::cerr << "Failed to create collision buffers!" << std::endl;
        return false;
    }
    
    if (!mapBuffers()) {
        std::cerr << "Failed to map collision buffers!" << std::endl;
        return false;
    }
    
    return true;
}

void CollisionEventManager::cleanup() {
    if (!context) return;
    
    unmapBuffers();
    destroyBuffers();
    
    context = nullptr;
    sync = nullptr;
    resourceContext = nullptr;
}

void CollisionEventManager::beginFrame(uint32_t frameNumber) {
    currentFrame = frameNumber;
    frameActive = true;
    overflowDetected = false;
    
    // Clear previous frame events
    frameEvents.clear();
}

void CollisionEventManager::endFrame() {
    frameActive = false;
    
    // Update statistics
    uint32_t collisionCount = static_cast<uint32_t>(frameEvents.size());
    maxCollisionsPerFrame = std::max(maxCollisionsPerFrame, collisionCount);
    totalCollisionsProcessed += collisionCount;
    
    if (collisionCount >= MAX_COLLISIONS_PER_FRAME) {
        overflowDetected = true;
    }
}

bool CollisionEventManager::transferFromGPU() {
    if (!context || !stagingBuffer || !collisionBuffer) {
        return false;
    }
    
    // For now, implement basic CPU-side collision detection
    // This will be replaced with GPU compute shader output later
    
    stagingEvents.clear();
    
    // TODO: Replace with actual GPU->CPU transfer when GPU collision detection is implemented
    // For Phase 1, we just clear the staging events and return success
    
    return true;
}

void CollisionEventManager::clearFrameEvents() {
    frameEvents.clear();
}

void CollisionEventManager::processEvents(flecs::world& world) {
    // Process collision events and apply to ECS entities
    for (const auto& event : frameEvents) {
        // TODO: Add collision response logic here
        // This could include:
        // - Applying impulses
        // - Triggering damage
        // - Playing sound effects
        // - Spawning particle effects
        // - Destroying entities
        
        // For now, just log the collision for debugging
        #ifdef DEBUG_COLLISIONS
        std::cout << "Collision: Entity " << event.entityA << " vs Entity " << event.entityB 
                  << " at (" << event.contactPoint.x << ", " << event.contactPoint.y 
                  << ") depth=" << event.penetrationDepth << std::endl;
        #endif
        
        // Silence unused variable warning for now
        (void)event;
        (void)world;
    }
}

VkBuffer CollisionEventManager::getCollisionBuffer() const {
    return collisionBuffer;
}

bool CollisionEventManager::createBuffers() {
    if (!context || !resourceContext) {
        return false;
    }
    
    auto& loader = context->getLoader();
    VkDevice device = context->getDevice();
    
    // Create GPU buffer for collision data (device-local) using VulkanUtils
    if (!VulkanUtils::createBuffer(
            device, loader, COLLISION_BUFFER_SIZE,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            collisionBuffer, collisionBufferMemory)) {
        std::cerr << "Failed to create collision buffer!" << std::endl;
        return false;
    }
    
    // Create staging buffer for CPU access (host-visible) using VulkanUtils
    if (!VulkanUtils::createBuffer(
            device, loader, COLLISION_BUFFER_SIZE,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer, stagingBufferMemory)) {
        std::cerr << "Failed to create collision staging buffer!" << std::endl;
        return false;
    }
    
    return true;
}

bool CollisionEventManager::mapBuffers() {
    if (!context || !stagingBuffer) {
        return false;
    }
    
    auto& loader = context->getLoader();
    VkDevice device = context->getDevice();
    
    // Map staging buffer for CPU access
    if (loader.vkMapMemory(device, stagingBufferMemory, 0, COLLISION_BUFFER_SIZE, 0, &stagingBufferMapped) != VK_SUCCESS) {
        std::cerr << "Failed to map collision staging buffer!" << std::endl;
        return false;
    }
    
    return true;
}

void CollisionEventManager::unmapBuffers() {
    if (!context) return;
    
    auto& loader = context->getLoader();
    VkDevice device = context->getDevice();
    
    if (stagingBufferMapped) {
        loader.vkUnmapMemory(device, stagingBufferMemory);
        stagingBufferMapped = nullptr;
    }
}

void CollisionEventManager::destroyBuffers() {
    if (!context) return;
    
    auto& loader = context->getLoader();
    VkDevice device = context->getDevice();
    
    // Destroy collision buffer
    if (collisionBuffer != VK_NULL_HANDLE) {
        loader.vkDestroyBuffer(device, collisionBuffer, nullptr);
        collisionBuffer = VK_NULL_HANDLE;
    }
    
    if (collisionBufferMemory != VK_NULL_HANDLE) {
        loader.vkFreeMemory(device, collisionBufferMemory, nullptr);
        collisionBufferMemory = VK_NULL_HANDLE;
    }
    
    // Destroy staging buffer
    if (stagingBuffer != VK_NULL_HANDLE) {
        loader.vkDestroyBuffer(device, stagingBuffer, nullptr);
        stagingBuffer = VK_NULL_HANDLE;
    }
    
    if (stagingBufferMemory != VK_NULL_HANDLE) {
        loader.vkFreeMemory(device, stagingBufferMemory, nullptr);
        stagingBufferMemory = VK_NULL_HANDLE;
    }
}

void CollisionEventManager::processRawCollisionData() {
    // Process raw collision data from GPU buffer
    if (!stagingBufferMapped) return;
    
    CollisionEvent* events = static_cast<CollisionEvent*>(stagingBufferMapped);
    uint32_t eventCount = 0;
    
    // Read event count from first 4 bytes (convention for GPU output)
    std::memcpy(&eventCount, stagingBufferMapped, sizeof(uint32_t));
    eventCount = std::min(eventCount, MAX_COLLISIONS_PER_FRAME);
    
    // Copy events to staging vector
    stagingEvents.clear();
    stagingEvents.reserve(eventCount);
    
    for (uint32_t i = 0; i < eventCount; ++i) {
        CollisionEvent& event = events[i + 1]; // Skip count header
        event.frameNumber = currentFrame;
        stagingEvents.push_back(event);
    }
}

void CollisionEventManager::validateCollisionData() {
    // Validate collision events for consistency
    for (auto& event : stagingEvents) {
        // Ensure entity IDs are valid (non-zero)
        if (event.entityA == 0 || event.entityB == 0) {
            continue; // Skip invalid events
        }
        
        // Ensure entities are different
        if (event.entityA == event.entityB) {
            continue; // Skip self-collision
        }
        
        // Clamp penetration depth to reasonable range
        event.penetrationDepth = std::max(0.0f, std::min(event.penetrationDepth, 10.0f));
        
        // Normalize collision normal
        float normalLength = glm::length(event.normal);
        if (normalLength > 0.001f) {
            event.normal /= normalLength;
        } else {
            event.normal = glm::vec3(1.0f, 0.0f, 0.0f); // Default normal
        }
        
        // Add to frame events
        frameEvents.push_back(event);
    }
}
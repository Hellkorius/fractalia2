#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "vulkan_raii.h"
#include "vulkan_constants.h"
#include <vector>
#include <memory>
#include <unordered_map>

// Forward declarations
class VulkanContext;

/**
 * @brief Specialized command pool configurations for different queue types
 */
enum class CommandPoolType {
    Graphics,   // Persistent command buffers with reset capability
    Compute,    // Transient command buffers for short-lived dispatches  
    Transfer    // One-time command buffers for async transfers
};

/**
 * @brief Centralized queue and command buffer management system
 * 
 * Provides clean abstraction over Vulkan queue families with automatic fallback,
 * specialized command pool management, and optimal command buffer allocation.
 */
class QueueManager {
public:
    QueueManager();
    ~QueueManager();
    
    bool initialize(const VulkanContext& context);
    void cleanup();
    void cleanupBeforeContextDestruction();
    
    // Queue access with automatic fallback
    VkQueue getGraphicsQueue() const;
    VkQueue getComputeQueue() const;
    VkQueue getTransferQueue() const;
    VkQueue getPresentQueue() const;
    
    // Queue family indices
    uint32_t getGraphicsQueueFamily() const;
    uint32_t getComputeQueueFamily() const;
    uint32_t getTransferQueueFamily() const;
    uint32_t getPresentQueueFamily() const;
    
    // Queue capability queries
    bool hasDedicatedComputeQueue() const;
    bool hasDedicatedTransferQueue() const;
    bool supportsAsyncCompute() const;
    
    // Specialized command pool access
    VkCommandPool getCommandPool(CommandPoolType type) const;
    
    // Frame-based command buffer management (graphics/compute)
    VkCommandBuffer getGraphicsCommandBuffer(uint32_t frameIndex) const;
    VkCommandBuffer getComputeCommandBuffer(uint32_t frameIndex) const;
    
    // One-time command buffer allocation (transfer)
    struct TransferCommand {
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        vulkan_raii::Fence fence;
        VkCommandPool sourcePool = VK_NULL_HANDLE;
        
        bool isValid() const { return commandBuffer != VK_NULL_HANDLE && fence; }
    };
    
    TransferCommand allocateTransferCommand();
    void freeTransferCommand(TransferCommand& command);
    bool isTransferComplete(const TransferCommand& command) const;
    void waitForTransfer(const TransferCommand& command) const;
    
    // Command buffer lifecycle management
    void resetCommandBuffersForFrame(uint32_t frameIndex);
    void resetAllCommandBuffers();
    
    // Queue utilization telemetry
    struct QueueTelemetry {
        uint64_t graphicsSubmissions = 0;
        uint64_t computeSubmissions = 0;
        uint64_t transferSubmissions = 0;
        uint64_t presentSubmissions = 0;
        
        // Command buffer allocation tracking
        uint32_t activeTransferCommands = 0;
        uint32_t peakTransferCommands = 0;
        uint32_t totalTransferAllocations = 0;
        
        void recordSubmission(CommandPoolType type) {
            switch (type) {
                case CommandPoolType::Graphics: ++graphicsSubmissions; break;
                case CommandPoolType::Compute: ++computeSubmissions; break;
                case CommandPoolType::Transfer: ++transferSubmissions; break;
            }
        }
        
        void recordTransferAllocation() {
            ++activeTransferCommands;
            ++totalTransferAllocations;
            if (activeTransferCommands > peakTransferCommands) {
                peakTransferCommands = activeTransferCommands;
            }
        }
        
        void recordTransferDeallocation() {
            if (activeTransferCommands > 0) --activeTransferCommands;
        }
    };
    
    const QueueTelemetry& getTelemetry() const { return telemetry; }
    QueueTelemetry& getTelemetry() { return telemetry; }
    void logTelemetry() const;
    
private:
    const VulkanContext* context = nullptr;
    
    // Specialized command pools
    vulkan_raii::CommandPool graphicsCommandPool;
    vulkan_raii::CommandPool computeCommandPool;
    vulkan_raii::CommandPool transferCommandPool;
    
    // Frame-based command buffers
    std::vector<VkCommandBuffer> graphicsCommandBuffers;
    std::vector<VkCommandBuffer> computeCommandBuffers;
    
    // Telemetry tracking
    mutable QueueTelemetry telemetry;
    
    // Internal command pool creation
    bool createCommandPools();
    bool createFrameCommandBuffers();
    
    // Helper methods
    VkCommandPoolCreateFlags getCommandPoolFlags(CommandPoolType type) const;
    uint32_t getQueueFamilyForPool(CommandPoolType type) const;
};
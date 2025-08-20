#include "frame_graph.h"
#include "../core/vulkan_context.h"
#include "../core/vulkan_sync.h"
#include "../core/vulkan_utils.h"
#include "../core/queue_manager.h"
#include "../nodes/entity_compute_node.h"
#include "../nodes/physics_compute_node.h"
#include "../nodes/entity_graphics_node.h"
#include "../monitoring/gpu_memory_monitor.h"
#include "../monitoring/gpu_timeout_detector.h"
#include <iostream>
#include <queue>
#include <algorithm>
#include <cassert>
#include <functional>
#include <unordered_set>
#include <thread>
#include <chrono>

FrameGraph::FrameGraph() {
}

FrameGraph::~FrameGraph() {
    cleanup();
}

bool FrameGraph::initialize(const VulkanContext& context, VulkanSync* sync, QueueManager* queueManager) {
    this->context = &context;
    this->sync = sync;
    this->queueManager = queueManager;
    
    if (context.getDevice() == VK_NULL_HANDLE || !sync || !queueManager) {
        std::cerr << "FrameGraph: Invalid context, sync, or queue manager objects" << std::endl;
        return false;
    }
    
    initialized = true;
    std::cout << "FrameGraph initialized successfully with QueueManager" << std::endl;
    return true;
}

void FrameGraph::cleanup() {
    if (!initialized) return;
    
    cleanupBeforeContextDestruction();
    
    nodes.clear();
    resources.clear();
    resourceNameMap.clear();
    resourceCleanupInfo.clear();
    executionOrder.clear();
    barrierBatches.clear();
    resourceWriteTracking.clear();
    
    nextResourceId = 1;
    nextNodeId = 1;
    compiled = false;
    initialized = false;
}

void FrameGraph::cleanupBeforeContextDestruction() {
    // Clear all RAII resources before context destruction
    for (auto& [id, resource] : resources) {
        std::visit([](auto& res) {
            if (!res.isExternal) {
                if constexpr (std::is_same_v<std::decay_t<decltype(res)>, FrameGraphBuffer>) {
                    res.buffer.reset();
                    res.memory.reset();
                } else {
                    res.view.reset();
                    res.image.reset();
                    res.memory.reset();
                }
            }
        }, resource);
    }
}

FrameGraphTypes::ResourceId FrameGraph::createBuffer(const std::string& name, VkDeviceSize size, VkBufferUsageFlags usage) {
    if (!initialized) {
        std::cerr << "FrameGraph: Cannot create buffer, not initialized" << std::endl;
        return FrameGraphTypes::INVALID_RESOURCE;
    }
    
    // Check for duplicate names
    if (resourceNameMap.find(name) != resourceNameMap.end()) {
        std::cerr << "FrameGraph: Buffer with name '" << name << "' already exists" << std::endl;
        return FrameGraphTypes::INVALID_RESOURCE;
    }
    
    FrameGraphTypes::ResourceId id = nextResourceId++;
    
    FrameGraphBuffer buffer;
    buffer.size = size;
    buffer.usage = usage;
    buffer.isExternal = false;
    buffer.debugName = name;
    
    // Create Vulkan buffer
    if (!createVulkanBuffer(buffer)) {
        std::cerr << "FrameGraph: Failed to create Vulkan buffer for '" << name << "'" << std::endl;
        return FrameGraphTypes::INVALID_RESOURCE;
    }
    
    resources[id] = std::move(buffer);
    resourceNameMap[name] = id;
    
    // Initialize resource cleanup tracking
    ResourceCleanupInfo cleanupInfo;
    cleanupInfo.lastAccessTime = std::chrono::steady_clock::now();
    cleanupInfo.criticality = classifyResource(std::get<FrameGraphBuffer>(resources[id]));
    cleanupInfo.canEvict = !std::get<FrameGraphBuffer>(resources[id]).isExternal;
    resourceCleanupInfo[id] = cleanupInfo;
    
    std::cout << "FrameGraph: Created buffer '" << name << "' (ID: " << id << ", Size: " << size << ")" << std::endl;
    return id;
}

FrameGraphTypes::ResourceId FrameGraph::createImage(const std::string& name, VkFormat format, VkExtent2D extent, VkImageUsageFlags usage) {
    if (!initialized) {
        std::cerr << "FrameGraph: Cannot create image, not initialized" << std::endl;
        return FrameGraphTypes::INVALID_RESOURCE;
    }
    
    // Check for duplicate names
    if (resourceNameMap.find(name) != resourceNameMap.end()) {
        std::cerr << "FrameGraph: Image with name '" << name << "' already exists" << std::endl;
        return FrameGraphTypes::INVALID_RESOURCE;
    }
    
    FrameGraphTypes::ResourceId id = nextResourceId++;
    
    FrameGraphImage image;
    image.format = format;
    image.extent = extent;
    image.usage = usage;
    image.isExternal = false;
    image.debugName = name;
    
    // Create Vulkan image
    if (!createVulkanImage(image)) {
        std::cerr << "FrameGraph: Failed to create Vulkan image for '" << name << "'" << std::endl;
        return FrameGraphTypes::INVALID_RESOURCE;
    }
    
    resources[id] = std::move(image);
    resourceNameMap[name] = id;
    
    // Initialize resource cleanup tracking
    ResourceCleanupInfo cleanupInfo;
    cleanupInfo.lastAccessTime = std::chrono::steady_clock::now();
    cleanupInfo.criticality = classifyResource(std::get<FrameGraphImage>(resources[id]));
    cleanupInfo.canEvict = !std::get<FrameGraphImage>(resources[id]).isExternal;
    resourceCleanupInfo[id] = cleanupInfo;
    
    std::cout << "FrameGraph: Created image '" << name << "' (ID: " << id << ")" << std::endl;
    return id;
}

FrameGraphTypes::ResourceId FrameGraph::importExternalBuffer(const std::string& name, VkBuffer buffer, VkDeviceSize size, VkBufferUsageFlags usage) {
    if (!initialized) {
        std::cerr << "FrameGraph: Cannot import buffer, not initialized" << std::endl;
        return FrameGraphTypes::INVALID_RESOURCE;
    }
    
    // Check for duplicate names
    if (resourceNameMap.find(name) != resourceNameMap.end()) {
        std::cerr << "FrameGraph: Resource with name '" << name << "' already exists" << std::endl;
        return FrameGraphTypes::INVALID_RESOURCE;
    }
    
    FrameGraphTypes::ResourceId id = nextResourceId++;
    
    FrameGraphBuffer frameGraphBuffer;
    frameGraphBuffer.buffer = vulkan_raii::Buffer(buffer, context);
    frameGraphBuffer.buffer.detach(); // Don't manage lifecycle for external buffers
    frameGraphBuffer.size = size;
    frameGraphBuffer.usage = usage;
    frameGraphBuffer.isExternal = true; // Don't manage lifecycle
    frameGraphBuffer.debugName = name;
    
    resources[id] = std::move(frameGraphBuffer);
    resourceNameMap[name] = id;
    
    // Initialize resource cleanup tracking for external buffer
    ResourceCleanupInfo cleanupInfo;
    cleanupInfo.lastAccessTime = std::chrono::steady_clock::now();
    cleanupInfo.criticality = classifyResource(std::get<FrameGraphBuffer>(resources[id]));
    cleanupInfo.canEvict = false; // External buffers cannot be evicted
    resourceCleanupInfo[id] = cleanupInfo;
    
    std::cout << "FrameGraph: Imported external buffer '" << name << "' (ID: " << id << ")" << std::endl;
    return id;
}

FrameGraphTypes::ResourceId FrameGraph::importExternalImage(const std::string& name, VkImage image, VkImageView view, VkFormat format, VkExtent2D extent) {
    if (!initialized) {
        std::cerr << "FrameGraph: Cannot import image, not initialized" << std::endl;
        return FrameGraphTypes::INVALID_RESOURCE;
    }
    
    // Check for duplicate names
    if (resourceNameMap.find(name) != resourceNameMap.end()) {
        std::cerr << "FrameGraph: Resource with name '" << name << "' already exists" << std::endl;
        return FrameGraphTypes::INVALID_RESOURCE;
    }
    
    FrameGraphTypes::ResourceId id = nextResourceId++;
    
    FrameGraphImage frameGraphImage;
    frameGraphImage.image = vulkan_raii::Image(image, context);
    frameGraphImage.image.detach(); // Don't manage lifecycle for external images
    frameGraphImage.view = vulkan_raii::ImageView(view, context);
    frameGraphImage.view.detach(); // Don't manage lifecycle for external image views
    frameGraphImage.format = format;
    frameGraphImage.extent = extent;
    frameGraphImage.isExternal = true; // Don't manage lifecycle
    frameGraphImage.debugName = name;
    
    resources[id] = std::move(frameGraphImage);
    resourceNameMap[name] = id;
    
    // Initialize resource cleanup tracking for external image
    ResourceCleanupInfo cleanupInfo;
    cleanupInfo.lastAccessTime = std::chrono::steady_clock::now();
    cleanupInfo.criticality = classifyResource(std::get<FrameGraphImage>(resources[id]));
    cleanupInfo.canEvict = false; // External images cannot be evicted
    resourceCleanupInfo[id] = cleanupInfo;
    
    return id;
}

bool FrameGraph::compile() {
    if (!initialized) {
        std::cerr << "FrameGraph: Cannot compile, not initialized" << std::endl;
        return false;
    }
    
    // Compiling frame graph
    static int compileCount = 0;
    if (compileCount++ < 5) {  // Only log first 5 compilations
        std::cout << "FrameGraph compilation #" << compileCount << std::endl;
    }
    
    // Backup current state for transactional compilation
    backupCompilationState();
    
    // Clear current compilation state
    executionOrder.clear();
    barrierBatches.clear();
    resourceWriteTracking.clear();
    compiled = false;
    
    // Build dependency graph
    if (!buildDependencyGraph()) {
        std::cerr << "FrameGraph: Failed to build dependency graph" << std::endl;
        restoreCompilationState();
        return false;
    }
    
    // Enhanced topological sort with detailed cycle analysis
    CircularDependencyReport cycleReport;
    if (!topologicalSortWithCycleDetection(cycleReport)) {
        std::cerr << "FrameGraph: Compilation failed due to circular dependencies" << std::endl;
        
        // Print detailed cycle analysis
        if (!cycleReport.cycles.empty()) {
            std::cerr << "\nDetailed Cycle Analysis:" << std::endl;
            for (size_t i = 0; i < cycleReport.cycles.size(); i++) {
                const auto& cycle = cycleReport.cycles[i];
                std::cerr << "Cycle " << (i + 1) << ": ";
                
                for (size_t j = 0; j < cycle.nodeChain.size(); j++) {
                    auto nodeIt = nodes.find(cycle.nodeChain[j]);
                    if (nodeIt != nodes.end()) {
                        std::cerr << nodeIt->second->getName();
                        if (j < cycle.resourceChain.size()) {
                            std::cerr << " --[resource " << cycle.resourceChain[j] << "]--> ";
                        }
                    }
                }
                std::cerr << std::endl;
            }
            
            std::cerr << "\nResolution Suggestions:" << std::endl;
            for (const auto& suggestion : cycleReport.resolutionSuggestions) {
                std::cerr << "- " << suggestion << std::endl;
            }
        }
        
        // Attempt partial compilation as fallback
        PartialCompilationResult partialResult = attemptPartialCompilation();
        if (partialResult.hasValidSubgraph) {
            std::cerr << "\nFalling back to partial compilation:" << std::endl;
            std::cerr << "- Executing " << partialResult.validNodes.size() << " valid nodes" << std::endl;
            std::cerr << "- Skipping " << partialResult.problematicNodes.size() << " problematic nodes" << std::endl;
            
            executionOrder = partialResult.validNodes;
            
            // Insert synchronization barriers for valid subgraph
            analyzeBarrierRequirements();
            createOptimalBarrierBatches();
            
            // Setup valid nodes only
            for (auto nodeId : executionOrder) {
                auto it = nodes.find(nodeId);
                if (it != nodes.end()) {
                    it->second->setup(*this);
                }
            }
            
            compiled = true;
            std::cerr << "Partial compilation successful" << std::endl;
            return true;
        }
        
        restoreCompilationState();
        return false;
    }
    
    // Insert synchronization barriers
    analyzeBarrierRequirements();
    createOptimalBarrierBatches();
    
    // Setup nodes
    for (auto nodeId : executionOrder) {
        auto it = nodes.find(nodeId);
        if (it != nodes.end()) {
            it->second->setup(*this);
        }
    }
    
    compiled = true;
    std::cout << "FrameGraph compilation successful (" << executionOrder.size() << " nodes)" << std::endl;
    
    return true;
}

void FrameGraph::updateFrameData(float time, float deltaTime, uint32_t frameCounter, uint32_t currentFrameIndex) {
    // Update frame data for all nodes that support it
    for (auto& [nodeId, node] : nodes) {
        // Check if this is an EntityComputeNode and update it
        auto* computeNode = dynamic_cast<EntityComputeNode*>(node.get());
        if (computeNode) {
            computeNode->updateFrameData(time, deltaTime, frameCounter);
        }
        
        // Check if this is a PhysicsComputeNode and update it
        auto* physicsNode = dynamic_cast<PhysicsComputeNode*>(node.get());
        if (physicsNode) {
            physicsNode->updateFrameData(time, deltaTime, frameCounter);
        }
        
        // Check if this is an EntityGraphicsNode and update it  
        auto* graphicsNode = dynamic_cast<EntityGraphicsNode*>(node.get());
        if (graphicsNode) {
            graphicsNode->updateFrameData(time, deltaTime, currentFrameIndex); // Use currentFrameIndex for buffer sync
        }
    }
}

FrameGraph::ExecutionResult FrameGraph::execute(uint32_t frameIndex) {
    ExecutionResult result;
    if (!compiled) {
        std::cerr << "FrameGraph: Cannot execute, not compiled" << std::endl;
        return result;
    }
    
    if (!sync) {
        std::cerr << "FrameGraph: Cannot execute, no sync object" << std::endl;
        return result;
    }
    
    // Check for memory pressure and perform cleanup if needed
    if (isMemoryPressureCritical()) {
        performResourceCleanup();
        
        // If still critical, attempt resource eviction
        if (isMemoryPressureCritical()) {
            evictNonCriticalResources();
        }
    }
    
    // Note: Command buffer reset is handled by VulkanRenderer after fence waits
    // Frame graph assumes command buffers are already reset and ready for recording
    
    // Analyze which command buffers we'll need
    auto [computeNeeded, graphicsNeeded] = analyzeQueueRequirements();
    result.computeCommandBufferUsed = computeNeeded;
    result.graphicsCommandBufferUsed = graphicsNeeded;
    
    // Begin only the command buffers that will be used
    beginCommandBuffers(computeNeeded, graphicsNeeded, frameIndex);
    
    // Execute nodes with timeout monitoring if available
    bool computeExecuted = false;
    if (timeoutDetector) {
        if (!executeWithTimeoutMonitoring(frameIndex, computeExecuted)) {
            // Timeout occurred, end command buffers and return early
            endCommandBuffers(computeNeeded, graphicsNeeded, frameIndex);
            handleExecutionTimeout();
            return result;
        }
    } else {
        executeNodesInOrder(frameIndex, computeExecuted);
    }
    
    // End only the command buffers that were begun
    endCommandBuffers(computeNeeded, graphicsNeeded, frameIndex);
    
    // Frame graph complete - command buffers are ready for submission by VulkanRenderer
    return result;
}

void FrameGraph::reset() {
    // Reset for next frame - clear transient state but keep persistent resources and nodes
    // Keep execution order and barriers once compiled
    if (!compiled) {
        executionOrder.clear();
        barrierBatches.clear();
    resourceWriteTracking.clear();
    }
    
    // Clear transient resources (including swapchain images that change per frame)
    // Keep only persistent external buffers (entity/position buffers)
    auto it = resources.begin();
    while (it != resources.end()) {
        bool shouldRemove = false;
        
        std::visit([&shouldRemove](const auto& resource) {
            // Remove non-external resources (frame-created)
            if (!resource.isExternal) {
                shouldRemove = true;
                return;
            }
            
            // Keep all external images including swapchain images (now cached)
        }, it->second);
        
        if (shouldRemove) {
            FrameGraphTypes::ResourceId resourceId = it->first;
            resourceNameMap.erase(std::visit([](const auto& resource) { return resource.debugName; }, it->second));
            resourceCleanupInfo.erase(resourceId); // Remove cleanup tracking for removed resources
            it = resources.erase(it);
        } else {
            ++it;
        }
    }
}

void FrameGraph::removeSwapchainResources() {
    // Remove all swapchain images from the frame graph to prevent "already exists" errors
    auto it = resources.begin();
    while (it != resources.end()) {
        bool shouldRemove = false;
        std::string debugName;
        
        std::visit([&shouldRemove, &debugName](const auto& resource) {
            debugName = resource.debugName;
            // Remove swapchain images (they have names like "SwapchainImage_0", "SwapchainImage_1", etc.)
            if (debugName.find("SwapchainImage_") == 0) {
                shouldRemove = true;
            }
        }, it->second);
        
        if (shouldRemove) {
            std::cout << "FrameGraph: Removing old swapchain resource: " << debugName << std::endl;
            FrameGraphTypes::ResourceId resourceId = it->first;
            resourceNameMap.erase(debugName);
            resourceCleanupInfo.erase(resourceId); // Remove cleanup tracking for swapchain resources
            it = resources.erase(it);
        } else {
            ++it;
        }
    }
}

VkBuffer FrameGraph::getBuffer(FrameGraphTypes::ResourceId id) const {
    const FrameGraphBuffer* buffer = getBufferResource(id);
    return buffer ? buffer->buffer.get() : VK_NULL_HANDLE;
}

VkImage FrameGraph::getImage(FrameGraphTypes::ResourceId id) const {
    const FrameGraphImage* image = getImageResource(id);
    return image ? image->image.get() : VK_NULL_HANDLE;
}

VkImageView FrameGraph::getImageView(FrameGraphTypes::ResourceId id) const {
    const FrameGraphImage* image = getImageResource(id);
    return image ? image->view.get() : VK_NULL_HANDLE;
}

void FrameGraph::debugPrint() const {
    std::cout << "\n=== FrameGraph Debug Info ===" << std::endl;
    std::cout << "Compiled: " << (compiled ? "Yes" : "No") << std::endl;
    std::cout << "Resources (" << resources.size() << "):" << std::endl;
    
    for (const auto& [id, resource] : resources) {
        std::visit([id](const auto& res) {
            std::cout << "  ID " << id << ": " << res.debugName;
            if constexpr (std::is_same_v<std::decay_t<decltype(res)>, FrameGraphBuffer>) {
                std::cout << " (Buffer, Size: " << res.size << ")";
            } else {
                std::cout << " (Image, " << res.extent.width << "x" << res.extent.height << ")";
            }
            std::cout << (res.isExternal ? " [External]" : " [Managed]") << std::endl;
        }, resource);
    }
    
    std::cout << "Nodes (" << nodes.size() << "):" << std::endl;
    for (const auto& [id, node] : nodes) {
        std::cout << "  ID " << id << ": " << node->getName() << std::endl;
    }
    
    if (compiled) {
        std::cout << "Execution Order: ";
        for (auto nodeId : executionOrder) {
            auto it = nodes.find(nodeId);
            if (it != nodes.end()) {
                std::cout << it->second->getName() << " -> ";
            }
        }
        std::cout << "END" << std::endl;
    }
    std::cout << "============================\n" << std::endl;
}

// Private implementation methods

std::pair<bool, bool> FrameGraph::analyzeQueueRequirements() const {
    bool computeNeeded = false;
    bool graphicsNeeded = false;
    
    for (auto nodeId : executionOrder) {
        auto it = nodes.find(nodeId);
        if (it != nodes.end()) {
            if (it->second->needsComputeQueue()) computeNeeded = true;
            if (it->second->needsGraphicsQueue()) graphicsNeeded = true;
        }
    }
    
    return {computeNeeded, graphicsNeeded};
}

void FrameGraph::beginCommandBuffers(bool useCompute, bool useGraphics, uint32_t frameIndex) {
    const auto& vk = context->getLoader();
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    if (useCompute) {
        VkCommandBuffer computeCmd = queueManager->getComputeCommandBuffer(frameIndex);
        vk.vkBeginCommandBuffer(computeCmd, &beginInfo);
    }
    if (useGraphics) {
        VkCommandBuffer graphicsCmd = queueManager->getGraphicsCommandBuffer(frameIndex);
        vk.vkBeginCommandBuffer(graphicsCmd, &beginInfo);
    }
}

void FrameGraph::endCommandBuffers(bool useCompute, bool useGraphics, uint32_t frameIndex) {
    const auto& vk = context->getLoader();
    
    if (useCompute) {
        VkCommandBuffer computeCmd = queueManager->getComputeCommandBuffer(frameIndex);
        vk.vkEndCommandBuffer(computeCmd);
    }
    if (useGraphics) {
        VkCommandBuffer graphicsCmd = queueManager->getGraphicsCommandBuffer(frameIndex);
        vk.vkEndCommandBuffer(graphicsCmd);
    }
}

void FrameGraph::insertBarriersForNode(FrameGraphTypes::NodeId nodeId, VkCommandBuffer graphicsCmd, bool& computeExecuted, bool nodeNeedsGraphics) {
    if (!computeExecuted || !nodeNeedsGraphics) return;
    
    // Find barriers targeting this node
    for (const auto& batch : barrierBatches) {
        if (batch.targetNodeId == nodeId) {
            insertBarrierBatch(batch, graphicsCmd);
        }
    }
}

void FrameGraph::executeNodesInOrder(uint32_t frameIndex, bool& computeExecuted) {
    VkCommandBuffer currentComputeCmd = queueManager->getComputeCommandBuffer(frameIndex);
    VkCommandBuffer currentGraphicsCmd = queueManager->getGraphicsCommandBuffer(frameIndex);
    
    for (auto nodeId : executionOrder) {
        auto it = nodes.find(nodeId);
        if (it == nodes.end()) continue;
        
        auto& node = it->second;
        
        insertBarriersForNode(nodeId, currentGraphicsCmd, computeExecuted, node->needsGraphicsQueue());
        
        VkCommandBuffer cmdBuffer = node->needsComputeQueue() ? currentComputeCmd : currentGraphicsCmd;
        
        if (node->needsComputeQueue()) {
            computeExecuted = true;
        }
        
        node->execute(cmdBuffer, *this);
    }
}

bool FrameGraph::createVulkanBuffer(FrameGraphBuffer& buffer) {
    allocationTelemetry.recordAttempt();
    
    // Classify resource criticality for allocation strategy
    ResourceCriticality criticality = classifyResource(buffer);
    
    return tryAllocateWithStrategy(buffer, criticality);
}

bool FrameGraph::createVulkanImage(FrameGraphImage& image) {
    allocationTelemetry.recordAttempt();
    
    // Classify resource criticality for allocation strategy
    ResourceCriticality criticality = classifyResource(image);
    
    return tryAllocateWithStrategy(image, criticality);
}

uint32_t FrameGraph::findAnyCompatibleMemoryType(uint32_t typeFilter) const {
    const auto& vk = context->getLoader();
    
    VkPhysicalDeviceMemoryProperties memProperties;
    vk.vkGetPhysicalDeviceMemoryProperties(context->getPhysicalDevice(), &memProperties);
    
    // Find first compatible memory type
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if (typeFilter & (1 << i)) {
            return i;
        }
    }
    
    throw std::runtime_error("No compatible memory type found for fallback allocation!");
}

ResourceCriticality FrameGraph::classifyResource(const FrameGraphBuffer& buffer) const {
    // Critical: Entity and position buffers that are accessed every frame
    if (buffer.usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
        // Check debug names for ECS buffers
        if (buffer.debugName.find("Entity") != std::string::npos ||
            buffer.debugName.find("Position") != std::string::npos) {
            return ResourceCriticality::Critical;
        }
    }
    
    // Important: Vertex/Index buffers used for rendering
    if (buffer.usage & (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT)) {
        return ResourceCriticality::Important;
    }
    
    // Flexible: Staging, uniform, and other temporary buffers
    return ResourceCriticality::Flexible;
}

ResourceCriticality FrameGraph::classifyResource(const FrameGraphImage& image) const {
    // Critical: Render targets and depth buffers
    if (image.usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
        return ResourceCriticality::Critical;
    }
    
    // Important: Textures that are sampled frequently
    if (image.usage & VK_IMAGE_USAGE_SAMPLED_BIT) {
        return ResourceCriticality::Important;
    }
    
    // Flexible: Storage images and other temporary resources
    return ResourceCriticality::Flexible;
}

bool FrameGraph::tryAllocateWithStrategy(FrameGraphBuffer& buffer, ResourceCriticality criticality) {
    const auto& vk = context->getLoader();
    const VkDevice device = context->getDevice();
    
    // Determine retry and fallback limits based on criticality
    int maxRetries = 1;
    std::vector<VkMemoryPropertyFlags> memoryStrategies;
    
    switch (criticality) {
        case ResourceCriticality::Critical:
            maxRetries = 2;  // Limited retries for critical resources
            memoryStrategies = {VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};  // Device local only
            break;
            
        case ResourceCriticality::Important:
            maxRetries = 2;
            memoryStrategies = {
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            };
            break;
            
        case ResourceCriticality::Flexible:
            maxRetries = 3;
            memoryStrategies = {
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                0  // Any compatible memory
            };
            break;
    }
    
    for (int retry = 0; retry < maxRetries; ++retry) {
        // Exponential backoff delay for retries (except first attempt)
        if (retry > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10 * (1 << (retry - 1))));
        }
        
        // Create buffer
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = buffer.size;
        bufferInfo.usage = buffer.usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        VkBuffer vkBuffer;
        VkResult bufferResult = vk.vkCreateBuffer(device, &bufferInfo, nullptr, &vkBuffer);
        if (bufferResult != VK_SUCCESS) {
            std::cerr << "[FrameGraph] Buffer creation failed: " << buffer.debugName 
                      << " (criticality: " << static_cast<int>(criticality) << ", attempt " << (retry + 1) 
                      << "/" << maxRetries << ", VkResult: " << bufferResult << ")" << std::endl;
            continue;
        }
        
        buffer.buffer = vulkan_raii::Buffer(vkBuffer, context);
        
        // Get memory requirements
        VkMemoryRequirements memRequirements;
        vk.vkGetBufferMemoryRequirements(device, buffer.buffer.get(), &memRequirements);
        
        // Try memory allocation strategies
        bool wasRetried = (retry > 0);
        bool wasFallback = false;
        bool wasHostMemory = false;
        
        for (size_t strategyIdx = 0; strategyIdx < memoryStrategies.size(); ++strategyIdx) {
            VkMemoryPropertyFlags memoryProperties = memoryStrategies[strategyIdx];
            wasFallback = (strategyIdx > 0);
            wasHostMemory = (memoryProperties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
            
            try {
                uint32_t memoryTypeIndex;
                if (memoryProperties == 0) {
                    memoryTypeIndex = findAnyCompatibleMemoryType(memRequirements.memoryTypeBits);
                } else {
                    memoryTypeIndex = VulkanUtils::findMemoryType(context->getPhysicalDevice(), vk, 
                                                                  memRequirements.memoryTypeBits, memoryProperties);
                }
                
                VkMemoryAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                allocInfo.allocationSize = memRequirements.size;
                allocInfo.memoryTypeIndex = memoryTypeIndex;
                
                VkDeviceMemory vkMemory;
                VkResult allocResult = vk.vkAllocateMemory(device, &allocInfo, nullptr, &vkMemory);
                if (allocResult == VK_SUCCESS) {
                    buffer.memory = vulkan_raii::DeviceMemory(vkMemory, context);
                    
                    VkResult bindResult = vk.vkBindBufferMemory(device, buffer.buffer.get(), buffer.memory.get(), 0);
                    if (bindResult == VK_SUCCESS) {
                        // Success - record telemetry
                        allocationTelemetry.recordSuccess(wasRetried, wasFallback, wasHostMemory);
                        
                        if (wasFallback) {
                            std::cerr << "[FrameGraph] PERFORMANCE WARNING: Buffer '" << buffer.debugName 
                                      << "' allocated with fallback memory (properties: 0x" << std::hex 
                                      << memoryProperties << std::dec << ")" << std::endl;
                        }
                        
                        return true;
                    } else {
                        std::cerr << "[FrameGraph] Buffer memory bind failed: " << buffer.debugName 
                                  << " (VkResult: " << bindResult << ")" << std::endl;
                        buffer.memory.reset();
                    }
                } else if (allocResult == VK_ERROR_OUT_OF_DEVICE_MEMORY || 
                          allocResult == VK_ERROR_OUT_OF_HOST_MEMORY) {
                    // Expected allocation failures - continue to next strategy
                    continue;
                }
            } catch (const std::exception& e) {
                // Memory type not available - continue
                continue;
            }
        }
        
        // All memory strategies failed for this attempt
        buffer.buffer.reset();
    }
    
    // Complete failure
    if (criticality == ResourceCriticality::Critical) {
        allocationTelemetry.recordCriticalFailure();
        std::cerr << "[FrameGraph] CRITICAL FAILURE: Unable to allocate critical buffer '" 
                  << buffer.debugName << "' - system performance will be severely degraded" << std::endl;
    }
    
    return false;
}

bool FrameGraph::tryAllocateWithStrategy(FrameGraphImage& image, ResourceCriticality criticality) {
    const auto& vk = context->getLoader();
    const VkDevice device = context->getDevice();
    
    // Determine retry and fallback limits based on criticality
    int maxRetries = 1;
    std::vector<VkMemoryPropertyFlags> memoryStrategies;
    
    switch (criticality) {
        case ResourceCriticality::Critical:
            maxRetries = 2;
            memoryStrategies = {VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};
            break;
            
        case ResourceCriticality::Important:
            maxRetries = 2;
            memoryStrategies = {
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            };
            break;
            
        case ResourceCriticality::Flexible:
            maxRetries = 3;
            memoryStrategies = {
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                0
            };
            break;
    }
    
    for (int retry = 0; retry < maxRetries; ++retry) {
        if (retry > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10 * (1 << (retry - 1))));
        }
        
        // Create image
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = image.extent.width;
        imageInfo.extent.height = image.extent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = image.format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = image.usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        VkImage vkImage;
        VkResult imageResult = vk.vkCreateImage(device, &imageInfo, nullptr, &vkImage);
        if (imageResult != VK_SUCCESS) {
            std::cerr << "[FrameGraph] Image creation failed: " << image.debugName 
                      << " (criticality: " << static_cast<int>(criticality) << ", attempt " << (retry + 1) 
                      << "/" << maxRetries << ", VkResult: " << imageResult << ")" << std::endl;
            continue;
        }
        
        image.image = vulkan_raii::Image(vkImage, context);
        
        // Get memory requirements
        VkMemoryRequirements memRequirements;
        vk.vkGetImageMemoryRequirements(device, image.image.get(), &memRequirements);
        
        bool wasRetried = (retry > 0);
        bool wasFallback = false;
        bool wasHostMemory = false;
        bool memoryAllocated = false;
        VkMemoryPropertyFlags usedMemoryProperties = 0;
        
        for (size_t strategyIdx = 0; strategyIdx < memoryStrategies.size(); ++strategyIdx) {
            VkMemoryPropertyFlags memoryProperties = memoryStrategies[strategyIdx];
            wasFallback = (strategyIdx > 0);
            wasHostMemory = (memoryProperties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
            
            try {
                uint32_t memoryTypeIndex;
                if (memoryProperties == 0) {
                    memoryTypeIndex = findAnyCompatibleMemoryType(memRequirements.memoryTypeBits);
                } else {
                    memoryTypeIndex = VulkanUtils::findMemoryType(context->getPhysicalDevice(), vk, 
                                                                  memRequirements.memoryTypeBits, memoryProperties);
                }
                
                VkMemoryAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                allocInfo.allocationSize = memRequirements.size;
                allocInfo.memoryTypeIndex = memoryTypeIndex;
                
                VkDeviceMemory vkMemory;
                VkResult allocResult = vk.vkAllocateMemory(device, &allocInfo, nullptr, &vkMemory);
                if (allocResult == VK_SUCCESS) {
                    image.memory = vulkan_raii::DeviceMemory(vkMemory, context);
                    
                    VkResult bindResult = vk.vkBindImageMemory(device, image.image.get(), image.memory.get(), 0);
                    if (bindResult == VK_SUCCESS) {
                        memoryAllocated = true;
                        usedMemoryProperties = memoryProperties;
                        break;
                    } else {
                        image.memory.reset();
                    }
                } else if (allocResult != VK_ERROR_OUT_OF_DEVICE_MEMORY && 
                          allocResult != VK_ERROR_OUT_OF_HOST_MEMORY) {
                    // Unexpected error - log and continue
                    continue;
                }
            } catch (const std::exception&) {
                continue;
            }
        }
        
        if (!memoryAllocated) {
            image.image.reset();
            continue;
        }
        
        // Create image view if needed
        if (image.usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)) {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = image.image.get();
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = image.format;
            viewInfo.subresourceRange.aspectMask = (image.format == VK_FORMAT_D32_SFLOAT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;
            
            VkImageView vkImageView;
            VkResult viewResult = vk.vkCreateImageView(device, &viewInfo, nullptr, &vkImageView);
            if (viewResult != VK_SUCCESS) {
                image.memory.reset();
                image.image.reset();
                continue;
            }
            image.view = vulkan_raii::ImageView(vkImageView, context);
        }
        
        // Success
        allocationTelemetry.recordSuccess(wasRetried, wasFallback, wasHostMemory);
        
        if (wasFallback) {
            std::cerr << "[FrameGraph] PERFORMANCE WARNING: Image '" << image.debugName 
                      << "' allocated with fallback memory (properties: 0x" << std::hex 
                      << usedMemoryProperties << std::dec << ")" << std::endl;
        }
        
        return true;
    }
    
    // Complete failure
    if (criticality == ResourceCriticality::Critical) {
        allocationTelemetry.recordCriticalFailure();
        std::cerr << "[FrameGraph] CRITICAL FAILURE: Unable to allocate critical image '" 
                  << image.debugName << "' - system performance will be severely degraded" << std::endl;
    }
    
    return false;
}

void FrameGraph::logAllocationTelemetry() const {
    if (allocationTelemetry.totalAttempts == 0) return;
    
    std::cout << "[FrameGraph] Allocation Telemetry Report:" << std::endl;
    std::cout << "  Total attempts: " << allocationTelemetry.totalAttempts << std::endl;
    std::cout << "  Successful: " << allocationTelemetry.successfulCreations << std::endl;
    std::cout << "  Retry rate: " << (allocationTelemetry.getRetryRate() * 100.0f) << "%" << std::endl;
    std::cout << "  Fallback rate: " << (allocationTelemetry.getFallbackRate() * 100.0f) << "%" << std::endl;
    std::cout << "  Host memory rate: " << (allocationTelemetry.getHostMemoryRate() * 100.0f) << "%" << std::endl;
    std::cout << "  Critical failures: " << allocationTelemetry.criticalResourceFailures << std::endl;
    
    // Performance impact warnings
    if (allocationTelemetry.getHostMemoryRate() > 0.1f) {
        std::cerr << "[FrameGraph] WARNING: >10% of allocations using host memory - GPU performance impacted" << std::endl;
    }
    
    if (allocationTelemetry.criticalResourceFailures > 0) {
        std::cerr << "[FrameGraph] CRITICAL: " << allocationTelemetry.criticalResourceFailures 
                  << " critical resource allocation failures detected" << std::endl;
    }
}

bool FrameGraph::buildDependencyGraph() {
    // For now, implement a simple dependency system
    // In a full implementation, this would build a proper dependency graph
    // based on resource reads/writes between nodes
    return true;
}

bool FrameGraph::topologicalSort() {
    // Efficient O(V + E) topological sort using Kahn's algorithm
    executionOrder.clear();
    
    // Build resource producer mapping for O(1) dependency lookups
    std::unordered_map<FrameGraphTypes::ResourceId, FrameGraphTypes::NodeId> resourceProducers;
    for (const auto& [nodeId, node] : nodes) {
        auto outputs = node->getOutputs();
        for (const auto& output : outputs) {
            resourceProducers[output.resourceId] = nodeId;
        }
    }
    
    // Build adjacency list and calculate in-degrees
    std::unordered_map<FrameGraphTypes::NodeId, std::vector<FrameGraphTypes::NodeId>> adjacencyList;
    std::unordered_map<FrameGraphTypes::NodeId, int> inDegree;
    
    // Initialize in-degrees to 0
    for (const auto& [nodeId, node] : nodes) {
        inDegree[nodeId] = 0;
        adjacencyList[nodeId] = {};
    }
    
    // Build graph edges: if nodeA produces resource that nodeB consumes, nodeA -> nodeB
    for (const auto& [nodeId, node] : nodes) {
        auto inputs = node->getInputs();
        for (const auto& input : inputs) {
            auto producerIt = resourceProducers.find(input.resourceId);
            if (producerIt != resourceProducers.end() && producerIt->second != nodeId) {
                FrameGraphTypes::NodeId producerNodeId = producerIt->second;
                adjacencyList[producerNodeId].push_back(nodeId);
                inDegree[nodeId]++;
            }
        }
    }
    
    // Kahn's algorithm: start with nodes that have no dependencies
    std::queue<FrameGraphTypes::NodeId> zeroInDegreeQueue;
    for (const auto& [nodeId, degree] : inDegree) {
        if (degree == 0) {
            zeroInDegreeQueue.push(nodeId);
        }
    }
    
    size_t processedNodes = 0;
    while (!zeroInDegreeQueue.empty()) {
        FrameGraphTypes::NodeId currentNode = zeroInDegreeQueue.front();
        zeroInDegreeQueue.pop();
        
        executionOrder.push_back(currentNode);
        processedNodes++;
        
        // Reduce in-degree for all dependent nodes
        for (FrameGraphTypes::NodeId dependentNode : adjacencyList[currentNode]) {
            inDegree[dependentNode]--;
            if (inDegree[dependentNode] == 0) {
                zeroInDegreeQueue.push(dependentNode);
            }
        }
    }
    
    // Check for circular dependencies
    if (processedNodes != nodes.size()) {
        std::cerr << "FrameGraph: Circular dependency detected. Processed " << processedNodes 
                  << " nodes out of " << nodes.size() << std::endl;
        
        // Report nodes involved in cycles (for debugging)
        for (const auto& [nodeId, degree] : inDegree) {
            if (degree > 0) {
                auto nodeIt = nodes.find(nodeId);
                if (nodeIt != nodes.end()) {
                    std::cerr << "FrameGraph: Node in cycle: " << nodeIt->second->getName() 
                              << " (ID: " << nodeId << ", remaining in-degree: " << degree << ")" << std::endl;
                }
            }
        }
        return false;
    }
    
    return true;
}

void FrameGraph::analyzeBarrierRequirements() {
    // Clear previous tracking
    resourceWriteTracking.clear();
    
    // Single pass analysis - O(n) complexity
    for (auto nodeId : executionOrder) {
        auto nodeIt = nodes.find(nodeId);
        if (nodeIt == nodes.end()) continue;
        
        auto& node = nodeIt->second;
        
        // Track resource writes for dependency analysis
        auto outputs = node->getOutputs();
        for (const auto& output : outputs) {
            resourceWriteTracking[output.resourceId] = {nodeId, output.stage, output.access};
        }
    }
}

void FrameGraph::createOptimalBarrierBatches() {
    barrierBatches.clear();
    
    // Analyze each node's input dependencies
    for (auto nodeId : executionOrder) {
        auto nodeIt = nodes.find(nodeId);
        if (nodeIt == nodes.end() || !nodeIt->second->needsGraphicsQueue()) continue;
        
        auto& node = nodeIt->second;
        auto inputs = node->getInputs();
        
        for (const auto& input : inputs) {
            auto writeIt = resourceWriteTracking.find(input.resourceId);
            if (writeIt != resourceWriteTracking.end()) {
                auto& writeInfo = writeIt->second;
                auto writerNodeIt = nodes.find(writeInfo.writerNode);
                
                // Create barriers for compute->graphics AND compute->compute transitions
                if (writerNodeIt != nodes.end() && writerNodeIt->second->needsComputeQueue()) {
                    addResourceBarrier(input.resourceId, nodeId, writeInfo.stage, input.stage, 
                                     writeInfo.access, input.access);
                }
            }
        }
    }
}

void FrameGraph::addResourceBarrier(FrameGraphTypes::ResourceId resourceId, FrameGraphTypes::NodeId targetNode,
                                   PipelineStage srcStage, PipelineStage dstStage,
                                   ResourceAccess srcAccess, ResourceAccess dstAccess) {
    
    auto convertAccess = [](ResourceAccess access, PipelineStage stage) -> VkAccessFlags {
        switch (access) {
            case ResourceAccess::Read: 
                if (stage == PipelineStage::VertexShader) {
                    return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
                }
                return VK_ACCESS_SHADER_READ_BIT;
            case ResourceAccess::Write: 
                return VK_ACCESS_SHADER_WRITE_BIT;
            case ResourceAccess::ReadWrite: 
                return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            default: 
                return 0;
        }
    };
    
    // Find or create barrier batch for this target node
    auto batchIt = std::find_if(barrierBatches.begin(), barrierBatches.end(),
        [targetNode](const NodeBarrierInfo& batch) { return batch.targetNodeId == targetNode; });
    
    if (batchIt == barrierBatches.end()) {
        barrierBatches.emplace_back();
        batchIt = barrierBatches.end() - 1;
        batchIt->targetNodeId = targetNode;
    }
    
    // Add resource barrier to the batch (no deduplication for performance)
    const FrameGraphBuffer* buffer = getBufferResource(resourceId);
    if (buffer) {
        VkBufferMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = convertAccess(srcAccess, srcStage);
        barrier.dstAccessMask = convertAccess(dstAccess, dstStage);
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = buffer->buffer.get();
        barrier.offset = 0;
        barrier.size = VK_WHOLE_SIZE;
        
        batchIt->bufferBarriers.push_back(barrier);
        return;
    }
    
    const FrameGraphImage* image = getImageResource(resourceId);
    if (image) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = convertAccess(srcAccess, srcStage);
        barrier.dstAccessMask = convertAccess(dstAccess, dstStage);
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image->image.get();
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        
        batchIt->imageBarriers.push_back(barrier);
    }
}

void FrameGraph::insertBarrierBatch(const NodeBarrierInfo& batch, VkCommandBuffer commandBuffer) {
    if (batch.bufferBarriers.empty() && batch.imageBarriers.empty()) return;
    
    const auto& vk = context->getLoader();
    
    vk.vkCmdPipelineBarrier(
        commandBuffer,
        batch.srcStage,
        batch.dstStage,
        0, // dependencyFlags
        0, nullptr, // memory barriers
        static_cast<uint32_t>(batch.bufferBarriers.size()), 
        batch.bufferBarriers.data(),
        static_cast<uint32_t>(batch.imageBarriers.size()), 
        batch.imageBarriers.data()
    );
}

FrameGraphTypes::NodeId FrameGraph::findNextGraphicsNode(FrameGraphTypes::NodeId fromNode) const {
    auto it = std::find(executionOrder.begin(), executionOrder.end(), fromNode);
    if (it == executionOrder.end()) return 0;
    
    for (++it; it != executionOrder.end(); ++it) {
        auto nodeIt = nodes.find(*it);
        if (nodeIt != nodes.end() && nodeIt->second->needsGraphicsQueue()) {
            return *it;
        }
    }
    return 0;
}

// No longer needed - RAII handles automatic cleanup

FrameGraphBuffer* FrameGraph::getBufferResource(FrameGraphTypes::ResourceId id) {
    auto it = resources.find(id);
    if (it == resources.end()) return nullptr;
    
    return std::get_if<FrameGraphBuffer>(&it->second);
}

FrameGraphImage* FrameGraph::getImageResource(FrameGraphTypes::ResourceId id) {
    auto it = resources.find(id);
    if (it == resources.end()) return nullptr;
    
    return std::get_if<FrameGraphImage>(&it->second);
}

const FrameGraphBuffer* FrameGraph::getBufferResource(FrameGraphTypes::ResourceId id) const {
    auto it = resources.find(id);
    if (it == resources.end()) return nullptr;
    
    return std::get_if<FrameGraphBuffer>(&it->second);
}

const FrameGraphImage* FrameGraph::getImageResource(FrameGraphTypes::ResourceId id) const {
    auto it = resources.find(id);
    if (it == resources.end()) return nullptr;
    
    return std::get_if<FrameGraphImage>(&it->second);
}

// Concrete node implementations

ComputeNode::ComputeNode(FrameGraphTypes::ResourceId entityBuffer, FrameGraphTypes::ResourceId positionBuffer)
    : entityBufferId(entityBuffer), positionBufferId(positionBuffer) {
}

std::vector<ResourceDependency> ComputeNode::getInputs() const {
    return {
        {entityBufferId, ResourceAccess::Read, PipelineStage::ComputeShader},
    };
}

std::vector<ResourceDependency> ComputeNode::getOutputs() const {
    return {
        {positionBufferId, ResourceAccess::Write, PipelineStage::ComputeShader},
    };
}

void ComputeNode::execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) {
    // Placeholder for compute execution
    // In the real implementation, this would bind the compute pipeline,
    // descriptor sets, push constants, and dispatch the compute shader
    std::cout << "ComputeNode: Executing compute pass" << std::endl;
}

GraphicsNode::GraphicsNode(FrameGraphTypes::ResourceId entityBuffer, FrameGraphTypes::ResourceId positionBuffer, FrameGraphTypes::ResourceId colorTarget)
    : entityBufferId(entityBuffer), positionBufferId(positionBuffer), colorTargetId(colorTarget) {
}

std::vector<ResourceDependency> GraphicsNode::getInputs() const {
    return {
        {entityBufferId, ResourceAccess::Read, PipelineStage::VertexShader},
        {positionBufferId, ResourceAccess::Read, PipelineStage::VertexShader},
    };
}

std::vector<ResourceDependency> GraphicsNode::getOutputs() const {
    return {
        {colorTargetId, ResourceAccess::Write, PipelineStage::ColorAttachment},
    };
}

void GraphicsNode::execute(VkCommandBuffer commandBuffer, const FrameGraph& frameGraph) {
    // Placeholder for graphics execution
    // In the real implementation, this would begin render pass,
    // bind graphics pipeline, descriptor sets, push constants, and draw
    std::cout << "GraphicsNode: Executing graphics pass" << std::endl;
}

// Enhanced compilation methods implementation
void FrameGraph::backupCompilationState() {
    backupState.executionOrder = executionOrder;
    backupState.barrierBatches = barrierBatches;
    backupState.resourceWriteTracking = resourceWriteTracking;
    backupState.compiled = compiled;
}

void FrameGraph::restoreCompilationState() {
    executionOrder = backupState.executionOrder;
    barrierBatches = backupState.barrierBatches;
    resourceWriteTracking = backupState.resourceWriteTracking;
    compiled = backupState.compiled;
}

bool FrameGraph::topologicalSortWithCycleDetection(CircularDependencyReport& report) {
    // Efficient O(V + E) topological sort using Kahn's algorithm with enhanced cycle detection
    executionOrder.clear();
    
    // Build resource producer mapping for O(1) dependency lookups
    std::unordered_map<FrameGraphTypes::ResourceId, FrameGraphTypes::NodeId> resourceProducers;
    for (const auto& [nodeId, node] : nodes) {
        auto outputs = node->getOutputs();
        for (const auto& output : outputs) {
            resourceProducers[output.resourceId] = nodeId;
        }
    }
    
    // Build adjacency list and calculate in-degrees
    std::unordered_map<FrameGraphTypes::NodeId, std::vector<FrameGraphTypes::NodeId>> adjacencyList;
    std::unordered_map<FrameGraphTypes::NodeId, int> inDegree;
    
    // Initialize in-degrees to 0
    for (const auto& [nodeId, node] : nodes) {
        inDegree[nodeId] = 0;
        adjacencyList[nodeId] = {};
    }
    
    // Build graph edges: if nodeA produces resource that nodeB consumes, nodeA -> nodeB
    for (const auto& [nodeId, node] : nodes) {
        auto inputs = node->getInputs();
        for (const auto& input : inputs) {
            auto producerIt = resourceProducers.find(input.resourceId);
            if (producerIt != resourceProducers.end() && producerIt->second != nodeId) {
                FrameGraphTypes::NodeId producerNodeId = producerIt->second;
                adjacencyList[producerNodeId].push_back(nodeId);
                inDegree[nodeId]++;
            }
        }
    }
    
    // Kahn's algorithm: start with nodes that have no dependencies
    std::queue<FrameGraphTypes::NodeId> zeroInDegreeQueue;
    for (const auto& [nodeId, degree] : inDegree) {
        if (degree == 0) {
            zeroInDegreeQueue.push(nodeId);
        }
    }
    
    size_t processedNodes = 0;
    while (!zeroInDegreeQueue.empty()) {
        FrameGraphTypes::NodeId currentNode = zeroInDegreeQueue.front();
        zeroInDegreeQueue.pop();
        
        executionOrder.push_back(currentNode);
        processedNodes++;
        
        // Reduce in-degree for all dependent nodes
        for (FrameGraphTypes::NodeId dependentNode : adjacencyList[currentNode]) {
            inDegree[dependentNode]--;
            if (inDegree[dependentNode] == 0) {
                zeroInDegreeQueue.push(dependentNode);
            }
        }
    }
    
    // Check for circular dependencies
    if (processedNodes != nodes.size()) {
        // Enhanced cycle analysis
        report = analyzeCycles(inDegree);
        return false;
    }
    
    return true;
}

FrameGraph::CircularDependencyReport FrameGraph::analyzeCycles(const std::unordered_map<FrameGraphTypes::NodeId, int>& inDegree) {
    CircularDependencyReport report;
    
    // Find nodes involved in cycles (those with remaining in-degree > 0)
    std::unordered_set<FrameGraphTypes::NodeId> cycleNodes;
    for (const auto& [nodeId, degree] : inDegree) {
        if (degree > 0) {
            cycleNodes.insert(nodeId);
        }
    }
    
    // Build adjacency list for cycle nodes only
    std::unordered_map<FrameGraphTypes::NodeId, std::vector<FrameGraphTypes::NodeId>> cycleAdjacencyList;
    std::unordered_map<FrameGraphTypes::ResourceId, FrameGraphTypes::NodeId> resourceProducers;
    
    // Build resource producers map
    for (const auto& [nodeId, node] : nodes) {
        if (cycleNodes.find(nodeId) != cycleNodes.end()) {
            auto outputs = node->getOutputs();
            for (const auto& output : outputs) {
                resourceProducers[output.resourceId] = nodeId;
            }
        }
    }
    
    // Build cycle adjacency list with resource tracking
    for (const auto& nodeId : cycleNodes) {
        auto nodeIt = nodes.find(nodeId);
        if (nodeIt != nodes.end()) {
            auto inputs = nodeIt->second->getInputs();
            for (const auto& input : inputs) {
                auto producerIt = resourceProducers.find(input.resourceId);
                if (producerIt != resourceProducers.end() && producerIt->second != nodeId) {
                    FrameGraphTypes::NodeId producerNodeId = producerIt->second;
                    if (cycleNodes.find(producerNodeId) != cycleNodes.end()) {
                        cycleAdjacencyList[producerNodeId].push_back(nodeId);
                    }
                }
            }
        }
    }
    
    // Find actual cycle paths using DFS
    std::unordered_set<FrameGraphTypes::NodeId> visited;
    for (const auto& startNode : cycleNodes) {
        if (visited.find(startNode) == visited.end()) {
            std::vector<DependencyPath> cyclePaths = findCyclePaths(startNode, cycleAdjacencyList);
            report.cycles.insert(report.cycles.end(), cyclePaths.begin(), cyclePaths.end());
            
            // Mark all nodes in found cycles as visited
            for (const auto& path : cyclePaths) {
                for (const auto& nodeId : path.nodeChain) {
                    visited.insert(nodeId);
                }
            }
        }
    }
    
    // Generate resolution suggestions
    report.resolutionSuggestions = generateResolutionSuggestions(report.cycles);
    
    return report;
}

std::vector<FrameGraph::DependencyPath> FrameGraph::findCyclePaths(FrameGraphTypes::NodeId startNode, 
    const std::unordered_map<FrameGraphTypes::NodeId, std::vector<FrameGraphTypes::NodeId>>& adjacencyList) {
    
    std::vector<DependencyPath> cycles;
    std::vector<FrameGraphTypes::NodeId> path;
    std::unordered_set<FrameGraphTypes::NodeId> inPath;
    
    std::function<void(FrameGraphTypes::NodeId)> dfs = [&](FrameGraphTypes::NodeId node) {
        if (inPath.find(node) != inPath.end()) {
            // Found a cycle - extract the cycle from the path
            DependencyPath cycle;
            bool inCycle = false;
            for (size_t i = 0; i < path.size(); i++) {
                if (path[i] == node) {
                    inCycle = true;
                }
                if (inCycle) {
                    cycle.nodeChain.push_back(path[i]);
                    
                    // Find the resource that creates the dependency
                    if (i + 1 < path.size()) {
                        FrameGraphTypes::NodeId nextNode = path[i + 1];
                        auto nextNodeIt = nodes.find(nextNode);
                        if (nextNodeIt != nodes.end()) {
                            auto inputs = nextNodeIt->second->getInputs();
                            for (const auto& input : inputs) {
                                // Find which resource from current node is consumed by next node
                                auto currentNodeIt = nodes.find(path[i]);
                                if (currentNodeIt != nodes.end()) {
                                    auto outputs = currentNodeIt->second->getOutputs();
                                    for (const auto& output : outputs) {
                                        if (output.resourceId == input.resourceId) {
                                            cycle.resourceChain.push_back(input.resourceId);
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            cycle.nodeChain.push_back(node); // Close the cycle
            cycles.push_back(cycle);
            return;
        }
        
        path.push_back(node);
        inPath.insert(node);
        
        auto adjIt = adjacencyList.find(node);
        if (adjIt != adjacencyList.end()) {
            for (FrameGraphTypes::NodeId neighbor : adjIt->second) {
                dfs(neighbor);
            }
        }
        
        path.pop_back();
        inPath.erase(node);
    };
    
    dfs(startNode);
    return cycles;
}

std::vector<std::string> FrameGraph::generateResolutionSuggestions(const std::vector<DependencyPath>& cycles) {
    std::vector<std::string> suggestions;
    
    if (cycles.empty()) {
        return suggestions;
    }
    
    suggestions.push_back("Consider these resolution strategies:");
    
    // Analyze each cycle for specific suggestions
    for (size_t i = 0; i < cycles.size(); i++) {
        const auto& cycle = cycles[i];
        
        suggestions.push_back("Cycle " + std::to_string(i + 1) + " resolution options:");
        
        // Suggest breaking the cycle by removing dependencies
        if (cycle.nodeChain.size() >= 2) {
            auto node1It = nodes.find(cycle.nodeChain[0]);
            auto node2It = nodes.find(cycle.nodeChain[1]);
            if (node1It != nodes.end() && node2It != nodes.end()) {
                suggestions.push_back("  • Remove dependency between " + node1It->second->getName() + 
                                    " and " + node2It->second->getName());
            }
        }
        
        // Suggest intermediate resources
        suggestions.push_back("  • Introduce intermediate buffer/texture to break direct dependency");
        
        // Suggest reordering operations
        suggestions.push_back("  • Consider if operations can be reordered or merged");
        
        // Suggest alternative data flow
        suggestions.push_back("  • Use separate render targets or double buffering");
    }
    
    suggestions.push_back("General strategies:");
    suggestions.push_back("  • Split complex nodes into smaller, independent operations");
    suggestions.push_back("  • Use temporal separation (multi-pass rendering)");
    suggestions.push_back("  • Consider if read-after-write can be converted to write-after-read");
    
    return suggestions;
}

FrameGraph::PartialCompilationResult FrameGraph::attemptPartialCompilation() {
    PartialCompilationResult result;
    
    // Build resource producer mapping
    std::unordered_map<FrameGraphTypes::ResourceId, FrameGraphTypes::NodeId> resourceProducers;
    for (const auto& [nodeId, node] : nodes) {
        auto outputs = node->getOutputs();
        for (const auto& output : outputs) {
            resourceProducers[output.resourceId] = nodeId;
        }
    }
    
    // Build adjacency list
    std::unordered_map<FrameGraphTypes::NodeId, std::vector<FrameGraphTypes::NodeId>> adjacencyList;
    std::unordered_map<FrameGraphTypes::NodeId, int> inDegree;
    
    for (const auto& [nodeId, node] : nodes) {
        inDegree[nodeId] = 0;
        adjacencyList[nodeId] = {};
    }
    
    for (const auto& [nodeId, node] : nodes) {
        auto inputs = node->getInputs();
        for (const auto& input : inputs) {
            auto producerIt = resourceProducers.find(input.resourceId);
            if (producerIt != resourceProducers.end() && producerIt->second != nodeId) {
                FrameGraphTypes::NodeId producerNodeId = producerIt->second;
                adjacencyList[producerNodeId].push_back(nodeId);
                inDegree[nodeId]++;
            }
        }
    }
    
    // Identify cycle nodes
    std::queue<FrameGraphTypes::NodeId> zeroInDegreeQueue;
    std::vector<FrameGraphTypes::NodeId> tempExecutionOrder;
    
    for (const auto& [nodeId, degree] : inDegree) {
        if (degree == 0) {
            zeroInDegreeQueue.push(nodeId);
        }
    }
    
    // Process acyclic portion
    auto tempInDegree = inDegree;
    while (!zeroInDegreeQueue.empty()) {
        FrameGraphTypes::NodeId currentNode = zeroInDegreeQueue.front();
        zeroInDegreeQueue.pop();
        
        tempExecutionOrder.push_back(currentNode);
        result.validNodes.push_back(currentNode);
        
        for (FrameGraphTypes::NodeId dependentNode : adjacencyList[currentNode]) {
            tempInDegree[dependentNode]--;
            if (tempInDegree[dependentNode] == 0) {
                zeroInDegreeQueue.push(dependentNode);
            }
        }
    }
    
    // Identify problematic nodes
    for (const auto& [nodeId, degree] : tempInDegree) {
        if (degree > 0) {
            result.problematicNodes.push_back(nodeId);
            result.cycleNodes.insert(nodeId);
        }
    }
    
    result.hasValidSubgraph = !result.validNodes.empty();
    
    return result;
}

// Resource cleanup and memory management implementation
void FrameGraph::performResourceCleanup() {
    if (!memoryMonitor) return;
    
    // Update access tracking for all resources accessed this frame
    for (auto nodeId : executionOrder) {
        auto nodeIt = nodes.find(nodeId);
        if (nodeIt == nodes.end()) continue;
        
        // Track input resources
        auto inputs = nodeIt->second->getInputs();
        for (const auto& input : inputs) {
            updateResourceAccessTracking(input.resourceId);
        }
        
        // Track output resources
        auto outputs = nodeIt->second->getOutputs();
        for (const auto& output : outputs) {
            updateResourceAccessTracking(output.resourceId);
        }
    }
    
    std::cout << "[FrameGraph] Resource cleanup performed" << std::endl;
}

bool FrameGraph::isMemoryPressureCritical() const {
    if (!memoryMonitor) return false;
    
    float memoryPressure = memoryMonitor->getMemoryPressure();
    return memoryPressure > 0.85f; // Critical threshold: >85% memory pressure
}

void FrameGraph::evictNonCriticalResources() {
    auto candidates = getEvictionCandidates();
    
    if (candidates.empty()) {
        std::cerr << "[FrameGraph] No eviction candidates available" << std::endl;
        return;
    }
    
    size_t evictedCount = 0;
    size_t targetEvictions = std::min(candidates.size(), size_t(5)); // Limit to 5 evictions per frame
    
    for (size_t i = 0; i < targetEvictions; ++i) {
        if (attemptResourceEviction(candidates[i])) {
            ++evictedCount;
        }
    }
    
    std::cout << "[FrameGraph] Evicted " << evictedCount << " non-critical resources" << std::endl;
}

void FrameGraph::updateResourceAccessTracking(FrameGraphTypes::ResourceId resourceId) {
    auto it = resourceCleanupInfo.find(resourceId);
    if (it != resourceCleanupInfo.end()) {
        it->second.lastAccessTime = std::chrono::steady_clock::now();
        it->second.accessCount++;
    }
}

void FrameGraph::markResourceForEviction(FrameGraphTypes::ResourceId resourceId) {
    auto it = resourceCleanupInfo.find(resourceId);
    if (it != resourceCleanupInfo.end() && it->second.canEvict) {
        it->second.canEvict = true; // Mark as evictable if not already
    }
}

std::vector<FrameGraphTypes::ResourceId> FrameGraph::getEvictionCandidates() {
    std::vector<FrameGraphTypes::ResourceId> candidates;
    auto now = std::chrono::steady_clock::now();
    
    // Find resources that haven't been accessed in the last 3 seconds and can be evicted
    const auto EVICTION_THRESHOLD = std::chrono::seconds(3);
    
    for (const auto& [resourceId, cleanupInfo] : resourceCleanupInfo) {
        if (!cleanupInfo.canEvict) continue;
        if (cleanupInfo.criticality == ResourceCriticality::Critical) continue; // Never evict critical resources
        
        auto timeSinceAccess = now - cleanupInfo.lastAccessTime;
        if (timeSinceAccess > EVICTION_THRESHOLD) {
            candidates.push_back(resourceId);
        }
    }
    
    // Sort candidates by criticality and last access time (least critical, oldest first)
    std::sort(candidates.begin(), candidates.end(), [this](FrameGraphTypes::ResourceId a, FrameGraphTypes::ResourceId b) {
        auto& infoA = resourceCleanupInfo.at(a);
        auto& infoB = resourceCleanupInfo.at(b);
        
        if (infoA.criticality != infoB.criticality) {
            return infoA.criticality > infoB.criticality; // Less critical first
        }
        return infoA.lastAccessTime < infoB.lastAccessTime; // Older first
    });
    
    return candidates;
}

bool FrameGraph::attemptResourceEviction(FrameGraphTypes::ResourceId resourceId) {
    auto resourceIt = resources.find(resourceId);
    auto cleanupIt = resourceCleanupInfo.find(resourceId);
    
    if (resourceIt == resources.end() || cleanupIt == resourceCleanupInfo.end()) {
        return false;
    }
    
    if (!cleanupIt->second.canEvict || cleanupIt->second.criticality == ResourceCriticality::Critical) {
        return false;
    }
    
    // Find and remove from name map
    std::string debugName;
    std::visit([&debugName](const auto& resource) {
        debugName = resource.debugName;
    }, resourceIt->second);
    
    auto nameIt = std::find_if(resourceNameMap.begin(), resourceNameMap.end(),
        [resourceId](const auto& pair) { return pair.second == resourceId; });
    
    if (nameIt != resourceNameMap.end()) {
        resourceNameMap.erase(nameIt);
    }
    
    // Remove resource and cleanup info
    resources.erase(resourceIt);
    resourceCleanupInfo.erase(cleanupIt);
    
    std::cout << "[FrameGraph] Evicted resource: " << debugName << " (ID: " << resourceId << ")" << std::endl;
    return true;
}

// Timeout-aware execution implementation
bool FrameGraph::executeWithTimeoutMonitoring(uint32_t frameIndex, bool& computeExecuted) {
    VkCommandBuffer currentComputeCmd = queueManager->getComputeCommandBuffer(frameIndex);
    VkCommandBuffer currentGraphicsCmd = queueManager->getGraphicsCommandBuffer(frameIndex);
    
    for (auto nodeId : executionOrder) {
        auto it = nodes.find(nodeId);
        if (it == nodes.end()) continue;
        
        auto& node = it->second;
        
        // Check GPU health before executing
        if (!timeoutDetector->isGPUHealthy()) {
            std::cerr << "[FrameGraph] GPU unhealthy, aborting execution" << std::endl;
            return false;
        }
        
        insertBarriersForNode(nodeId, currentGraphicsCmd, computeExecuted, node->needsGraphicsQueue());
        
        VkCommandBuffer cmdBuffer = node->needsComputeQueue() ? currentComputeCmd : currentGraphicsCmd;
        
        // Begin timeout monitoring for this node
        std::string nodeName = node->getName() + "_FrameGraph";
        if (node->needsComputeQueue()) {
            timeoutDetector->beginComputeDispatch(nodeName.c_str(), 1); // Generic workgroup count
            computeExecuted = true;
        }
        
        // Execute the node
        node->execute(cmdBuffer, *this);
        
        // End timeout monitoring
        if (node->needsComputeQueue()) {
            timeoutDetector->endComputeDispatch();
            
            // Check if we need to apply recovery recommendations
            auto recommendation = timeoutDetector->getRecoveryRecommendation();
            if (recommendation.shouldReduceWorkload) {
                std::cout << "[FrameGraph] Applying timeout recovery recommendations" << std::endl;
                // Future: Could implement workload reduction at frame graph level
            }
        }
        
        // Final health check after node execution
        if (!timeoutDetector->isGPUHealthy()) {
            std::cerr << "[FrameGraph] GPU became unhealthy after node execution" << std::endl;
            return false;
        }
    }
    
    return true;
}

void FrameGraph::handleExecutionTimeout() {
    std::cerr << "[FrameGraph] Execution timeout detected - frame graph execution aborted" << std::endl;
    
    if (timeoutDetector) {
        auto stats = timeoutDetector->getStats();
        std::cerr << "[FrameGraph] Timeout stats - Average: " << stats.averageDispatchTimeMs 
                  << "ms, Peak: " << stats.peakDispatchTimeMs << "ms, Warnings: " << stats.warningCount << std::endl;
    }
    
    // Could implement additional recovery strategies here:
    // - Mark nodes for reduced execution
    // - Schedule recompilation with simpler graph
    // - Request external systems to reduce entity count
}
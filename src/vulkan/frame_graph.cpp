#include "frame_graph.h"
#include "vulkan_context.h"
#include "vulkan_sync.h"
#include "vulkan_utils.h"
#include "nodes/entity_compute_node.h"
#include "nodes/entity_graphics_node.h"
#include <iostream>
#include <queue>
#include <algorithm>
#include <cassert>
#include <functional>
#include <unordered_set>

FrameGraph::FrameGraph() {
}

FrameGraph::~FrameGraph() {
    cleanup();
}

bool FrameGraph::initialize(const VulkanContext& context, VulkanSync* sync) {
    this->context = &context;
    this->sync = sync;
    
    if (context.getDevice() == VK_NULL_HANDLE || !sync) {
        std::cerr << "FrameGraph: Invalid context or sync objects" << std::endl;
        return false;
    }
    
    initialized = true;
    std::cout << "FrameGraph initialized successfully" << std::endl;
    return true;
}

void FrameGraph::cleanup() {
    if (!initialized) return;
    
    cleanupResources();
    
    nodes.clear();
    resources.clear();
    resourceNameMap.clear();
    executionOrder.clear();
    computeToGraphicsBarriers.bufferBarriers.clear();
    computeToGraphicsBarriers.imageBarriers.clear();
    
    nextResourceId = 1;
    nextNodeId = 1;
    compiled = false;
    initialized = false;
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
    
    resources[id] = buffer;
    resourceNameMap[name] = id;
    
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
    
    resources[id] = image;
    resourceNameMap[name] = id;
    
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
    frameGraphBuffer.buffer = buffer;
    frameGraphBuffer.size = size;
    frameGraphBuffer.usage = usage;
    frameGraphBuffer.isExternal = true; // Don't manage lifecycle
    frameGraphBuffer.debugName = name;
    
    resources[id] = frameGraphBuffer;
    resourceNameMap[name] = id;
    
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
    frameGraphImage.image = image;
    frameGraphImage.view = view;
    frameGraphImage.format = format;
    frameGraphImage.extent = extent;
    frameGraphImage.isExternal = true; // Don't manage lifecycle
    frameGraphImage.debugName = name;
    
    resources[id] = frameGraphImage;
    resourceNameMap[name] = id;
    
    // Imported external image
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
    
    // Clear previous compilation
    executionOrder.clear();
    computeToGraphicsBarriers.bufferBarriers.clear();
    computeToGraphicsBarriers.imageBarriers.clear();
    
    // Build dependency graph and sort nodes
    if (!buildDependencyGraph()) {
        std::cerr << "FrameGraph: Failed to build dependency graph" << std::endl;
        return false;
    }
    
    if (!topologicalSort()) {
        std::cerr << "FrameGraph: Failed to sort nodes (circular dependency?)" << std::endl;
        return false;
    }
    
    // Insert synchronization barriers
    insertSynchronizationBarriers();
    
    // Setup nodes
    for (auto nodeId : executionOrder) {
        auto it = nodes.find(nodeId);
        if (it != nodes.end()) {
            it->second->setup(*this);
        }
    }
    
    compiled = true;
    // Compilation successful
    
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
    
    // Get command buffers and synchronization objects
    const auto& computeCommandBuffers = sync->getComputeCommandBuffers();
    const auto& graphicsCommandBuffers = sync->getCommandBuffers();
    
    VkCommandBuffer currentComputeCmd = computeCommandBuffers[frameIndex];
    VkCommandBuffer currentGraphicsCmd = graphicsCommandBuffers[frameIndex];
    
    // Note: Command buffer reset is handled by VulkanRenderer after fence waits
    // Frame graph assumes command buffers are already reset and ready for recording
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    // Check which command buffers we'll need
    for (auto nodeId : executionOrder) {
        auto it = nodes.find(nodeId);
        if (it != nodes.end()) {
            if (it->second->needsComputeQueue()) result.computeCommandBufferUsed = true;
            if (it->second->needsGraphicsQueue()) result.graphicsCommandBufferUsed = true;
        }
    }
    
    // Only begin command buffers that will be used
    if (result.computeCommandBufferUsed) {
        context->getLoader().vkBeginCommandBuffer(currentComputeCmd, &beginInfo);
    }
    if (result.graphicsCommandBufferUsed) {
        context->getLoader().vkBeginCommandBuffer(currentGraphicsCmd, &beginInfo);
    }
    
    // Execute nodes in dependency order
    bool computeExecuted = false;
    for (auto nodeId : executionOrder) {
        auto it = nodes.find(nodeId);
        if (it == nodes.end()) continue;
        
        auto& node = it->second;
        
        // If switching from compute to graphics, insert barriers first
        if (computeExecuted && node->needsGraphicsQueue() && (!computeToGraphicsBarriers.bufferBarriers.empty() || !computeToGraphicsBarriers.imageBarriers.empty())) {
            // Inserting barriers between compute and graphics
            
            // Barriers ready for insertion
            
            // Insert barriers into graphics command buffer BEFORE graphics work
            context->getLoader().vkCmdPipelineBarrier(
                currentGraphicsCmd,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                0,
                0, nullptr,
                static_cast<uint32_t>(computeToGraphicsBarriers.bufferBarriers.size()), 
                computeToGraphicsBarriers.bufferBarriers.data(),
                static_cast<uint32_t>(computeToGraphicsBarriers.imageBarriers.size()), 
                computeToGraphicsBarriers.imageBarriers.data()
            );
            // Barrier insertion complete
            
            // Clear the flag to avoid inserting barriers multiple times
            computeExecuted = false;
        }
        
        // Choose appropriate command buffer based on node requirements
        VkCommandBuffer cmdBuffer = node->needsComputeQueue() ? currentComputeCmd : currentGraphicsCmd;
        
        // Track if compute was executed
        if (node->needsComputeQueue()) {
            computeExecuted = true;
        }
        
        // Executing node
        
        // Execute the node
        node->execute(cmdBuffer, *this);
        
        // Node execution completed
    }
    
    // End only the command buffers that were begun
    if (result.computeCommandBufferUsed) {
        context->getLoader().vkEndCommandBuffer(currentComputeCmd);
    }
    if (result.graphicsCommandBufferUsed) {
        context->getLoader().vkEndCommandBuffer(currentGraphicsCmd);
    }
    
    // Frame graph complete - command buffers are ready for submission by VulkanRenderer
    return result;
}

void FrameGraph::reset() {
    // Reset for next frame - clear transient state but keep persistent resources and nodes
    // Keep execution order and barriers once compiled
    if (!compiled) {
        executionOrder.clear();
        computeToGraphicsBarriers.bufferBarriers.clear();
        computeToGraphicsBarriers.imageBarriers.clear();
        computeToGraphicsBarriers.srcStage = 0;
        computeToGraphicsBarriers.dstStage = 0;
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
            resourceNameMap.erase(std::visit([](const auto& resource) { return resource.debugName; }, it->second));
            it = resources.erase(it);
        } else {
            ++it;
        }
    }
}

VkBuffer FrameGraph::getBuffer(FrameGraphTypes::ResourceId id) const {
    const FrameGraphBuffer* buffer = getBufferResource(id);
    return buffer ? buffer->buffer : VK_NULL_HANDLE;
}

VkImage FrameGraph::getImage(FrameGraphTypes::ResourceId id) const {
    const FrameGraphImage* image = getImageResource(id);
    return image ? image->image : VK_NULL_HANDLE;
}

VkImageView FrameGraph::getImageView(FrameGraphTypes::ResourceId id) const {
    const FrameGraphImage* image = getImageResource(id);
    return image ? image->view : VK_NULL_HANDLE;
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

bool FrameGraph::createVulkanBuffer(FrameGraphBuffer& buffer) {
    // Create buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = buffer.size;
    bufferInfo.usage = buffer.usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (context->getLoader().vkCreateBuffer(context->getDevice(), &bufferInfo, nullptr, &buffer.buffer) != VK_SUCCESS) {
        std::cerr << "FrameGraph: Failed to create buffer" << std::endl;
        return false;
    }
    
    // Allocate memory
    VkMemoryRequirements memRequirements;
    context->getLoader().vkGetBufferMemoryRequirements(context->getDevice(), buffer.buffer, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VulkanUtils::findMemoryType(context->getPhysicalDevice(), context->getLoader(), memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    if (context->getLoader().vkAllocateMemory(context->getDevice(), &allocInfo, nullptr, &buffer.memory) != VK_SUCCESS) {
        std::cerr << "FrameGraph: Failed to allocate buffer memory" << std::endl;
        context->getLoader().vkDestroyBuffer(context->getDevice(), buffer.buffer, nullptr);
        buffer.buffer = VK_NULL_HANDLE;
        return false;
    }
    
    // Bind memory
    context->getLoader().vkBindBufferMemory(context->getDevice(), buffer.buffer, buffer.memory, 0);
    
    return true;
}

bool FrameGraph::createVulkanImage(FrameGraphImage& image) {
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
    
    if (context->getLoader().vkCreateImage(context->getDevice(), &imageInfo, nullptr, &image.image) != VK_SUCCESS) {
        std::cerr << "FrameGraph: Failed to create image" << std::endl;
        return false;
    }
    
    // Allocate memory
    VkMemoryRequirements memRequirements;
    context->getLoader().vkGetImageMemoryRequirements(context->getDevice(), image.image, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VulkanUtils::findMemoryType(context->getPhysicalDevice(), context->getLoader(), memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    if (context->getLoader().vkAllocateMemory(context->getDevice(), &allocInfo, nullptr, &image.memory) != VK_SUCCESS) {
        std::cerr << "FrameGraph: Failed to allocate image memory" << std::endl;
        context->getLoader().vkDestroyImage(context->getDevice(), image.image, nullptr);
        image.image = VK_NULL_HANDLE;
        return false;
    }
    
    // Bind memory
    context->getLoader().vkBindImageMemory(context->getDevice(), image.image, image.memory, 0);
    
    // Create image view if it's going to be used as texture/render target
    if (image.usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image.image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = image.format;
        viewInfo.subresourceRange.aspectMask = (image.format == VK_FORMAT_D32_SFLOAT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        
        if (context->getLoader().vkCreateImageView(context->getDevice(), &viewInfo, nullptr, &image.view) != VK_SUCCESS) {
            std::cerr << "FrameGraph: Failed to create image view" << std::endl;
            context->getLoader().vkFreeMemory(context->getDevice(), image.memory, nullptr);
            context->getLoader().vkDestroyImage(context->getDevice(), image.image, nullptr);
            image.image = VK_NULL_HANDLE;
            image.memory = VK_NULL_HANDLE;
            return false;
        }
    }
    
    return true;
}

bool FrameGraph::buildDependencyGraph() {
    // For now, implement a simple dependency system
    // In a full implementation, this would build a proper dependency graph
    // based on resource reads/writes between nodes
    return true;
}

bool FrameGraph::topologicalSort() {
    // Implement proper topological sorting based on resource dependencies
    executionOrder.clear();
    
    std::unordered_set<FrameGraphTypes::NodeId> visited;
    std::unordered_set<FrameGraphTypes::NodeId> visiting;
    
    std::function<bool(FrameGraphTypes::NodeId)> visit = [&](FrameGraphTypes::NodeId nodeId) -> bool {
        if (visiting.count(nodeId)) {
            std::cerr << "FrameGraph: Circular dependency detected involving node " << nodeId << std::endl;
            return false; // Circular dependency
        }
        if (visited.count(nodeId)) {
            return true; // Already processed
        }
        
        visiting.insert(nodeId);
        
        auto nodeIt = nodes.find(nodeId);
        if (nodeIt == nodes.end()) return false;
        
        auto& node = nodeIt->second;
        auto inputs = node->getInputs();
        
        // Find nodes that produce our input resources (dependencies)
        for (const auto& input : inputs) {
            for (const auto& [otherNodeId, otherNode] : nodes) {
                if (otherNodeId == nodeId) continue;
                
                auto outputs = otherNode->getOutputs();
                for (const auto& output : outputs) {
                    if (output.resourceId == input.resourceId) {
                        // This node depends on otherNode
                        if (!visit(otherNodeId)) {
                            return false;
                        }
                    }
                }
            }
        }
        
        visiting.erase(nodeId);
        visited.insert(nodeId);
        executionOrder.push_back(nodeId);
        return true;
    };
    
    // Visit all nodes
    for (const auto& [nodeId, node] : nodes) {
        if (!visited.count(nodeId)) {
            if (!visit(nodeId)) {
                return false;
            }
        }
    }
    
    return true;
}

void FrameGraph::insertSynchronizationBarriers() {
    // Optimized O(n) barrier insertion with resource-based batching
    
    // Clear existing barriers
    computeToGraphicsBarriers.bufferBarriers.clear();
    computeToGraphicsBarriers.imageBarriers.clear();
    computeToGraphicsBarriers.srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    computeToGraphicsBarriers.dstStage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    
    // Track resource write states for linear dependency checking
    std::unordered_map<FrameGraphTypes::ResourceId, std::pair<PipelineStage, ResourceAccess>> lastResourceWrites;
    std::unordered_set<FrameGraphTypes::ResourceId> barrierPendingResources;
    
    // Single pass through execution order - O(n) complexity
    for (auto nodeId : executionOrder) {
        auto nodeIt = nodes.find(nodeId);
        if (nodeIt == nodes.end()) continue;
        
        auto& node = nodeIt->second;
        bool isComputeNode = node->needsComputeQueue();
        bool isGraphicsNode = node->needsGraphicsQueue();
        
        // Process node inputs - check for barriers needed
        auto inputs = node->getInputs();
        for (const auto& input : inputs) {
            auto writeIt = lastResourceWrites.find(input.resourceId);
            if (writeIt != lastResourceWrites.end()) {
                // Resource was previously written - check if barrier needed
                auto [lastStage, lastAccess] = writeIt->second;
                
                // Only create barriers for compute->graphics transitions
                if (isGraphicsNode && !barrierPendingResources.count(input.resourceId)) {
                    insertBarrierForResource(input.resourceId, lastStage, input.stage, lastAccess, input.access);
                    barrierPendingResources.insert(input.resourceId);
                }
            }
        }
        
        // Update resource write tracking for node outputs
        auto outputs = node->getOutputs();
        for (const auto& output : outputs) {
            lastResourceWrites[output.resourceId] = {output.stage, output.access};
            // Clear barrier pending since this is a new write
            barrierPendingResources.erase(output.resourceId);
        }
    }
}

void FrameGraph::insertBarrierForResource(FrameGraphTypes::ResourceId resourceId, 
                                          PipelineStage srcStage, PipelineStage dstStage,
                                          ResourceAccess srcAccess, ResourceAccess dstAccess) {
    
    auto convertAccess = [](ResourceAccess access, PipelineStage stage) -> VkAccessFlags {
        switch (access) {
            case ResourceAccess::Read: 
                // Special case: vertex shader reading as vertex attributes
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
    
    // Check if this is a buffer or image resource
    const FrameGraphBuffer* buffer = getBufferResource(resourceId);
    if (buffer) {
        VkAccessFlags srcMask = convertAccess(srcAccess, srcStage);
        VkAccessFlags dstMask = convertAccess(dstAccess, dstStage);
        
        // Check for duplicate barriers on same buffer
        for (const auto& existingBarrier : computeToGraphicsBarriers.bufferBarriers) {
            if (existingBarrier.buffer == buffer->buffer && 
                existingBarrier.srcAccessMask == srcMask && 
                existingBarrier.dstAccessMask == dstMask) {
                return; // Duplicate barrier - skip
            }
        }
        
        VkBufferMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = srcMask;
        barrier.dstAccessMask = dstMask;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = buffer->buffer;
        barrier.offset = 0;
        barrier.size = VK_WHOLE_SIZE;
        
        computeToGraphicsBarriers.bufferBarriers.push_back(barrier);
        return;
    }
    
    const FrameGraphImage* image = getImageResource(resourceId);
    if (image) {
        VkAccessFlags srcMask = convertAccess(srcAccess, srcStage);
        VkAccessFlags dstMask = convertAccess(dstAccess, dstStage);
        
        // Check for duplicate barriers on same image
        for (const auto& existingBarrier : computeToGraphicsBarriers.imageBarriers) {
            if (existingBarrier.image == image->image && 
                existingBarrier.srcAccessMask == srcMask && 
                existingBarrier.dstAccessMask == dstMask) {
                return; // Duplicate barrier - skip
            }
        }
        
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = srcMask;
        barrier.dstAccessMask = dstMask;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL; // Simplified for now
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image->image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        
        computeToGraphicsBarriers.imageBarriers.push_back(barrier);
    }
}

void FrameGraph::insertBarriersIntoCommandBuffer(VkCommandBuffer commandBuffer) {
    if (computeToGraphicsBarriers.bufferBarriers.empty() && computeToGraphicsBarriers.imageBarriers.empty()) return;
    
    context->getLoader().vkCmdPipelineBarrier(
        commandBuffer,
        computeToGraphicsBarriers.srcStage,
        computeToGraphicsBarriers.dstStage,
        0, // dependencyFlags
        0, nullptr, // memory barriers
        static_cast<uint32_t>(computeToGraphicsBarriers.bufferBarriers.size()), 
        computeToGraphicsBarriers.bufferBarriers.data(),
        static_cast<uint32_t>(computeToGraphicsBarriers.imageBarriers.size()), 
        computeToGraphicsBarriers.imageBarriers.data()
    );
}

void FrameGraph::cleanupResources() {
    if (!context || context->getDevice() == VK_NULL_HANDLE) return;
    
    for (auto& [id, resource] : resources) {
        std::visit([this](auto& res) {
            if (!res.isExternal) {
                if constexpr (std::is_same_v<std::decay_t<decltype(res)>, FrameGraphBuffer>) {
                    if (res.buffer != VK_NULL_HANDLE) {
                        context->getLoader().vkDestroyBuffer(context->getDevice(), res.buffer, nullptr);
                        res.buffer = VK_NULL_HANDLE;
                    }
                    if (res.memory != VK_NULL_HANDLE) {
                        context->getLoader().vkFreeMemory(context->getDevice(), res.memory, nullptr);
                        res.memory = VK_NULL_HANDLE;
                    }
                } else {
                    if (res.view != VK_NULL_HANDLE) {
                        context->getLoader().vkDestroyImageView(context->getDevice(), res.view, nullptr);
                        res.view = VK_NULL_HANDLE;
                    }
                    if (res.image != VK_NULL_HANDLE) {
                        context->getLoader().vkDestroyImage(context->getDevice(), res.image, nullptr);
                        res.image = VK_NULL_HANDLE;
                    }
                    if (res.memory != VK_NULL_HANDLE) {
                        context->getLoader().vkFreeMemory(context->getDevice(), res.memory, nullptr);
                        res.memory = VK_NULL_HANDLE;
                    }
                }
            }
        }, resource);
    }
}

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
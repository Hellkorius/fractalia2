#include "frame_graph.h"
#include "vulkan_context.h"
#include "vulkan_sync.h"
#include "vulkan_utils.h"
#include <iostream>
#include <queue>
#include <algorithm>
#include <cassert>

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
    bufferBarriers.clear();
    imageBarriers.clear();
    
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
    
    std::cout << "FrameGraph: Imported external image '" << name << "' (ID: " << id << ")" << std::endl;
    return id;
}

bool FrameGraph::compile() {
    if (!initialized) {
        std::cerr << "FrameGraph: Cannot compile, not initialized" << std::endl;
        return false;
    }
    
    std::cout << "FrameGraph: Compiling with " << nodes.size() << " nodes and " << resources.size() << " resources" << std::endl;
    
    // Clear previous compilation
    executionOrder.clear();
    bufferBarriers.clear();
    imageBarriers.clear();
    
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
    std::cout << "FrameGraph: Compilation successful, execution order: ";
    for (auto nodeId : executionOrder) {
        auto it = nodes.find(nodeId);
        if (it != nodes.end()) {
            std::cout << it->second->getName() << " ";
        }
    }
    std::cout << std::endl;
    
    return true;
}

void FrameGraph::execute(uint32_t frameIndex) {
    if (!compiled) {
        std::cerr << "FrameGraph: Cannot execute, not compiled" << std::endl;
        return;
    }
    
    if (!sync) {
        std::cerr << "FrameGraph: Cannot execute, no sync object" << std::endl;
        return;
    }
    
    // Get command buffers (assuming we have both compute and graphics)
    const auto& computeCommandBuffers = sync->getComputeCommandBuffers();
    const auto& graphicsCommandBuffers = sync->getCommandBuffers();
    
    VkCommandBuffer currentComputeCmd = computeCommandBuffers[frameIndex];
    VkCommandBuffer currentGraphicsCmd = graphicsCommandBuffers[frameIndex];
    
    // Execute nodes in dependency order
    for (auto nodeId : executionOrder) {
        auto it = nodes.find(nodeId);
        if (it == nodes.end()) continue;
        
        auto& node = it->second;
        
        // Choose appropriate command buffer based on node requirements
        VkCommandBuffer cmdBuffer = node->needsComputeQueue() ? currentComputeCmd : currentGraphicsCmd;
        
        // Execute the node
        node->execute(cmdBuffer, *this);
    }
}

void FrameGraph::reset() {
    // Reset for next frame - keep resources and nodes, just clear execution state
    // This allows recompilation with different resource states if needed
    compiled = false;
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
    // Simple implementation: just add nodes in order they were added
    // In a full implementation, this would do proper topological sorting
    // based on the dependency graph
    
    for (const auto& [nodeId, node] : nodes) {
        executionOrder.push_back(nodeId);
    }
    
    return true;
}

void FrameGraph::insertSynchronizationBarriers() {
    // Placeholder for barrier insertion logic
    // In a full implementation, this would analyze resource dependencies
    // and insert appropriate memory barriers between nodes
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
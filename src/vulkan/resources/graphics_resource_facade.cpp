#include "graphics_resource_facade.h"
#include "resource_context.h"
#include "graphics_resource_manager.h"
#include <iostream>

GraphicsResourceFacade::GraphicsResourceFacade() {
}

GraphicsResourceFacade::~GraphicsResourceFacade() {
    cleanup();
}

bool GraphicsResourceFacade::initialize(ResourceContext* resourceContext, GraphicsResourceManager* graphicsManager) {
    this->resourceContext = resourceContext;
    this->graphicsManager = graphicsManager;
    
    if (!validateDependencies()) {
        std::cerr << "GraphicsResourceFacade: Invalid dependencies provided!" << std::endl;
        return false;
    }
    
    initialized = true;
    return true;
}

void GraphicsResourceFacade::cleanup() {
    cleanupGraphicsResources();
    resourceContext = nullptr;
    graphicsManager = nullptr;
    initialized = false;
    resourcesNeedRecreation = false;
}

bool GraphicsResourceFacade::createAllGraphicsResources() {
    if (!validateDependencies() || !initialized) {
        return false;
    }
    
    // Create resources in proper order
    bool success = true;
    
    success &= createUniformBuffers();
    success &= createGeometryBuffers();
    
    if (success) {
        clearRecreationFlag();
    }
    
    return success;
}

bool GraphicsResourceFacade::recreateGraphicsResources() {
    if (!validateDependencies()) {
        return false;
    }
    
    bool success = graphicsManager->recreateGraphicsDescriptors();
    
    if (success) {
        clearRecreationFlag();
    }
    
    return success;
}

void GraphicsResourceFacade::cleanupGraphicsResources() {
    if (graphicsManager) {
        graphicsManager->cleanupBeforeContextDestruction();
    }
    markForRecreation();
}

bool GraphicsResourceFacade::createUniformBuffers() {
    if (!validateDependencies()) {
        return false;
    }
    
    return graphicsManager->createUniformBuffers();
}

const std::vector<VkBuffer>& GraphicsResourceFacade::getUniformBuffers() const {
    static const std::vector<VkBuffer> empty;
    return graphicsManager ? graphicsManager->getUniformBuffers() : empty;
}

const std::vector<void*>& GraphicsResourceFacade::getUniformBuffersMapped() const {
    static const std::vector<void*> empty;
    return graphicsManager ? graphicsManager->getUniformBuffersMapped() : empty;
}

bool GraphicsResourceFacade::createGeometryBuffers() {
    if (!validateDependencies()) {
        return false;
    }
    
    return graphicsManager->createTriangleBuffers();
}

VkBuffer GraphicsResourceFacade::getVertexBuffer() const {
    return graphicsManager ? graphicsManager->getVertexBuffer() : VK_NULL_HANDLE;
}

VkBuffer GraphicsResourceFacade::getIndexBuffer() const {
    return graphicsManager ? graphicsManager->getIndexBuffer() : VK_NULL_HANDLE;
}

uint32_t GraphicsResourceFacade::getIndexCount() const {
    return graphicsManager ? graphicsManager->getIndexCount() : 0;
}

bool GraphicsResourceFacade::createDescriptorResources(VkDescriptorSetLayout descriptorSetLayout) {
    if (!validateDependencies() || descriptorSetLayout == VK_NULL_HANDLE) {
        return false;
    }
    
    bool success = true;
    success &= graphicsManager->createGraphicsDescriptorPool(descriptorSetLayout);
    success &= graphicsManager->createGraphicsDescriptorSets(descriptorSetLayout);
    
    return success;
}

bool GraphicsResourceFacade::updateDescriptorSetsForEntityRendering(VkBuffer entityBuffer, VkBuffer positionBuffer) {
    if (!validateDependencies()) {
        return false;
    }
    
    return graphicsManager->updateDescriptorSetsWithEntityAndPositionBuffers(entityBuffer, positionBuffer);
}

bool GraphicsResourceFacade::updateDescriptorSetsForPositionBuffers(VkBuffer currentPositionBuffer, VkBuffer targetPositionBuffer) {
    if (!validateDependencies()) {
        return false;
    }
    
    return graphicsManager->updateDescriptorSetsWithPositionBuffers(currentPositionBuffer, targetPositionBuffer);
}

VkDescriptorPool GraphicsResourceFacade::getDescriptorPool() const {
    return graphicsManager ? graphicsManager->getGraphicsDescriptorPool() : VK_NULL_HANDLE;
}

const std::vector<VkDescriptorSet>& GraphicsResourceFacade::getDescriptorSets() const {
    static const std::vector<VkDescriptorSet> empty;
    return graphicsManager ? graphicsManager->getGraphicsDescriptorSets() : empty;
}

bool GraphicsResourceFacade::areResourcesCreated() const {
    if (!graphicsManager) {
        return false;
    }
    
    return !getUniformBuffers().empty() && 
           getVertexBuffer() != VK_NULL_HANDLE && 
           getIndexBuffer() != VK_NULL_HANDLE;
}

bool GraphicsResourceFacade::areDescriptorsCreated() const {
    return graphicsManager && 
           getDescriptorPool() != VK_NULL_HANDLE && 
           !getDescriptorSets().empty();
}

bool GraphicsResourceFacade::needsRecreation() const {
    return resourcesNeedRecreation;
}

GraphicsResourceFacade::GraphicsResourceInfo GraphicsResourceFacade::getResourceInfo() const {
    GraphicsResourceInfo info;
    
    if (!graphicsManager) {
        return info;
    }
    
    const auto& uniformBuffers = getUniformBuffers();
    const auto& descriptorSets = getDescriptorSets();
    
    info.uniformBuffersCreated = !uniformBuffers.empty();
    info.geometryBuffersCreated = (getVertexBuffer() != VK_NULL_HANDLE && getIndexBuffer() != VK_NULL_HANDLE);
    info.descriptorPoolCreated = (getDescriptorPool() != VK_NULL_HANDLE);
    info.descriptorSetsCreated = !descriptorSets.empty();
    
    info.uniformBufferCount = static_cast<uint32_t>(uniformBuffers.size());
    info.descriptorSetCount = static_cast<uint32_t>(descriptorSets.size());
    
    // Note: We don't have direct access to buffer sizes from GraphicsResourceManager
    // This could be added if detailed memory tracking is needed
    info.totalUniformBufferSize = 0;
    info.totalGeometryBufferSize = 0;
    
    return info;
}

bool GraphicsResourceFacade::createResourcesBatch(const ResourceCreationBatch& batch) {
    if (!validateDependencies()) {
        return false;
    }
    
    bool success = true;
    
    if (batch.createUniformBuffers) {
        success &= createUniformBuffers();
    }
    
    if (batch.createGeometryBuffers) {
        success &= createGeometryBuffers();
    }
    
    if (batch.createDescriptorPool || batch.createDescriptorSets) {
        if (batch.descriptorSetLayout != VK_NULL_HANDLE) {
            if (batch.createDescriptorPool) {
                success &= graphicsManager->createGraphicsDescriptorPool(batch.descriptorSetLayout);
            }
            
            if (batch.createDescriptorSets) {
                success &= graphicsManager->createGraphicsDescriptorSets(batch.descriptorSetLayout);
            }
        } else {
            std::cerr << "GraphicsResourceFacade: Descriptor set layout required for descriptor operations!" << std::endl;
            success = false;
        }
    }
    
    if (success) {
        clearRecreationFlag();
    }
    
    return success;
}

bool GraphicsResourceFacade::optimizeGraphicsMemoryUsage() {
    // Future optimization opportunities:
    // - Coalesce small uniform buffers
    // - Optimize descriptor pool sizes
    // - Defragment graphics memory
    
    // For now, just attempt to recreate resources if needed
    if (needsRecreation()) {
        return recreateGraphicsResources();
    }
    
    return true;
}

VkDeviceSize GraphicsResourceFacade::getGraphicsMemoryFootprint() const {
    // This would require memory tracking from the graphics resource manager
    // For now, return 0 as placeholder
    return 0;
}

bool GraphicsResourceFacade::validateDependencies() const {
    return resourceContext && graphicsManager;
}
#include "frame_graph_resource_registry.h"
#include "frame_graph.h"
#include "../ecs/gpu_entity_manager.h"
#include <iostream>

FrameGraphResourceRegistry::FrameGraphResourceRegistry() {
}

FrameGraphResourceRegistry::~FrameGraphResourceRegistry() {
    cleanup();
}

bool FrameGraphResourceRegistry::initialize(FrameGraph* frameGraph, GPUEntityManager* gpuEntityManager) {
    this->frameGraph = frameGraph;
    this->gpuEntityManager = gpuEntityManager;
    return true;
}

void FrameGraphResourceRegistry::cleanup() {
    // Dependencies are managed externally
}

bool FrameGraphResourceRegistry::importEntityResources() {
    if (!frameGraph || !gpuEntityManager) {
        std::cerr << "FrameGraphResourceRegistry: Invalid dependencies" << std::endl;
        return false;
    }

    // Import entity buffer
    entityBufferId = frameGraph->importExternalBuffer(
        "EntityBuffer",
        gpuEntityManager->getEntityBuffer(),
        gpuEntityManager->getMaxEntities() * sizeof(GPUEntity),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );

    // Import position buffer
    positionBufferId = frameGraph->importExternalBuffer(
        "PositionBuffer",
        gpuEntityManager->getPositionBuffer(),
        gpuEntityManager->getMaxEntities() * sizeof(glm::vec4),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );

    // Import current position buffer
    currentPositionBufferId = frameGraph->importExternalBuffer(
        "CurrentPositionBuffer",
        gpuEntityManager->getCurrentPositionBuffer(),
        gpuEntityManager->getMaxEntities() * sizeof(glm::vec4),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    );

    // Import target position buffer
    targetPositionBufferId = frameGraph->importExternalBuffer(
        "TargetPositionBuffer",
        gpuEntityManager->getTargetPositionBuffer(),
        gpuEntityManager->getMaxEntities() * sizeof(glm::vec4),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    );

    return true;
}
#pragma once

#include "frame_graph.h"

// Forward declarations
class FrameGraph;
class GPUEntityManager;

class FrameGraphResourceRegistry {
public:
    FrameGraphResourceRegistry();
    ~FrameGraphResourceRegistry();

    bool initialize(FrameGraph* frameGraph, GPUEntityManager* gpuEntityManager);

    // Import all entity-related resources into frame graph
    bool importEntityResources();

    // Getters for resource IDs
    FrameGraphTypes::ResourceId getEntityBufferId() const { return entityBufferId; }
    FrameGraphTypes::ResourceId getPositionBufferId() const { return positionBufferId; }
    FrameGraphTypes::ResourceId getCurrentPositionBufferId() const { return currentPositionBufferId; }
    FrameGraphTypes::ResourceId getTargetPositionBufferId() const { return targetPositionBufferId; }

private:
    // Dependencies
    FrameGraph* frameGraph = nullptr;
    GPUEntityManager* gpuEntityManager = nullptr;

    // Resource IDs
    FrameGraphTypes::ResourceId entityBufferId = 0;
    FrameGraphTypes::ResourceId positionBufferId = 0;
    FrameGraphTypes::ResourceId currentPositionBufferId = 0;
    FrameGraphTypes::ResourceId targetPositionBufferId = 0;
};
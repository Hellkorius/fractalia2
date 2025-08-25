#pragma once

#include <cstdint>

namespace FrameGraphTypes {
    using ResourceId = uint32_t;
    using NodeId = uint32_t;
    
    constexpr ResourceId INVALID_RESOURCE = 0;
    constexpr NodeId INVALID_NODE = 0;
}

// Resource access patterns for dependency tracking
enum class ResourceAccess {
    Read,
    Write,
    ReadWrite
};

// Pipeline stages for synchronization
enum class PipelineStage {
    ComputeShader,
    VertexShader,
    FragmentShader,
    ColorAttachment,
    DepthAttachment,
    Transfer
};

// Resource classification for allocation strategies
enum class ResourceCriticality {
    Critical,    // Must be device local, fail fast if not possible
    Important,   // Prefer device local, allow limited fallback
    Flexible     // Accept any memory type for allocation success
};

// Resource dependency descriptor
struct ResourceDependency {
    FrameGraphTypes::ResourceId resourceId;
    ResourceAccess access;
    PipelineStage stage;
};

// Unified push constants for all frame graph compute nodes
struct NodePushConstants {
    float time;
    float deltaTime;
    uint32_t entityCount;
    uint32_t frame;
    uint32_t param1;        // Flexible parameter - entityOffset for physics, globalFrame for entity
    uint32_t param2;        // Future expansion
    float gravityStrength;  // Gravity acceleration for physics nodes
    float restitution;      // Bounce factor for floor collisions
    float friction;         // Surface friction coefficient
    uint32_t padding;       // Maintain 16-byte alignment
};
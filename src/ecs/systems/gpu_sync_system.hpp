#pragma once

#include "../component.hpp"
#include <flecs.h>

// Forward declarations
class VulkanRenderer;

// Flecs-native GPU synchronization system that replaces manual GPUEntityUpload
namespace GPUSyncSystem {
    
    // Initialize the GPU sync system
    void initialize(flecs::world& world, VulkanRenderer* renderer);
    
    // GPU entity upload system - runs in PostRender phase to maintain GPU fence timing
    void gpuEntityUploadSystem(flecs::entity e, GPUEntitySync& gpuSync, const ApplicationState& appState);
    
}
#pragma once

#include <flecs.h>
#include "component.h"
#include <cstdint>

using EntityHandle = flecs::entity;

// GPU Entity structure for CPU->GPU synchronization (separate from ECS entities)
struct GPUEntityData {
    uint32_t id{0};
    Transform transform{};
    Renderable renderable{};
    MovementPattern movement{};
    
    // Default constructor
    GPUEntityData() = default;
    
    // Constructor from Flecs entity
    explicit GPUEntityData(flecs::entity e) : id(static_cast<uint32_t>(e.id())) {
        if (e.has<Transform>()) {
            transform = *e.get<Transform>();
        }
        if (e.has<Renderable>()) {
            renderable = *e.get<Renderable>();
        }
        if (e.has<MovementPattern>()) {
            movement = *e.get<MovementPattern>();
        }
    }
};
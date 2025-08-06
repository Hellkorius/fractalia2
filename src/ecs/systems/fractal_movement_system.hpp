#pragma once

#include "../component.hpp"
#include <flecs.h>

// Beautiful fractal movement system
void fractal_movement_system(flecs::entity e, Transform& transform, MovementPattern& pattern);

// Velocity-based movement for entities without patterns
void velocity_movement_system(flecs::entity e, Transform& transform, Velocity& velocity);

// Reset per-frame caches - call once at the beginning of each frame
void reset_movement_frame_cache();
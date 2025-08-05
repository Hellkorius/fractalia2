#pragma once

#include "../component.hpp"
#include <flecs.h>

void movement_system(flecs::entity e, Position& pos, Velocity& vel);
void rotation_system(flecs::entity e, Position& pos, Rotation& rot);
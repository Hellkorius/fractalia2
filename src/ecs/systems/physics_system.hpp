#pragma once

#include "../component.hpp"
#include <flecs.h>

void movement_system(flecs::entity e, Transform& transform, Velocity& vel);
void lifetime_system(flecs::entity e, Lifetime& lifetime);
#pragma once

#include "../component.hpp"
#include <flecs.h>

void lifetime_system(flecs::entity e, Lifetime& lifetime);
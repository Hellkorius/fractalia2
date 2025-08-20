#pragma once

#include "../components/component.h"
#include <flecs.h>

void lifetime_system(flecs::entity e, Lifetime& lifetime);
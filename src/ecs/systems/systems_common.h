#pragma once

// Common headers used across multiple system files
#include <iostream>
#include <chrono>
#include <flecs.h>
#include "../components/component.h"
#include "../utilities/constants.h"
#include "../utilities/debug.h"
#include "../../vulkan_renderer.h"
#include "../core/entity_factory.h"
#include "../gpu_entity_manager.h"
#include "../movement_command_system.h"
#include "../utilities/profiler.h"
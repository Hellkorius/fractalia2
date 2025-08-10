#pragma once

#include <flecs.h>
#include "entity.hpp"
#include "component.hpp"

// Minimal system utilities - no complex hierarchy
// Use direct Flecs system registration: world.system<Components>().each(func).child_of(phase)

// Helper function for system performance monitoring (optional)
namespace SystemUtils {
    
    // Simple system timing helper if needed
    inline void logSystemTime(const std::string& systemName, double timeMs) {
        // Could add timing logic here if desired
        (void)systemName; (void)timeMs; // Suppress unused warnings for now
    }
    
}
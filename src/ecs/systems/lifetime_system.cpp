#include "../component.hpp"
#include <flecs.h>

void lifetime_system(flecs::entity e, Lifetime& lifetime) {
    const float deltaTime = e.world().delta_time();
    
    if (lifetime.maxAge > 0.0f) {
        lifetime.currentAge += deltaTime;
        
        if (lifetime.autoDestroy && lifetime.currentAge >= lifetime.maxAge) {
            e.destruct();
        }
    }
}
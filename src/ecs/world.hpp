#pragma once

#include <flecs.h>
#include "entity.hpp"
#include "component.hpp"

class World {
private:
    flecs::world flecsWorld;

public:
    World() = default;
    ~World() = default;

    // Wrapper methods for flecs::world functionality
    Entity entity() {
        return flecsWorld.entity();
    }

    template<typename... Components>
    auto system() {
        return flecsWorld.system<Components...>();
    }

    template<typename... Components>
    auto query() {
        return flecsWorld.query<Components...>();
    }

    void progress(float deltaTime) {
        flecsWorld.progress(deltaTime);
    }

    // Direct access to flecs world for advanced usage
    flecs::world& getFlecsWorld() {
        return flecsWorld;
    }

    const flecs::world& getFlecsWorld() const {
        return flecsWorld;
    }
};
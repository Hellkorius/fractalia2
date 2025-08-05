#pragma once

#include <flecs.h>
#include "entity.hpp"
#include "component.hpp"

class SystemBase {
public:
    virtual ~SystemBase() = default;
    virtual void initialize(flecs::world& world) = 0;
};

template<typename... Components>
class System : public SystemBase {
public:
    using SystemFunction = void(*)(flecs::entity, Components&...);

    System(SystemFunction func) : systemFunc(func) {}

    void initialize(flecs::world& world) override {
        world.system<Components...>().each(systemFunc);
    }

private:
    SystemFunction systemFunc;
};
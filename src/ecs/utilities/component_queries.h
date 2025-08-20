#pragma once

#include <flecs.h>
#include "../components/component.h"
#include <functional>
#include <vector>
#include <type_traits>

namespace ComponentQueries {

template<typename... Components>
class Query {
private:
    flecs::query<Components...> query_;
    
public:
    explicit Query(flecs::world& world) 
        : query_(world.query<Components...>()) {}
    
    template<typename Func>
    void each(Func&& func) {
        query_.each(std::forward<Func>(func));
    }
    
    template<typename Func>
    void iter(Func&& func) {
        query_.iter(std::forward<Func>(func));
    }
    
    size_t count() const {
        return query_.count();
    }
    
    bool empty() const {
        return count() == 0;
    }
    
    flecs::entity first() const {
        flecs::entity result;
        query_.each([&result](flecs::entity e, const Components&...) {
            if (!result.is_valid()) {
                result = e;
            }
        });
        return result;
    }
};

template<typename... Components>
Query<Components...> makeQuery(flecs::world& world) {
    return Query<Components...>(world);
}

class CachedQuery {
private:
    flecs::query_base query_;
    mutable std::vector<flecs::entity> cached_entities_;
    mutable bool cache_dirty_ = true;
    
public:
    template<typename... Components>
    CachedQuery(flecs::world& world) 
        : query_(world.query<Components...>()) {
        query_.observe(flecs::OnSet, [this](flecs::iter& it) {
            cache_dirty_ = true;
        });
    }
    
    const std::vector<flecs::entity>& getEntities() const {
        if (cache_dirty_) {
            cached_entities_.clear();
            query_.each([this](flecs::entity e) {
                cached_entities_.push_back(e);
            });
            cache_dirty_ = false;
        }
        return cached_entities_;
    }
    
    size_t count() const {
        return getEntities().size();
    }
    
    void invalidateCache() {
        cache_dirty_ = true;
    }
};

namespace CommonQueries {
    inline auto renderableEntities(flecs::world& world) {
        return makeQuery<Transform, Renderable>(world);
    }
    
    inline auto movingEntities(flecs::world& world) {
        return makeQuery<Transform, MovementPattern>(world);
    }
    
    inline auto physicsEntities(flecs::world& world) {
        return makeQuery<Transform, Velocity>(world);
    }
    
    inline auto inputEntities(flecs::world& world) {
        return makeQuery<KeyboardControlled>(world);
    }
    
    inline auto dynamicEntities(flecs::world& world) {
        return makeQuery<Transform, Dynamic>(world);
    }
    
    inline auto staticEntities(flecs::world& world) {
        return makeQuery<Transform, Static>(world);
    }
    
    inline auto cameraEntities(flecs::world& world) {
        return makeQuery<Camera>(world);
    }
    
    inline auto entitiesWithLifetime(flecs::world& world) {
        return makeQuery<Lifetime>(world);
    }
    
    inline auto visibleEntities(flecs::world& world) {
        return world.query_builder<Transform, Renderable>()
            .with<Renderable>([](Renderable& r) { return r.visible; })
            .build();
    }
    
    template<typename TagType>
    inline auto entitiesWithTag(flecs::world& world) {
        return makeQuery<TagType>(world);
    }
}

template<typename ComponentType>
class ComponentView {
private:
    flecs::world& world_;
    
public:
    explicit ComponentView(flecs::world& world) : world_(world) {}
    
    ComponentType* get(flecs::entity entity) const {
        return entity.get_mut<ComponentType>();
    }
    
    const ComponentType* getReadOnly(flecs::entity entity) const {
        return entity.get<ComponentType>();
    }
    
    bool has(flecs::entity entity) const {
        return entity.has<ComponentType>();
    }
    
    void set(flecs::entity entity, const ComponentType& component) {
        entity.set<ComponentType>(component);
    }
    
    void remove(flecs::entity entity) {
        entity.remove<ComponentType>();
    }
    
    template<typename Func>
    void forEach(Func&& func) {
        world_.each<ComponentType>(std::forward<Func>(func));
    }
    
    size_t count() const {
        return world_.count<ComponentType>();
    }
};

template<typename ComponentType>
ComponentView<ComponentType> getComponentView(flecs::world& world) {
    return ComponentView<ComponentType>(world);
}

class EntityFilter {
private:
    std::function<bool(flecs::entity)> predicate_;
    
public:
    template<typename Func>
    explicit EntityFilter(Func&& func) : predicate_(std::forward<Func>(func)) {}
    
    bool operator()(flecs::entity entity) const {
        return predicate_(entity);
    }
    
    template<typename... Components>
    std::vector<flecs::entity> apply(Query<Components...>& query) {
        std::vector<flecs::entity> result;
        query.each([this, &result](flecs::entity e, const Components&...) {
            if (predicate_(e)) {
                result.push_back(e);
            }
        });
        return result;
    }
};

template<typename Func>
EntityFilter makeFilter(Func&& func) {
    return EntityFilter(std::forward<Func>(func));
}

namespace Filters {
    inline auto byName(const std::string& name) {
        return makeFilter([name](flecs::entity e) {
            return e.name() == name;
        });
    }
    
    inline auto byPosition(const glm::vec3& center, float radius) {
        return makeFilter([center, radius](flecs::entity e) {
            if (auto* transform = e.get<Transform>()) {
                float distance = glm::length(transform->position - center);
                return distance <= radius;
            }
            return false;
        });
    }
    
    inline auto byLayer(uint32_t layer) {
        return makeFilter([layer](flecs::entity e) {
            if (auto* renderable = e.get<Renderable>()) {
                return renderable->layer == layer;
            }
            return false;
        });
    }
    
    inline auto isVisible() {
        return makeFilter([](flecs::entity e) {
            if (auto* renderable = e.get<Renderable>()) {
                return renderable->visible;
            }
            return false;
        });
    }
    
    inline auto hasMovement() {
        return makeFilter([](flecs::entity e) {
            return e.has<MovementPattern>();
        });
    }
}

} // namespace ComponentQueries
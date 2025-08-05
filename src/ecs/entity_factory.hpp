#pragma once

#include "entity.hpp"
#include "component.hpp"
#include <memory>
#include <functional>
#include <vector>
#include <random>

// Entity builder pattern for designer-friendly workflow
class EntityBuilder {
private:
    Entity entity;
    
public:
    EntityBuilder(Entity e) : entity(e) {}
    
    // Transform methods
    EntityBuilder& at(const glm::vec3& position) {
        if (auto* transform = entity.get_mut<Transform>()) {
            transform->setPosition(position);
        } else {
            entity.set<Transform>({.position = position});
        }
        return *this;
    }
    
    EntityBuilder& at(float x, float y, float z = 0.0f) {
        return at(glm::vec3(x, y, z));
    }
    
    EntityBuilder& rotated(const glm::vec3& rotation) {
        if (auto* transform = entity.get_mut<Transform>()) {
            transform->setRotation(rotation);
        } else {
            auto t = entity.get_mut<Transform>();
            if (!t) {
                entity.set<Transform>({});
                t = entity.get_mut<Transform>();
            }
            t->setRotation(rotation);
        }
        return *this;
    }
    
    EntityBuilder& scaled(const glm::vec3& scale) {
        if (auto* transform = entity.get_mut<Transform>()) {
            transform->setScale(scale);
        } else {
            auto t = entity.get_mut<Transform>();
            if (!t) {
                entity.set<Transform>({});
                t = entity.get_mut<Transform>();
            }
            t->setScale(scale);
        }
        return *this;
    }
    
    // Rendering methods
    EntityBuilder& withShape(Renderable::ShapeType shape) {
        if (auto* renderable = entity.get_mut<Renderable>()) {
            renderable->shape = shape;
            renderable->markDirty();
        } else {
            entity.set<Renderable>({.shape = shape});
        }
        return *this;
    }
    
    EntityBuilder& withColor(const glm::vec4& color) {
        if (auto* renderable = entity.get_mut<Renderable>()) {
            renderable->color = color;
            renderable->markDirty();
        } else {
            auto r = entity.get_mut<Renderable>();
            if (!r) {
                entity.set<Renderable>({});
                r = entity.get_mut<Renderable>();
            }
            r->color = color;
        }
        return *this;
    }
    
    EntityBuilder& withColor(float r, float g, float b, float a = 1.0f) {
        return withColor(glm::vec4(r, g, b, a));
    }
    
    EntityBuilder& onLayer(uint32_t layer) {
        if (auto* renderable = entity.get_mut<Renderable>()) {
            renderable->layer = layer;
            renderable->markDirty();
        } else {
            auto r = entity.get_mut<Renderable>();
            if (!r) {
                entity.set<Renderable>({});
                r = entity.get_mut<Renderable>();
            }
            r->layer = layer;
        }
        return *this;
    }
    
    // Physics methods
    EntityBuilder& withVelocity(const glm::vec3& linear, const glm::vec3& angular = glm::vec3(0.0f)) {
        entity.set<Velocity>({.linear = linear, .angular = angular});
        return *this;
    }
    
    EntityBuilder& withBounds(const glm::vec3& min, const glm::vec3& max) {
        entity.set<Bounds>({.min = min, .max = max});
        return *this;
    }
    
    // Lifetime methods
    EntityBuilder& withLifetime(float maxAge, bool autoDestroy = true) {
        entity.set<Lifetime>({.maxAge = maxAge, .autoDestroy = autoDestroy});
        return *this;
    }
    
    // Tag methods
    EntityBuilder& asStatic() {
        entity.add<Static>();
        entity.remove<Dynamic>();
        return *this;
    }
    
    EntityBuilder& asDynamic() {
        entity.add<Dynamic>();
        entity.remove<Static>();
        return *this;
    }
    
    EntityBuilder& asPooled() {
        entity.add<Pooled>();
        return *this;
    }
    
    // Finish building
    Entity build() {
        return entity;
    }
};

// Entity factory with pooling support
class EntityFactory {
private:
    flecs::world& world;
    std::vector<Entity> entityPool;
    std::mt19937 rng{std::random_device{}()};
    
public:
    EntityFactory(flecs::world& w) : world(w) {
        entityPool.reserve(1000); // Pre-allocate pool
    }
    
    // Create new entity or reuse from pool
    EntityBuilder create() {
        Entity entity;
        
        // Try to reuse pooled entity
        if (!entityPool.empty()) {
            entity = entityPool.back();
            entityPool.pop_back();
            
            // Clear existing components
            entity.clear();
        } else {
            entity = world.entity();
        }
        
        return EntityBuilder(entity);
    }
    
    // Return entity to pool for reuse
    void recycle(Entity entity) {
        if (entity.is_valid() && entity.has<Pooled>()) {
            // Clear components but keep entity alive
            entity.clear();
            entityPool.push_back(entity);
        }
    }
    
    // Batch creation methods for performance
    std::vector<Entity> createBatch(size_t count, std::function<void(EntityBuilder&, size_t)> configure) {
        std::vector<Entity> entities;
        entities.reserve(count);
        
        for (size_t i = 0; i < count; ++i) {
            auto builder = create();
            if (configure) {
                configure(builder, i);
            }
            entities.push_back(builder.build());
        }
        
        return entities;
    }
    
    // Predefined entity types for common use cases
    Entity createTriangle(const glm::vec3& pos, const glm::vec4& color, uint32_t layer = 0) {
        return create()
            .at(pos)
            .withShape(Renderable::ShapeType::Triangle)
            .withColor(color)
            .onLayer(layer)
            .asDynamic()
            .build();
    }
    
    Entity createSquare(const glm::vec3& pos, const glm::vec4& color, uint32_t layer = 0) {
        return create()
            .at(pos)
            .withShape(Renderable::ShapeType::Square)
            .withColor(color)
            .onLayer(layer)
            .asDynamic()
            .build();
    }
    
    // Create swarm of entities for stress testing
    std::vector<Entity> createSwarm(size_t count, const glm::vec3& center, float radius) {
        std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * M_PI);
        std::uniform_real_distribution<float> radiusDist(0.0f, radius);
        std::uniform_real_distribution<float> colorDist(0.0f, 1.0f);
        std::uniform_int_distribution<int> shapeDist(0, 1);
        
        return createBatch(count, [&](EntityBuilder& builder, size_t i) {
            float angle = angleDist(rng);
            float r = radiusDist(rng);
            glm::vec3 pos = center + glm::vec3(
                r * std::cos(angle),
                r * std::sin(angle),
                0.0f
            );
            
            Renderable::ShapeType shape = shapeDist(rng) == 0 ? 
                Renderable::ShapeType::Triangle : Renderable::ShapeType::Square;
                
            glm::vec4 color(colorDist(rng), colorDist(rng), colorDist(rng), 1.0f);
            
            builder.at(pos)
                   .withShape(shape)
                   .withColor(color)
                   .withVelocity(glm::vec3(
                       (colorDist(rng) - 0.5f) * 2.0f,
                       (colorDist(rng) - 0.5f) * 2.0f,
                       0.0f
                   ))
                   .asDynamic()
                   .asPooled();
        });
    }
    
    // Cleanup pool
    void clearPool() {
        for (auto& entity : entityPool) {
            if (entity.is_valid()) {
                entity.destruct();
            }
        }
        entityPool.clear();
    }
};
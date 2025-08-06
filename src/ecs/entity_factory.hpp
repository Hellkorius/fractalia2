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
    
    // Helper function to generate beautiful fractal colors
    glm::vec4 generateFractalColor(int movementType, size_t index, size_t totalCount, const glm::vec3& pos) {
        float t = static_cast<float>(index) / static_cast<float>(totalCount);
        (void)pos; // Suppress unused parameter warning
        
        glm::vec3 color;
        switch (movementType) {
            case 0: // Linear - Blue to cyan gradient
                color = glm::mix(glm::vec3(0.2f, 0.6f, 1.0f), glm::vec3(0.0f, 1.0f, 1.0f), t);
                break;
            case 1: // Orbital - Green spiral
                color = glm::vec3(0.2f + 0.6f * std::sin(t * 6.28f), 0.8f, 0.3f + 0.4f * std::cos(t * 6.28f));
                break;
            case 2: // Spiral - Golden ratio colors
                color = glm::vec3(1.0f, 0.618f * (1.0f - t), 0.382f + 0.618f * t);
                break;
            case 3: // Lissajous - Purple to pink
                color = glm::mix(glm::vec3(0.8f, 0.2f, 1.0f), glm::vec3(1.0f, 0.4f, 0.8f), t);
                break;
            case 4: // Brownian - Earth tones
                color = glm::vec3(0.6f + 0.4f * t, 0.4f + 0.3f * t, 0.2f + 0.2f * t);
                break;
            case 5: // Fractal - Rainbow spectrum
                color = glm::vec3(
                    0.5f + 0.5f * std::sin(t * 6.28f),
                    0.5f + 0.5f * std::sin(t * 6.28f + 2.09f),
                    0.5f + 0.5f * std::sin(t * 6.28f + 4.19f)
                );
                break;
            case 6: // Wave - Ocean blues
                color = glm::vec3(0.1f + 0.3f * t, 0.4f + 0.4f * t, 0.8f);
                break;
            case 7: // Petal - Flower colors
                color = glm::mix(glm::vec3(1.0f, 0.7f, 0.8f), glm::vec3(0.9f, 0.3f, 0.6f), t);
                break;
            case 8: // Butterfly - Iridescent
                color = glm::vec3(
                    0.3f + 0.7f * std::abs(std::sin(t * 12.56f)),
                    0.5f + 0.5f * std::abs(std::cos(t * 8.37f)),
                    0.8f + 0.2f * std::abs(std::sin(t * 15.71f))
                );
                break;
            default:
                color = glm::vec3(0.7f, 0.7f, 0.9f);
        }
        
        return glm::vec4(color, 1.0f);
    }
    
    // Helper function to create fractal movement patterns
    MovementPattern createFractalPattern(MovementType type, const glm::vec3& center, float radius, size_t index, size_t totalCount) {
        MovementPattern pattern;
        pattern.type = type;
        // Don't set center here - let each entity use its own starting position
        
        // Create variation based on entity index
        float t = static_cast<float>(index) / static_cast<float>(totalCount);
        float goldenRatio = 1.618033988749895f;
        
        // More individualized parameters for character
        pattern.amplitude = 0.2f + 0.6f * std::fmod(t * goldenRatio * 3.0f, 1.0f); // Varied amplitude
        pattern.frequency = 0.3f + 1.5f * std::fmod(t * goldenRatio * 7.0f, 1.0f); // Varied speed
        pattern.phase = t * 6.28318530718f * 4.0f; // More phase variation
        pattern.timeOffset = t * 20.0f; // Stagger timing more
        
        // Pattern-specific customization
        switch (type) {
            case MovementType::Spiral:
                pattern.frequency *= 0.3f; // Slower for spirals
                pattern.decay = 0.05f; // Slight decay
                break;
                
            case MovementType::Lissajous:
                pattern.lissajousRatio = glm::vec2(
                    2.0f + 3.0f * std::fmod(t * goldenRatio, 1.0f),
                    1.0f + 2.0f * std::fmod(t * goldenRatio * goldenRatio, 1.0f)
                );
                break;
                
            case MovementType::Fractal:
                pattern.recursionDepth = 2.0f + 3.0f * t;
                pattern.selfSimilarity = 0.5f + 0.3f * std::sin(t * 6.28f);
                break;
                
            case MovementType::Orbital:
                pattern.axis = glm::normalize(glm::vec3(
                    std::sin(t * 6.28f),
                    std::cos(t * 6.28f),
                    0.5f * std::sin(t * 12.56f)
                ));
                break;
                
            case MovementType::Petal:
            case MovementType::Butterfly:
                pattern.frequency *= 0.7f; // Slower for complex curves
                pattern.phaseShift = 0.1f * std::sin(t * 3.14f);
                break;
                
            default:
                break;
        }
        
        return pattern;
    }
    
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
    
    // Create a single entity with fractal movement pattern
    Entity createFractalEntity(const glm::vec3& pos, Renderable::ShapeType shape = Renderable::ShapeType::Triangle) {
        std::uniform_int_distribution<int> movementDist(0, 8); // 9 movement types
        std::uniform_int_distribution<int> shapeDist(0, 1);
        
        if (shape == Renderable::ShapeType::Triangle && shapeDist(rng) == 1) {
            shape = Renderable::ShapeType::Square;
        }
        
        int movementType = movementDist(rng);
        glm::vec4 color = generateFractalColor(movementType, 0, 1, pos);
        
        MovementPattern pattern = createFractalPattern(
            static_cast<MovementType>(movementType), 
            pos, 
            0.3f + 0.4f * std::uniform_real_distribution<float>(0.0f, 1.0f)(rng), // Random radius
            0, 
            1
        );
        
        Entity entity = create()
            .at(pos)
            .withShape(shape)
            .withColor(color)
            .asDynamic()
            .build();
            
        entity.set<MovementPattern>(pattern);
        return entity;
    }
    
    // Create swarm of entities with beautiful fractal movement patterns
    std::vector<Entity> createSwarm(size_t count, const glm::vec3& center, float radius) {
        std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * M_PI);
        std::uniform_real_distribution<float> radiusDist(0.0f, radius);
        std::uniform_real_distribution<float> colorDist(0.0f, 1.0f);
        std::uniform_real_distribution<float> paramDist(0.0f, 1.0f);
        std::uniform_int_distribution<int> shapeDist(0, 1);
        std::uniform_int_distribution<int> movementDist(0, 8); // 9 movement types
        
        return createBatch(count, [&](EntityBuilder& builder, size_t i) {
            // Spread entities more naturally across the area
            float angle = angleDist(rng);
            float r = std::sqrt(radiusDist(rng)) * radius; // Square root for better distribution
            glm::vec3 pos = center + glm::vec3(
                r * std::cos(angle),
                r * std::sin(angle),
                0.0f
            );
            
            Renderable::ShapeType shape = shapeDist(rng) == 0 ? 
                Renderable::ShapeType::Triangle : Renderable::ShapeType::Square;
            
            // Create beautiful color gradients based on position and movement type
            int movementType = movementDist(rng);
            glm::vec4 color = generateFractalColor(movementType, i, count, pos);
            
            // Create movement pattern based on type - each entity gets its own character
            float goldenRatio = 1.618033988749895f;
            MovementPattern pattern = createFractalPattern(
                static_cast<MovementType>(movementType), 
                pos, // Use entity's starting position as reference
                0.5f + 0.5f * std::fmod(i * goldenRatio, 1.0f), // Varied movement radius
                i, 
                count
            );
            
            Entity entity = builder.at(pos)
                   .withShape(shape)
                   .withColor(color)
                   .asDynamic()
                   .asPooled()
                   .build();
                   
            // Add the fractal movement pattern
            entity.set<MovementPattern>(pattern);
            
            return entity;
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
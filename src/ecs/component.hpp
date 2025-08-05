#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdint>

// Transform component - consolidates position/rotation for better cache locality
struct Transform {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 rotation{0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};
    
    // Cached transform matrix - updated when dirty
    mutable glm::mat4 matrix{1.0f};
    mutable bool dirty{true};
    
    const glm::mat4& getMatrix() const {
        if (dirty) {
            matrix = glm::translate(glm::mat4(1.0f), position) *
                    glm::rotate(glm::mat4(1.0f), rotation.z, glm::vec3(0, 0, 1)) *
                    glm::rotate(glm::mat4(1.0f), rotation.y, glm::vec3(0, 1, 0)) *
                    glm::rotate(glm::mat4(1.0f), rotation.x, glm::vec3(1, 0, 0)) *
                    glm::scale(glm::mat4(1.0f), scale);
            dirty = false;
        }
        return matrix;
    }
    
    void setPosition(const glm::vec3& pos) { position = pos; dirty = true; }
    void setRotation(const glm::vec3& rot) { rotation = rot; dirty = true; }
    void setScale(const glm::vec3& scl) { scale = scl; dirty = true; }
};

// Velocity component for physics
struct Velocity {
    glm::vec3 linear{0.0f, 0.0f, 0.0f};
    glm::vec3 angular{0.0f, 0.0f, 0.0f};
};

// Render component - optimized for batch rendering
struct Renderable {
    enum class ShapeType : uint8_t {
        Triangle = 0,
        Square = 1,
        COUNT
    };
    
    ShapeType shape{ShapeType::Triangle};
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    uint32_t layer{0}; // For depth sorting
    bool visible{true};
    
    // Change tracking for optimization
    mutable uint32_t version{0};
    void markDirty() const { ++version; }
};

// Lifetime management component
struct Lifetime {
    float maxAge{-1.0f}; // -1 = infinite
    float currentAge{0.0f};
    bool autoDestroy{false};
};

// Physics bounds for collision detection
struct Bounds {
    glm::vec3 min{-0.5f, -0.5f, -0.5f};
    glm::vec3 max{0.5f, 0.5f, 0.5f};
    bool dynamic{true}; // Updates with transform changes
};

// Tag components for efficient filtering
struct Static {}; // Non-moving entities
struct Dynamic {}; // Moving entities  
struct Pooled {}; // Can be recycled

// Backward compatibility aliases
using Position = Transform;
using Color = Renderable;
using Shape = Renderable;
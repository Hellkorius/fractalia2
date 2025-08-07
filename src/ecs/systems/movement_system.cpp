#include "../component.hpp"
#include <flecs.h>
#include <glm/glm.hpp>
#include <cmath>

// Simple constants for petal movement
namespace MovementConstants {
    static constexpr float TWO_PI = 2.0f * M_PI;
    static constexpr float PI = M_PI;
}

// Simple dynamic color generation
glm::vec4 generateDynamicColor(const MovementPattern& pattern, float currentTime) {
    // Simple rainbow color based on time and pattern properties
    float hue = std::fmod(pattern.phase + currentTime * 0.5f, MovementConstants::TWO_PI) / MovementConstants::TWO_PI;
    float saturation = 0.7f + 0.3f * std::sin(pattern.frequency * currentTime);
    float brightness = 0.8f + 0.2f * std::sin(pattern.amplitude * currentTime * 0.5f);
    
    // Convert HSV to RGB
    float c = brightness * saturation;
    float x = c * (1.0f - std::abs(std::fmod(hue * 6.0f, 2.0f) - 1.0f));
    float m = brightness - c;
    
    glm::vec3 rgb;
    if (hue < 1.0f/6.0f) rgb = {c, x, 0};
    else if (hue < 2.0f/6.0f) rgb = {x, c, 0};
    else if (hue < 3.0f/6.0f) rgb = {0, c, x};
    else if (hue < 4.0f/6.0f) rgb = {0, x, c};
    else if (hue < 5.0f/6.0f) rgb = {x, 0, c};
    else rgb = {c, 0, x};
    
    return glm::vec4(rgb + m, 1.0f);
}

// Simple petal movement system
void movement_system(flecs::entity e, Transform& transform, MovementPattern& pattern) {
    const float deltaTime = e.world().delta_time();
    pattern.totalTime += deltaTime;
    
    // Initialize starting position and center
    if (!pattern.initialized) {
        pattern.center = transform.position;
        pattern.initialized = true;
    }
    
    const float t = pattern.totalTime + pattern.timeOffset;
    
    // Simple petal movement: use a different approach - smooth oscillating radius
    // This mimics more closely how the original fractal patterns worked
    const float radialOscillation = 0.5f + 0.5f * std::sin(t * pattern.frequency * 0.3f); // 0 to 1, smooth
    const float currentRadius = pattern.amplitude * radialOscillation;
    
    // Petal angle - rotate around the center
    const float petalAngle = t * pattern.frequency + pattern.phase;
    
    // Calculate new position directly (no smoothing to avoid jitter)
    const glm::vec3 newPosition = pattern.center + glm::vec3(
        currentRadius * std::cos(petalAngle),
        currentRadius * std::sin(petalAngle),
        0.0f
    );
    
    transform.setPosition(newPosition);
    
    // Update color based on radial position
    if (auto* renderable = e.get_mut<Renderable>()) {
        glm::vec4 newColor = generateDynamicColor(pattern, pattern.totalTime);
        // Only mark dirty if color actually changed significantly to reduce GPU command buffer pressure
        if (glm::distance(renderable->color, newColor) > 0.001f) {
            renderable->color = newColor;
            renderable->markDirty();
        }
    }
}

// System to apply velocity-based movement (for entities without MovementPattern)
void velocity_movement_system(flecs::entity e, Transform& transform, Velocity& velocity) {
    if (e.has<MovementPattern>()) return;
    
    const float deltaTime = e.world().delta_time();
    
    glm::vec3 newPos = transform.position + velocity.linear * deltaTime;
    transform.setPosition(newPos);
    
    // Apply angular velocity to rotation
    if (glm::length(velocity.angular) > 0.0f) {
        glm::vec3 newRot = transform.rotation + velocity.angular * deltaTime;
        transform.setRotation(newRot);
    }
}
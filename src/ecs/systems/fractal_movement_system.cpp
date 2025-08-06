#include "../component.hpp"
#include <flecs.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <random>

// Beautiful fractal movement system with various pattern types
void fractal_movement_system(flecs::entity e, Transform& transform, MovementPattern& pattern) {
    const float deltaTime = e.world().delta_time();
    pattern.totalTime += deltaTime;
    
    // Initialize starting position and create unique center for each entity
    if (!pattern.initialized) {
        pattern.lastPosition = transform.position;
        // Give each entity its own movement center based on starting position
        pattern.center = transform.position;
        pattern.initialized = true;
    }
    
    glm::vec3 newPosition = transform.position;
    float t = pattern.totalTime + pattern.timeOffset;
    float currentAmplitude = pattern.amplitude * (1.0f - pattern.decay * pattern.totalTime);
    
    switch (pattern.type) {
        case MovementType::Linear: {
            // Smooth directional movement with gentle curves
            float directionAngle = pattern.phase + t * 0.1f;
            glm::vec3 direction = glm::vec3(
                std::cos(directionAngle),
                std::sin(directionAngle),
                0.0f
            );
            newPosition += direction * pattern.frequency * deltaTime;
            break;
        }
        
        case MovementType::Orbital: {
            // Smooth orbital movement around entity's own center
            float angle = t * pattern.frequency + pattern.phase;
            glm::vec3 offset = glm::vec3(
                currentAmplitude * std::cos(angle),
                currentAmplitude * 0.8f * std::sin(angle),
                0.0f
            );
            newPosition = pattern.center + offset;
            break;
        }
        
        case MovementType::Spiral: {
            // Gentle expanding/contracting spiral
            float angle = t * pattern.frequency + pattern.phase;
            float radiusVariation = 1.0f + 0.3f * std::sin(t * 0.2f); // Gentle breathing
            float radius = currentAmplitude * radiusVariation;
            newPosition = pattern.center + glm::vec3(
                radius * std::cos(angle),
                radius * std::sin(angle),
                0.0f
            );
            break;
        }
        
        case MovementType::Lissajous: {
            // Smooth figure-8 and complex patterns
            float x_freq = pattern.lissajousRatio.x;
            float y_freq = pattern.lissajousRatio.y;
            newPosition = pattern.center + glm::vec3(
                currentAmplitude * std::sin(x_freq * t + pattern.phase),
                currentAmplitude * std::sin(y_freq * t + pattern.phase + glm::pi<float>() / 4.0f),
                0.0f
            );
            break;
        }
        
        case MovementType::Brownian: {
            // Smooth wandering motion with gentle direction changes
            static std::random_device rd;
            static std::mt19937 gen(rd());
            std::uniform_real_distribution<float> walk(-0.3f, 0.3f);
            
            // Update direction smoothly over time
            pattern.phase += walk(gen) * deltaTime;
            glm::vec3 direction = glm::vec3(
                std::cos(pattern.phase),
                std::sin(pattern.phase),
                0.0f
            );
            newPosition += direction * pattern.frequency * deltaTime * currentAmplitude;
            break;
        }
        
        case MovementType::Fractal: {
            // Layered smooth movement with multiple frequencies
            float baseFreq = pattern.frequency * 0.5f; // Slower base frequency
            glm::vec3 fractalPos(0.0f);
            
            // Combine 2-3 smooth layers instead of many chaotic ones
            for (int i = 0; i < 3; ++i) {
                float scale = std::pow(0.6f, i); // Gentler scaling
                float freq = baseFreq * std::pow(1.8f, i); // Less aggressive frequency scaling
                float phase = pattern.phase + i * glm::pi<float>() / 4.0f;
                
                fractalPos += scale * glm::vec3(
                    std::sin(freq * t + phase),
                    std::cos(freq * t + phase + glm::pi<float>() / 3.0f),
                    0.0f
                );
            }
            
            newPosition = pattern.center + currentAmplitude * fractalPos;
            break;
        }
        
        case MovementType::Wave: {
            // Smooth flowing wave patterns
            float wave1 = std::sin(pattern.frequency * t + pattern.phase);
            float wave2 = std::sin(pattern.frequency * 1.414f * t + pattern.phase + glm::pi<float>() / 3.0f);
            
            newPosition = pattern.center + currentAmplitude * glm::vec3(
                wave1 * 0.8f,
                wave2 * 0.6f,
                0.0f
            );
            break;
        }
        
        case MovementType::Petal: {
            // Gentle petal-like movements
            float k = 3.0f + 2.0f * std::sin(t * 0.1f); // Varying petal count
            float angle = t * pattern.frequency + pattern.phase;
            float r = currentAmplitude * std::abs(std::cos(k * angle * 0.5f));
            
            newPosition = pattern.center + glm::vec3(
                r * std::cos(angle),
                r * std::sin(angle),
                0.0f
            );
            break;
        }
        
        case MovementType::Butterfly: {
            // Smooth butterfly-like figure-8 movement
            float bt = t * pattern.frequency + pattern.phase;
            float scale = currentAmplitude * 0.3f; // Smaller, more controlled
            
            // Simplified butterfly curve that's smoother
            float x = scale * std::sin(bt) * (std::exp(std::cos(bt)) - 2.0f * std::cos(4.0f * bt));
            float y = scale * std::cos(bt) * (std::exp(std::cos(bt)) - 2.0f * std::cos(4.0f * bt));
            
            newPosition = pattern.center + glm::vec3(x * 0.1f, y * 0.1f, 0.0f);
            break;
        }
    }
    
    // Apply phase shift over time for dynamic evolution
    pattern.phase += pattern.phaseShift * deltaTime;
    
    // Smoothly interpolate position for better flow
    const float smoothing = 0.85f; // More responsive but still smooth
    glm::vec3 smoothPosition = glm::mix(transform.position, newPosition, smoothing);
    
    transform.setPosition(smoothPosition);
    pattern.lastPosition = smoothPosition;
}

// System to apply velocity-based movement (for entities without MovementPattern)
void velocity_movement_system(flecs::entity e, Transform& transform, Velocity& velocity) {
    // Only apply if entity doesn't have a MovementPattern
    if (e.has<MovementPattern>()) return;
    
    const float deltaTime = e.world().delta_time();
    
    glm::vec3 newPos = transform.position;
    newPos += velocity.linear * deltaTime;
    
    transform.setPosition(newPos);
    
    // Apply angular velocity to rotation
    if (glm::length(velocity.angular) > 0.0f) {
        glm::vec3 newRot = transform.rotation + velocity.angular * deltaTime;
        transform.setRotation(newRot);
    }
}
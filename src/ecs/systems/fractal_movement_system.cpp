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
    
    // Initialize starting position if needed
    if (!pattern.initialized) {
        pattern.lastPosition = transform.position;
        pattern.initialized = true;
    }
    
    glm::vec3 newPosition = transform.position;
    float t = pattern.totalTime + pattern.timeOffset;
    float currentAmplitude = pattern.amplitude * (1.0f - pattern.decay * pattern.totalTime);
    
    switch (pattern.type) {
        case MovementType::Linear: {
            // Simple linear movement with slight randomness
            static std::random_device rd;
            static std::mt19937 gen(rd());
            std::uniform_real_distribution<float> noise(-0.01f, 0.01f);
            
            newPosition.x += pattern.frequency * deltaTime + noise(gen);
            newPosition.y += pattern.frequency * 0.7f * deltaTime + noise(gen);
            break;
        }
        
        case MovementType::Orbital: {
            // Circular/elliptical orbit around center
            float angle = t * pattern.frequency + pattern.phase;
            newPosition = pattern.center + glm::vec3(
                currentAmplitude * std::cos(angle),
                currentAmplitude * 0.7f * std::sin(angle),
                currentAmplitude * 0.3f * std::sin(angle * 0.5f)
            );
            break;
        }
        
        case MovementType::Spiral: {
            // Logarithmic spiral - classic fractal pattern
            float angle = t * pattern.frequency + pattern.phase;
            float radius = currentAmplitude * std::exp(angle * 0.1f);
            newPosition = pattern.center + glm::vec3(
                radius * std::cos(angle),
                radius * std::sin(angle),
                currentAmplitude * 0.2f * std::sin(angle * 2.0f)
            );
            break;
        }
        
        case MovementType::Lissajous: {
            // Complex periodic patterns (Lissajous curves)
            float x_freq = pattern.lissajousRatio.x;
            float y_freq = pattern.lissajousRatio.y;
            newPosition = pattern.center + glm::vec3(
                currentAmplitude * std::sin(x_freq * t + pattern.phase),
                currentAmplitude * std::sin(y_freq * t + pattern.phase + glm::pi<float>() / 4.0f),
                currentAmplitude * 0.5f * std::sin((x_freq + y_freq) * 0.5f * t)
            );
            break;
        }
        
        case MovementType::Brownian: {
            // Brownian motion (random walk)
            static std::random_device rd;
            static std::mt19937 gen(rd());
            std::uniform_real_distribution<float> walk(-1.0f, 1.0f);
            
            glm::vec3 randomStep = glm::vec3(walk(gen), walk(gen), walk(gen) * 0.3f);
            newPosition += randomStep * pattern.frequency * deltaTime * currentAmplitude;
            
            // Keep within bounds
            float maxDistance = pattern.amplitude * 2.0f;
            glm::vec3 fromCenter = newPosition - pattern.center;
            if (glm::length(fromCenter) > maxDistance) {
                newPosition = pattern.center + glm::normalize(fromCenter) * maxDistance;
            }
            break;
        }
        
        case MovementType::Fractal: {
            // Self-similar recursive pattern
            float baseFreq = pattern.frequency;
            glm::vec3 fractalPos(0.0f);
            
            for (int i = 0; i < static_cast<int>(pattern.recursionDepth); ++i) {
                float scale = std::pow(pattern.selfSimilarity, i);
                float freq = baseFreq * std::pow(2.0f, i);
                float phase = pattern.phase + i * glm::pi<float>() / 3.0f;
                
                fractalPos += scale * glm::vec3(
                    std::sin(freq * t + phase),
                    std::cos(freq * t + phase + glm::pi<float>() / 2.0f),
                    0.5f * std::sin(freq * 0.5f * t + phase)
                );
            }
            
            newPosition = pattern.center + currentAmplitude * fractalPos;
            break;
        }
        
        case MovementType::Wave: {
            // Sine wave patterns
            float wave1 = std::sin(pattern.frequency * t + pattern.phase);
            float wave2 = std::sin(pattern.frequency * 1.618f * t + pattern.phase + glm::pi<float>() / 3.0f);
            float wave3 = std::sin(pattern.frequency * 0.382f * t + pattern.phase);
            
            newPosition = pattern.center + currentAmplitude * glm::vec3(
                wave1,
                wave2,
                0.5f * wave3
            );
            break;
        }
        
        case MovementType::Petal: {
            // Rose curve (petal patterns)
            float k = 5.0f; // Number of petals
            float angle = t * pattern.frequency + pattern.phase;
            float r = currentAmplitude * std::cos(k * angle);
            
            newPosition = pattern.center + glm::vec3(
                r * std::cos(angle),
                r * std::sin(angle),
                currentAmplitude * 0.3f * std::sin(k * angle)
            );
            break;
        }
        
        case MovementType::Butterfly: {
            // Butterfly curve - incredibly beautiful!
            float scale = currentAmplitude;
            float bt = t * pattern.frequency + pattern.phase;
            
            float r = std::exp(std::cos(bt)) - 2.0f * std::cos(4.0f * bt) - std::pow(std::sin(bt / 12.0f), 5.0f);
            
            newPosition = pattern.center + scale * glm::vec3(
                r * std::cos(bt),
                r * std::sin(bt),
                scale * 0.2f * std::sin(bt * 2.0f)
            );
            break;
        }
    }
    
    // Apply phase shift over time for dynamic evolution
    pattern.phase += pattern.phaseShift * deltaTime;
    
    // Smoothly update transform
    transform.setPosition(newPosition);
    pattern.lastPosition = newPosition;
}

// System to apply velocity-based movement (for entities without MovementPattern)
void velocity_movement_system(flecs::entity e, Transform& transform, Velocity& velocity) {
    // Only apply if entity doesn't have a MovementPattern
    if (e.has<MovementPattern>()) return;
    
    const float deltaTime = e.world().delta_time();
    
    glm::vec3 newPos = transform.position;
    newPos += velocity.linear * deltaTime;
    
    // Bounce off screen edges
    if (newPos.x > 2.0f || newPos.x < -2.0f) velocity.linear.x = -velocity.linear.x;
    if (newPos.y > 1.5f || newPos.y < -1.5f) velocity.linear.y = -velocity.linear.y;
    
    transform.setPosition(newPos);
    
    // Apply angular velocity to rotation
    if (glm::length(velocity.angular) > 0.0f) {
        glm::vec3 newRot = transform.rotation + velocity.angular * deltaTime;
        transform.setRotation(newRot);
    }
}
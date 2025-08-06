#include "../component.hpp"
#include <flecs.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <random>

// Dynamic color generation based on movement parameters
glm::vec4 generateDynamicColor(const MovementPattern& pattern, float currentTime) {
    // Base hue determined by movement type for visual distinction
    float baseHue = 0.0f;
    switch (pattern.type) {
        case MovementType::Linear:    baseHue = 0.0f;   break; // Red spectrum
        case MovementType::Orbital:   baseHue = 0.15f;  break; // Orange spectrum
        case MovementType::Spiral:    baseHue = 0.33f;  break; // Green spectrum
        case MovementType::Lissajous: baseHue = 0.5f;   break; // Cyan spectrum
        case MovementType::Brownian:  baseHue = 0.6f;   break; // Blue spectrum
        case MovementType::Fractal:   baseHue = 0.75f;  break; // Purple spectrum
        case MovementType::Wave:      baseHue = 0.85f;  break; // Magenta spectrum
        case MovementType::Petal:     baseHue = 0.92f;  break; // Pink spectrum
        case MovementType::Butterfly: baseHue = 0.08f;  break; // Yellow spectrum
    }
    
    // Modulate hue based on amplitude and time for variety within each type
    float hue = std::fmod(baseHue + pattern.amplitude * 0.1f + currentTime * 0.05f, 1.0f);
    
    // Map frequency to saturation (higher frequency = more saturated)
    float saturation = 0.7f + 0.3f * glm::clamp(pattern.frequency / 2.5f, 0.0f, 1.0f);
    
    // Base brightness with subtle pulsing based on movement phase
    float brightness = 0.8f + 0.2f * std::abs(std::sin(pattern.phase + currentTime * 1.5f));
    
    // Convert HSV to RGB
    auto hsvToRgb = [](float h, float s, float v) -> glm::vec3 {
        float c = v * s;
        float x = c * (1.0f - std::abs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
        float m = v - c;
        
        glm::vec3 rgb;
        if (h < 1.0f/6.0f) rgb = {c, x, 0};
        else if (h < 2.0f/6.0f) rgb = {x, c, 0};
        else if (h < 3.0f/6.0f) rgb = {0, c, x};
        else if (h < 4.0f/6.0f) rgb = {0, x, c};
        else if (h < 5.0f/6.0f) rgb = {x, 0, c};
        else rgb = {c, 0, x};
        
        return rgb + m;
    };
    
    glm::vec3 rgb = hsvToRgb(hue, saturation, brightness);
    return glm::vec4(rgb, 1.0f);
}

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
    
    // Update color dynamically based on amplitude and frequency
    if (auto* renderable = e.get_mut<Renderable>()) {
        glm::vec4 newColor = generateDynamicColor(pattern, pattern.totalTime);
        if (renderable->color != newColor) {
            renderable->color = newColor;
            renderable->markDirty();
        }
    }
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
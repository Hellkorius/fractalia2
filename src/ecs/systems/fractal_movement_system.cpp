#include "../component.hpp"
#include <flecs.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <random>
#include <iostream>
#include <algorithm>

// Performance optimization: Fast math functions with lookup tables
namespace FastMath {
    // Pre-computed sine/cosine lookup table for common angles
    static constexpr int LUT_SIZE = 1024;
    static constexpr float LUT_SCALE = LUT_SIZE / (2.0f * M_PI);
    static float sin_lut[LUT_SIZE];
    static float cos_lut[LUT_SIZE];
    static bool lut_initialized = false;
    
    void initializeLUT() {
        if (lut_initialized) return;
        for (int i = 0; i < LUT_SIZE; ++i) {
            float angle = (2.0f * M_PI * i) / LUT_SIZE;
            sin_lut[i] = std::sin(angle);
            cos_lut[i] = std::cos(angle);
        }
        lut_initialized = true;
    }
    
    // Fast sine approximation using lookup table with linear interpolation
    inline float fastSin(float x) {
        if (!lut_initialized) initializeLUT();
        
        // Normalize to [0, 2π)
        x = std::fmod(x, 2.0f * M_PI);
        if (x < 0) x += 2.0f * M_PI;
        
        float index = x * LUT_SCALE;
        int i = static_cast<int>(index);
        float frac = index - i;
        
        int next = (i + 1) % LUT_SIZE;
        return sin_lut[i] + frac * (sin_lut[next] - sin_lut[i]);
    }
    
    // Fast cosine approximation using lookup table with linear interpolation
    inline float fastCos(float x) {
        if (!lut_initialized) initializeLUT();
        
        // Normalize to [0, 2π)
        x = std::fmod(x, 2.0f * M_PI);
        if (x < 0) x += 2.0f * M_PI;
        
        float index = x * LUT_SCALE;
        int i = static_cast<int>(index);
        float frac = index - i;
        
        int next = (i + 1) % LUT_SIZE;
        return cos_lut[i] + frac * (cos_lut[next] - cos_lut[i]);
    }
    
    // Fast approximation for small exponentials
    inline float fastExp(float x) {
        if (x > 5.0f) return std::exp(5.0f); // Clamp to prevent overflow
        if (x < -5.0f) return 0.0f;
        return std::exp(x); // For small values, std::exp is reasonable
    }
}

// Cached computational values to reduce per-frame calculations
struct MovementCache {
    static constexpr float GOLDEN_RATIO = 1.618033988749f;
    static constexpr float SQRT_3_DIV_2 = 0.866025403784f;
    static constexpr float TWO_PI = 2.0f * M_PI;
    static constexpr float PI_DIV_3 = M_PI / 3.0f;
    static constexpr float TWO_PI_DIV_3 = 2.0f * M_PI / 3.0f;
};

// Optimized dynamic color generation with reduced calculations
glm::vec4 generateDynamicColor(const MovementPattern& pattern, float currentTime) {
    // Base hue determined by sacred geometry type for spiritual distinction
    float baseHue = 0.0f;
    switch (pattern.type) {
        case MovementType::FlowerOfLife:    baseHue = 0.0f;   break; // Red - Root chakra energy
        case MovementType::SeedOfLife:      baseHue = 0.08f;  break; // Orange - Sacral chakra
        case MovementType::VesicaPiscis:    baseHue = 0.17f;  break; // Yellow - Solar plexus
        case MovementType::SriYantra:       baseHue = 0.33f;  break; // Green - Heart chakra
        case MovementType::PlatonicSolids:  baseHue = 0.5f;   break; // Cyan - Throat chakra
        case MovementType::FibonacciSpiral: baseHue = 0.67f;  break; // Blue - Third eye
        case MovementType::GoldenRatio:     baseHue = 0.75f;  break; // Purple - Crown chakra
        case MovementType::Metatron:        baseHue = 0.83f;  break; // Violet - Divine connection
        case MovementType::TreeOfLife:      baseHue = 0.92f;  break; // Magenta - Universal love
        case MovementType::TetraktysFlow:   baseHue = 0.97f;  break; // White-pink - Pure wisdom
    }
    
    // Enhanced modulation based on amplitude, frequency, and time for rich variety
    float amplitudeInfluence = pattern.amplitude / 8.0f; // Scale for new larger amplitudes
    float hue = std::fmod(baseHue + amplitudeInfluence * 0.15f + currentTime * 0.05f, 1.0f);
    
    // Map frequency and amplitude to saturation for more dynamic colors
    float saturation = 0.6f + 0.4f * glm::clamp((pattern.frequency + amplitudeInfluence) / 3.0f, 0.0f, 1.0f);
    
    // Amplitude-based brightness variation with movement pulsing
    float amplitudeBrightness = 0.3f + 0.4f * glm::clamp(amplitudeInfluence, 0.0f, 1.0f);
    float phaseBrightness = 0.3f + 0.2f * std::abs(std::sin(pattern.phase + currentTime * 1.5f));
    float brightness = amplitudeBrightness + phaseBrightness;
    
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

// Frame-level cache for expensive calculations
struct FrameCache {
    static float deltaTime;
    static float frameTime;
    static bool initialized;
    
    static void update(float dt, float ft) {
        deltaTime = dt;
        frameTime = ft;
        initialized = true;
        FastMath::initializeLUT(); // Initialize lookup tables once per frame
    }
};

// Static initialization
float FrameCache::deltaTime = 0.0f;
float FrameCache::frameTime = 0.0f;
bool FrameCache::initialized = false;

// Sacred Geometry movement system with spiritual and mathematical beauty
void fractal_movement_system(flecs::entity e, Transform& transform, MovementPattern& pattern) {
    // Use cached delta time and frame time for all entities this frame
    if (!FrameCache::initialized) {
        FrameCache::update(e.world().delta_time(), 0.0f); // Remove unused frame time
        FastMath::initializeLUT(); // Initialize lookup tables once per frame
    }
    
    const float deltaTime = FrameCache::deltaTime;
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
    
    // Optimize amplitude calculation - avoid multiplication if decay is zero
    float currentAmplitude = (pattern.decay > 0.001f) ? 
        pattern.amplitude * (1.0f - pattern.decay * pattern.totalTime) : 
        pattern.amplitude;
    
    switch (pattern.type) {
        case MovementType::FlowerOfLife: {
            // Movement along overlapping circles in the Flower of Life pattern
            float mainAngle = t * pattern.frequency + pattern.phase;
            float petalIndex = std::floor(pattern.totalTime * 0.5f) + 1; // Cycle through petals
            petalIndex = std::fmod(petalIndex, 6.0f); // 6 outer petals
            
            float petalAngle = petalIndex * MovementCache::PI_DIV_3; // 60 degrees apart
            float petalCos = FastMath::fastCos(petalAngle);
            float petalSin = FastMath::fastSin(petalAngle);
            glm::vec3 petalCenter = pattern.center + glm::vec3(
                pattern.circleRadius * petalCos,
                pattern.circleRadius * petalSin,
                0.0f
            );
            
            float mainCos = FastMath::fastCos(mainAngle);
            float mainSin = FastMath::fastSin(mainAngle);
            newPosition = petalCenter + currentAmplitude * 0.5f * glm::vec3(
                mainCos,
                mainSin,
                0.0f
            );
            break;
        }
        
        case MovementType::SeedOfLife: {
            // Movement within the seven-circle pattern of the Seed of Life
            float angle = t * pattern.frequency + pattern.phase;
            int circleIndex = static_cast<int>(pattern.totalTime * 0.3f) % 7;
            
            glm::vec3 circleCenter = pattern.center;
            if (circleIndex > 0) {
                // Outer 6 circles
                float centerAngle = (circleIndex - 1) * MovementCache::PI_DIV_3;
                float centerCos = FastMath::fastCos(centerAngle);
                float centerSin = FastMath::fastSin(centerAngle);
                circleCenter += glm::vec3(
                    pattern.circleRadius * centerCos,
                    pattern.circleRadius * centerSin,
                    0.0f
                );
            }
            
            float angleCos = FastMath::fastCos(angle);
            float angleSin = FastMath::fastSin(angle);
            newPosition = circleCenter + currentAmplitude * 0.4f * glm::vec3(
                angleCos,
                angleSin,
                0.0f
            );
            break;
        }
        
        case MovementType::VesicaPiscis: {
            // Movement in the vesica piscis (sacred lens between two circles)
            float angle = t * pattern.frequency + pattern.phase;
            float circleOffset = pattern.circleRadius * 0.5f;
            
            // Alternate between the two circles using fast approximation
            bool leftCircle = FastMath::fastSin(t * 0.5f) > 0;
            glm::vec3 centerOffset = leftCircle ? 
                glm::vec3(-circleOffset, 0.0f, 0.0f) : 
                glm::vec3(circleOffset, 0.0f, 0.0f);
            
            // Focus movement in the intersection area
            float radius = currentAmplitude * pattern.vesicaRatio;
            float angleCos = FastMath::fastCos(angle);
            float angleSin = FastMath::fastSin(angle);
            newPosition = pattern.center + centerOffset + glm::vec3(
                radius * angleCos,
                radius * angleSin,
                0.0f
            );
            break;
        }
        
        case MovementType::SriYantra: {
            // Movement following the interlocking triangles of Sri Yantra
            float angle = t * pattern.frequency + pattern.phase;
            float trianglePhase = pattern.totalTime * 0.2f;
            
            // Outer triangle points
            float triangleSize = currentAmplitude * MovementCache::SQRT_3_DIV_2;
            int trianglePoint = static_cast<int>(trianglePhase) % 3;
            
            glm::vec3 triangleCenter = pattern.center;
            float pointAngle = trianglePoint * MovementCache::TWO_PI_DIV_3;
            float pointCos = FastMath::fastCos(pointAngle);
            float pointSin = FastMath::fastSin(pointAngle);
            triangleCenter += triangleSize * glm::vec3(
                pointCos,
                pointSin,
                0.0f
            );
            
            float angleCos = FastMath::fastCos(angle);
            float angleSin = FastMath::fastSin(angle);
            newPosition = triangleCenter + currentAmplitude * 0.3f * glm::vec3(
                angleCos,
                angleSin,
                0.0f
            );
            break;
        }
        
        case MovementType::PlatonicSolids: {
            // Movement based on 2D projections of the five Platonic solids
            float angle = t * pattern.frequency + pattern.phase;
            int solidType = static_cast<int>(pattern.totalTime * 0.1f) % 5;
            
            float radius = currentAmplitude;
            glm::vec3 offset(0.0f);
            
            switch (solidType) {
                case 0: // Tetrahedron (4 vertices)
                    {
                        float tetraAngle = angle * 4.0f / 3.0f;
                        offset = radius * glm::vec3(
                            FastMath::fastCos(tetraAngle),
                            FastMath::fastSin(tetraAngle),
                            0.0f
                        );
                    }
                    break;
                case 1: // Cube/Hexahedron (8 vertices)
                    {
                        float cos1 = FastMath::fastCos(angle);
                        float sin1 = FastMath::fastSin(angle);
                        float cos2 = FastMath::fastCos(angle * 2.0f);
                        float sin2 = FastMath::fastSin(angle * 2.0f);
                        offset = radius * glm::vec3(
                            cos1 + 0.3f * cos2,
                            sin1 + 0.3f * sin2,
                            0.0f
                        );
                    }
                    break;
                case 2: // Octahedron (6 vertices)
                    {
                        float octaAngle = angle * 1.5f;
                        offset = radius * glm::vec3(
                            FastMath::fastCos(octaAngle),
                            FastMath::fastSin(octaAngle),
                            0.0f
                        );
                    }
                    break;
                case 3: // Dodecahedron (20 vertices)
                    {
                        float dodecaAngle = angle * MovementCache::GOLDEN_RATIO;
                        offset = radius * glm::vec3(
                            FastMath::fastCos(dodecaAngle),
                            FastMath::fastSin(dodecaAngle),
                            0.0f
                        );
                    }
                    break;
                case 4: // Icosahedron (12 vertices)
                    {
                        float icosaAngle = angle * 1.2f;
                        offset = radius * glm::vec3(
                            FastMath::fastCos(icosaAngle) * MovementCache::GOLDEN_RATIO,
                            FastMath::fastSin(icosaAngle) / MovementCache::GOLDEN_RATIO,
                            0.0f
                        );
                    }
                    break;
            }
            
            newPosition = pattern.center + offset;
            break;
        }
        
        case MovementType::FibonacciSpiral: {
            // Golden spiral following Fibonacci sequence
            float spiralAngle = t * pattern.frequency + pattern.phase;
            float radius = currentAmplitude * FastMath::fastExp(spiralAngle * pattern.spiralGrowth / MovementCache::GOLDEN_RATIO);
            
            // Limit radius growth to prevent entities from going too far
            radius = std::min(radius, currentAmplitude * 3.0f);
            
            float spiralCos = FastMath::fastCos(spiralAngle);
            float spiralSin = FastMath::fastSin(spiralAngle);
            newPosition = pattern.center + glm::vec3(
                radius * spiralCos,
                radius * spiralSin,
                0.0f
            );
            break;
        }
        
        case MovementType::GoldenRatio: {
            // Patterns based on the golden ratio (phi = 1.618...)
            float angle = t * pattern.frequency + pattern.phase;
            
            // Golden spiral with ratio-based frequency
            float scaledAngle = angle * MovementCache::GOLDEN_RATIO;
            float expFactor = FastMath::fastExp(-angle * 0.1f);
            float x = currentAmplitude * FastMath::fastCos(scaledAngle) * expFactor;
            float y = currentAmplitude * FastMath::fastSin(scaledAngle) * expFactor;
            
            newPosition = pattern.center + glm::vec3(x, y, 0.0f);
            break;
        }
        
        case MovementType::Metatron: {
            // Metatron's Cube - contains all five Platonic solids
            float angle = t * pattern.frequency + pattern.phase;
            float cubePhase = pattern.totalTime * 0.15f;
            
            // 13 circles of Metatron's Cube
            int circleIndex = static_cast<int>(cubePhase) % 13;
            glm::vec3 circleCenter = pattern.center;
            
            if (circleIndex > 0) {
                float ringRadius = (circleIndex <= 6) ? pattern.circleRadius : pattern.circleRadius * 2.0f;
                float ringAngle = (circleIndex - 1) * glm::pi<float>() / 3.0f;
                if (circleIndex > 6) ringAngle = (circleIndex - 7) * glm::pi<float>() / 3.0f + glm::pi<float>() / 6.0f;
                
                circleCenter += glm::vec3(
                    ringRadius * std::cos(ringAngle),
                    ringRadius * std::sin(ringAngle),
                    0.0f
                );
            }
            
            newPosition = circleCenter + currentAmplitude * 0.25f * glm::vec3(
                std::cos(angle),
                std::sin(angle),
                0.0f
            );
            break;
        }
        
        case MovementType::TreeOfLife: {
            // Kabbalistic Tree of Life - 10 sephirot pattern
            float angle = t * pattern.frequency + pattern.phase;
            int sephira = static_cast<int>(pattern.totalTime * 0.2f) % 10;
            
            // Positions of the 10 sephirot in the Tree of Life
            glm::vec2 sephirotPositions[10] = {
                {0.0f, 1.0f},      // Keter (Crown)
                {-0.5f, 0.7f},     // Chokmah (Wisdom)
                {0.5f, 0.7f},      // Binah (Understanding)
                {-0.3f, 0.3f},     // Chesed (Mercy)
                {0.3f, 0.3f},      // Geburah (Severity)
                {0.0f, 0.0f},      // Tiferet (Beauty)
                {-0.3f, -0.3f},    // Netzach (Victory)
                {0.3f, -0.3f},     // Hod (Glory)
                {0.0f, -0.6f},     // Yesod (Foundation)
                {0.0f, -1.0f}      // Malkuth (Kingdom)
            };
            
            glm::vec3 sephiraCenter = pattern.center + currentAmplitude * glm::vec3(
                sephirotPositions[sephira].x,
                sephirotPositions[sephira].y,
                0.0f
            );
            
            newPosition = sephiraCenter + currentAmplitude * 0.15f * glm::vec3(
                std::cos(angle),
                std::sin(angle),
                0.0f
            );
            break;
        }
        
        case MovementType::TetraktysFlow: {
            // Sacred Pythagorean Tetraktys - triangular arrangement of 10 points
            float angle = t * pattern.frequency + pattern.phase;
            int pointIndex = static_cast<int>(pattern.totalTime * 0.3f) % 10;
            
            // Tetraktys points arranged in triangle (1+2+3+4 = 10 points)
            glm::vec2 tetraktysPoints[10] = {
                // Row 1 (1 point)
                {0.0f, 0.75f},
                // Row 2 (2 points)
                {-0.25f, 0.25f}, {0.25f, 0.25f},
                // Row 3 (3 points)
                {-0.5f, -0.25f}, {0.0f, -0.25f}, {0.5f, -0.25f},
                // Row 4 (4 points)
                {-0.75f, -0.75f}, {-0.25f, -0.75f}, {0.25f, -0.75f}, {0.75f, -0.75f}
            };
            
            glm::vec3 pointCenter = pattern.center + currentAmplitude * glm::vec3(
                tetraktysPoints[pointIndex].x,
                tetraktysPoints[pointIndex].y,
                0.0f
            );
            
            newPosition = pointCenter + currentAmplitude * 0.1f * glm::vec3(
                std::cos(angle),
                std::sin(angle),
                0.0f
            );
            break;
        }
    }
    
    // Apply phase shift over time for dynamic evolution
    pattern.phase += pattern.phaseShift * deltaTime;
    
    // Smoothly interpolate position for better flow
    const float smoothing = 0.85f; // More responsive but still smooth
    glm::vec3 smoothPosition = glm::mix(transform.position, newPosition, smoothing);
    
    // Validate and clamp position to prevent extreme values that cause rendering issues
    const float MAX_WORLD_POS = 500.0f; // Reasonable world bounds
    smoothPosition.x = glm::clamp(smoothPosition.x, -MAX_WORLD_POS, MAX_WORLD_POS);
    smoothPosition.y = glm::clamp(smoothPosition.y, -MAX_WORLD_POS, MAX_WORLD_POS);
    smoothPosition.z = glm::clamp(smoothPosition.z, -MAX_WORLD_POS, MAX_WORLD_POS);
    
    // Check for NaN/infinite values and reset if found
    if (!std::isfinite(smoothPosition.x) || !std::isfinite(smoothPosition.y) || !std::isfinite(smoothPosition.z)) {
        smoothPosition = pattern.center; // Reset to center if invalid
        std::cerr << "Warning: Invalid position detected, resetting entity to center" << std::endl;
    }
    
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
    
    // Validate and clamp position to prevent extreme values
    const float MAX_WORLD_POS = 500.0f;
    newPos.x = glm::clamp(newPos.x, -MAX_WORLD_POS, MAX_WORLD_POS);
    newPos.y = glm::clamp(newPos.y, -MAX_WORLD_POS, MAX_WORLD_POS);
    newPos.z = glm::clamp(newPos.z, -MAX_WORLD_POS, MAX_WORLD_POS);
    
    // Check for NaN/infinite values
    if (!std::isfinite(newPos.x) || !std::isfinite(newPos.y) || !std::isfinite(newPos.z)) {
        newPos = glm::vec3(0.0f); // Reset to origin if invalid
        velocity.linear = glm::vec3(0.0f); // Stop movement
    }
    
    transform.setPosition(newPos);
    
    // Apply angular velocity to rotation
    if (glm::length(velocity.angular) > 0.0f) {
        glm::vec3 newRot = transform.rotation + velocity.angular * deltaTime;
        transform.setRotation(newRot);
    }
}

// Reset frame cache - called once per frame for optimal performance
void reset_movement_frame_cache() {
    FrameCache::initialized = false;
}
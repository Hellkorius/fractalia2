#include <iostream>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>

// Copy of the ParticleUBO structure from particle nodes
struct ParticleUBO {
    glm::mat4 viewMatrix;        // 64 bytes
    glm::mat4 projMatrix;        // 64 bytes  
    glm::vec4 sunDirection;      // 16 bytes
    glm::vec4 sunPosition;       // 16 bytes
    glm::vec4 sceneCenter;       // 16 bytes
    float deltaTime;             // 4 bytes
    float totalTime;             // 4 bytes
    uint32_t maxParticles;       // 4 bytes
    uint32_t emissionRate;       // 4 bytes
    float particleLifetime;      // 4 bytes
    float windStrength;          // 4 bytes
    float gravityStrength;       // 4 bytes
    float sunRayLength;          // 4 bytes
}; 

int main() {
    std::cout << "ParticleUBO alignment analysis:" << std::endl;
    std::cout << "sizeof(ParticleUBO) = " << sizeof(ParticleUBO) << std::endl;
    std::cout << "alignof(ParticleUBO) = " << alignof(ParticleUBO) << std::endl;
    
    std::cout << "\nField offsets:" << std::endl;
    std::cout << "viewMatrix offset: " << offsetof(ParticleUBO, viewMatrix) << std::endl;
    std::cout << "projMatrix offset: " << offsetof(ParticleUBO, projMatrix) << std::endl;
    std::cout << "sunDirection offset: " << offsetof(ParticleUBO, sunDirection) << std::endl;
    std::cout << "sunPosition offset: " << offsetof(ParticleUBO, sunPosition) << std::endl;
    std::cout << "sceneCenter offset: " << offsetof(ParticleUBO, sceneCenter) << std::endl;
    std::cout << "deltaTime offset: " << offsetof(ParticleUBO, deltaTime) << std::endl;
    std::cout << "totalTime offset: " << offsetof(ParticleUBO, totalTime) << std::endl;
    std::cout << "maxParticles offset: " << offsetof(ParticleUBO, maxParticles) << std::endl;
    std::cout << "emissionRate offset: " << offsetof(ParticleUBO, emissionRate) << std::endl;
    std::cout << "particleLifetime offset: " << offsetof(ParticleUBO, particleLifetime) << std::endl;
    std::cout << "windStrength offset: " << offsetof(ParticleUBO, windStrength) << std::endl;
    std::cout << "gravityStrength offset: " << offsetof(ParticleUBO, gravityStrength) << std::endl;
    std::cout << "sunRayLength offset: " << offsetof(ParticleUBO, sunRayLength) << std::endl;
    
    // Calculate expected size vs actual
    size_t expectedSize = 64 + 64 + 16 + 16 + 16 + 8*4; // matrices + vec4s + floats/uints
    std::cout << "\nExpected minimum size: " << expectedSize << std::endl;
    std::cout << "Actual size: " << sizeof(ParticleUBO) << std::endl;
    std::cout << "Padding: " << sizeof(ParticleUBO) - expectedSize << std::endl;
    
    return 0;
}
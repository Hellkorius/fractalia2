#include "graphicstests.h"
#include "vulkan_renderer.h"
#include "ecs/gpu_entity_manager.h"
#include <iostream>

namespace GraphicsTests {
    
void runBufferOverflowTests(VulkanRenderer* renderer) {
    if (!renderer) {
        std::cerr << "ERROR: Cannot run tests - renderer is null!" << std::endl;
        return;
    }
    
    std::cout << "\n🔧 GRAPHICS BUFFER OVERFLOW TESTS INITIATED 🔧" << std::endl;
    std::cout << "Testing memory safety fixes..." << std::endl;
    
    // Run the comprehensive buffer overflow protection test
    bool testsPassed = renderer->testBufferOverflowProtection();
    
    if (testsPassed) {
        std::cout << "🎉 MEMORY SAFETY VALIDATION COMPLETE!" << std::endl;
        std::cout << "All buffer overflow protections are working correctly." << std::endl;
    } else {
        std::cerr << "⚠️  MEMORY SAFETY ISSUES DETECTED!" << std::endl;
        std::cerr << "Some buffer overflow protections may not be working properly." << std::endl;
    }
    
    std::cout << "Press any key to continue..." << std::endl;
}

void runPerformanceTests(VulkanRenderer* renderer) {
    if (!renderer) {
        std::cerr << "ERROR: Cannot run performance tests - renderer is null!" << std::endl;
        return;
    }
    
    std::cout << "\n📊 GRAPHICS PERFORMANCE TESTS" << std::endl;
    
    // Test CPU instance buffer capacity
    uint32_t maxCpuInstances = renderer->getMaxCpuInstances();
    std::cout << "CPU Instance Buffer Capacity: " << maxCpuInstances << " entities" << std::endl;
    
    // Test GPU entity buffer capacity
    auto* gpuManager = renderer->getGPUEntityManager();
    if (gpuManager) {
        uint32_t gpuEntityCount = gpuManager->getEntityCount();
        uint32_t maxGpuEntities = gpuManager->getMaxEntities();
        std::cout << "GPU Entity Buffer: " << gpuEntityCount << "/" << maxGpuEntities << " entities" << std::endl;
        
        float gpuUtilization = (gpuEntityCount > 0) ? (100.0f * gpuEntityCount / maxGpuEntities) : 0.0f;
        std::cout << "GPU Buffer Utilization: " << gpuUtilization << "%" << std::endl;
        
        if (gpuUtilization > 90.0f) {
            std::cout << "⚠️  GPU buffer is near capacity!" << std::endl;
        } else if (gpuUtilization > 75.0f) {
            std::cout << "📈 GPU buffer utilization is high" << std::endl;
        } else {
            std::cout << "✅ GPU buffer utilization is healthy" << std::endl;
        }
    } else {
        std::cout << "GPU Entity Manager: Not available (using CPU rendering)" << std::endl;
    }
    
    std::cout << "Performance test complete." << std::endl;
}

void runAllTests(VulkanRenderer* renderer) {
    std::cout << "\n🚀 RUNNING ALL GRAPHICS TESTS 🚀" << std::endl;
    
    runBufferOverflowTests(renderer);
    runPerformanceTests(renderer);
    
    std::cout << "\n✨ ALL GRAPHICS TESTS COMPLETE ✨\n" << std::endl;
}

} // namespace GraphicsTests
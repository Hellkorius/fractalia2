#pragma once

class VulkanRenderer;

namespace GraphicsTests {
    
    // Buffer overflow and memory safety tests
    void runBufferOverflowTests(VulkanRenderer* renderer);
    
    // Performance and capacity tests  
    void runPerformanceTests(VulkanRenderer* renderer);
    
    // Run all graphics tests
    void runAllTests(VulkanRenderer* renderer);
    
} // namespace GraphicsTests
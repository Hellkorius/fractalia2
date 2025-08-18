#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <functional>

// Forward declarations
class VulkanContext;
class ComputePipelineManager;
class GPUTimeoutDetector;
class GPUMemoryMonitor;

/**
 * Compute Stress Tester - Validates compute pipeline stability under various loads
 * to prevent VK_ERROR_DEVICE_LOST in production
 */
class ComputeStressTester {
public:
    ComputeStressTester(
        const VulkanContext* context,
        ComputePipelineManager* pipelineManager,
        std::shared_ptr<GPUTimeoutDetector> timeoutDetector = nullptr,
        std::shared_ptr<GPUMemoryMonitor> memoryMonitor = nullptr
    );
    ~ComputeStressTester();

    // Test configuration
    struct StressTestConfig {
        uint32_t maxWorkgroups;           // Maximum workgroups to test
        uint32_t workgroupIncrement;      // Increment step
        uint32_t iterationsPerSize;       // Iterations per workgroup count
        float timeoutThresholdMs;         // Fail if dispatch takes longer
        bool enableMemoryPressure;       // Test under memory pressure
        bool enableConcurrentTests;      // Test multiple dispatches
        bool validateResults;             // Verify compute results
        
        // Constructor with defaults
        StressTestConfig() 
            : maxWorkgroups(5000)
            , workgroupIncrement(250)
            , iterationsPerSize(10)
            , timeoutThresholdMs(50.0f)
            , enableMemoryPressure(true)
            , enableConcurrentTests(false)
            , validateResults(true) {}
    };
    
    // Test results
    struct StressTestResult {
        bool passed = false;
        uint32_t maxStableWorkgroups = 0;        // Largest successful dispatch
        float averageDispatchTimeMs = 0.0f;      // Average execution time
        float peakDispatchTimeMs = 0.0f;         // Worst case execution time
        uint32_t failedAttempts = 0;             // Number of failures
        VkResult lastError = VK_SUCCESS;         // Last Vulkan error
        
        // Performance metrics
        float throughputEntitiesPerSecond = 0.0f;
        float memoryBandwidthGBps = 0.0f;
        float gpuUtilizationPercent = 0.0f;
        
        // Recommendations
        struct {
            uint32_t recommendedMaxWorkgroups = 0;
            bool shouldEnableChunking = false;
            bool shouldReduceWorkgroupSize = false;
            float safeDispatchTimeMs = 0.0f;
        } recommendations;
        
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };
    
    // Testing interface
    StressTestResult runStressTest(const StressTestConfig& config = StressTestConfig{});
    StressTestResult runQuickValidation(uint32_t targetWorkgroups = 2500);
    StressTestResult runProgressiveLoad(uint32_t startWorkgroups = 100, uint32_t maxWorkgroups = 3000);
    
    // Specific test scenarios
    bool testEntityMovementDispatch(uint32_t workgroupCount, uint32_t iterations = 1);
    bool testMemoryBandwidth(uint32_t bufferSizeMB = 100);
    bool testConcurrentDispatches(uint32_t dispatchCount = 3, uint32_t workgroupsEach = 1000);
    
    // Validation and safety
    bool validateComputeResults(VkBuffer inputBuffer, VkBuffer outputBuffer, uint32_t elementCount);
    uint32_t findSafeMaxWorkgroups(float targetTimeMs = 16.0f);
    
    // GPU health monitoring
    bool isGPUStable() const;
    void resetGPUState();

private:
    const VulkanContext* context;
    ComputePipelineManager* pipelineManager;
    std::shared_ptr<GPUTimeoutDetector> timeoutDetector;
    std::shared_ptr<GPUMemoryMonitor> memoryMonitor;
    
    // Test infrastructure
    VkCommandPool testCommandPool = VK_NULL_HANDLE;
    VkCommandBuffer testCommandBuffer = VK_NULL_HANDLE;
    VkFence testFence = VK_NULL_HANDLE;
    
    // Test buffers
    VkBuffer testEntityBuffer = VK_NULL_HANDLE;
    VkDeviceMemory testEntityMemory = VK_NULL_HANDLE;
    VkBuffer testPositionBuffer = VK_NULL_HANDLE;
    VkDeviceMemory testPositionMemory = VK_NULL_HANDLE;
    VkBuffer testCurrentPosBuffer = VK_NULL_HANDLE;
    VkDeviceMemory testCurrentPosMemory = VK_NULL_HANDLE;
    VkBuffer testTargetPosBuffer = VK_NULL_HANDLE;
    VkDeviceMemory testTargetPosMemory = VK_NULL_HANDLE;
    
    VkDescriptorPool testDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet testDescriptorSet = VK_NULL_HANDLE;
    
    static constexpr uint32_t MAX_TEST_ENTITIES = 200000; // 200k entities for stress testing
    
    // Internal methods
    bool createTestResources();
    void destroyTestResources();
    bool createTestBuffers();
    bool createTestDescriptors();
    void populateTestData();
    
    // Test execution
    bool executeComputeDispatch(uint32_t workgroupCount, float& executionTimeMs);
    bool waitForCompletion(float timeoutMs = 1000.0f);
    void recordTestDispatch(VkCommandBuffer cmd, uint32_t workgroupCount);
    
    // Validation helpers
    bool validateEntityMovement(uint32_t entityCount);
    bool checkDeviceStatus();
    void logTestProgress(const std::string& testName, uint32_t current, uint32_t total);
    
    // Error handling
    void handleTestFailure(const std::string& testName, VkResult result);
    void addRecommendations(StressTestResult& result, const StressTestConfig& config);
};
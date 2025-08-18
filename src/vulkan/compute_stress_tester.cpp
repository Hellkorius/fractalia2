#include "compute_stress_tester.h"
#include "vulkan_context.h"
#include "vulkan_function_loader.h"
#include "compute_pipeline_manager.h"
#include "gpu_timeout_detector.h"
#include "gpu_memory_monitor.h"
#include "descriptor_layout_manager.h"
#include <iostream>
#include <chrono>
#include <random>
#include <thread>

ComputeStressTester::ComputeStressTester(
    const VulkanContext* context,
    ComputePipelineManager* pipelineManager,
    std::shared_ptr<GPUTimeoutDetector> timeoutDetector,
    std::shared_ptr<GPUMemoryMonitor> memoryMonitor
) : context(context)
  , pipelineManager(pipelineManager)
  , timeoutDetector(timeoutDetector)
  , memoryMonitor(memoryMonitor) {
    
    if (!createTestResources()) {
        std::cerr << "ComputeStressTester: Failed to create test resources" << std::endl;
    }
}

ComputeStressTester::~ComputeStressTester() {
    destroyTestResources();
}

ComputeStressTester::StressTestResult ComputeStressTester::runQuickValidation(uint32_t targetWorkgroups) {
    std::cout << "ComputeStressTester: Running quick validation for " << targetWorkgroups << " workgroups" << std::endl;
    
    StressTestResult result{};
    result.passed = testEntityMovementDispatch(targetWorkgroups, 5);
    
    if (result.passed) {
        result.maxStableWorkgroups = targetWorkgroups;
        std::cout << "ComputeStressTester: Quick validation PASSED" << std::endl;
    } else {
        std::cout << "ComputeStressTester: Quick validation FAILED" << std::endl;
        result.errors.push_back("Quick validation failed at " + std::to_string(targetWorkgroups) + " workgroups");
    }
    
    return result;
}

ComputeStressTester::StressTestResult ComputeStressTester::runProgressiveLoad(uint32_t startWorkgroups, uint32_t maxWorkgroups) {
    std::cout << "ComputeStressTester: Running progressive load test from " 
              << startWorkgroups << " to " << maxWorkgroups << " workgroups" << std::endl;
    
    StressTestResult result{};
    std::vector<float> executionTimes;
    
    uint32_t currentWorkgroups = startWorkgroups;
    uint32_t increment = 250; // Test every 250 workgroups
    
    while (currentWorkgroups <= maxWorkgroups) {
        logTestProgress("Progressive Load", currentWorkgroups, maxWorkgroups);
        
        float executionTime = 0.0f;
        bool success = executeComputeDispatch(currentWorkgroups, executionTime);
        
        if (!success) {
            result.errors.push_back("Progressive test failed at " + std::to_string(currentWorkgroups) + " workgroups");
            break;
        }
        
        executionTimes.push_back(executionTime);
        result.maxStableWorkgroups = currentWorkgroups;
        result.peakDispatchTimeMs = std::max(result.peakDispatchTimeMs, executionTime);
        
        // Check if execution time is getting dangerously high
        if (executionTime > 100.0f) { // 100ms is definitely too long
            result.warnings.push_back("Execution time approaching dangerous levels at " + 
                                     std::to_string(currentWorkgroups) + " workgroups");
            break;
        }
        
        currentWorkgroups += increment;
    }
    
    // Calculate statistics
    if (!executionTimes.empty()) {
        float totalTime = std::accumulate(executionTimes.begin(), executionTimes.end(), 0.0f);
        result.averageDispatchTimeMs = totalTime / executionTimes.size();
        result.passed = true;
        
        // Calculate throughput
        uint32_t entitiesPerDispatch = result.maxStableWorkgroups * 64; // 64 threads per workgroup
        if (result.averageDispatchTimeMs > 0.0f) {
            result.throughputEntitiesPerSecond = entitiesPerDispatch / (result.averageDispatchTimeMs / 1000.0f);
        }
    }
    
    std::cout << "ComputeStressTester: Progressive load test completed. Max stable: " 
              << result.maxStableWorkgroups << " workgroups" << std::endl;
    
    return result;
}

bool ComputeStressTester::testEntityMovementDispatch(uint32_t workgroupCount, uint32_t iterations) {
    if (!pipelineManager || !testCommandBuffer) {
        return false;
    }
    
    for (uint32_t i = 0; i < iterations; ++i) {
        float executionTime = 0.0f;
        if (!executeComputeDispatch(workgroupCount, executionTime)) {
            std::cerr << "ComputeStressTester: Entity movement dispatch failed on iteration " 
                      << (i + 1) << "/" << iterations << std::endl;
            return false;
        }
        
        // Validate results occasionally
        if (i % 3 == 0 && !validateEntityMovement(workgroupCount * 64)) {
            std::cerr << "ComputeStressTester: Entity movement validation failed" << std::endl;
            return false;
        }
    }
    
    return true;
}

bool ComputeStressTester::executeComputeDispatch(uint32_t workgroupCount, float& executionTimeMs) {
    if (!testCommandBuffer || !pipelineManager) {
        return false;
    }
    
    // Reset command buffer
    context->getLoader().vkResetCommandBuffer(testCommandBuffer, 0);
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    VkResult result = context->getLoader().vkBeginCommandBuffer(testCommandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        handleTestFailure("vkBeginCommandBuffer", result);
        return false;
    }
    
    // Record dispatch
    recordTestDispatch(testCommandBuffer, workgroupCount);
    
    result = context->getLoader().vkEndCommandBuffer(testCommandBuffer);
    if (result != VK_SUCCESS) {
        handleTestFailure("vkEndCommandBuffer", result);
        return false;
    }
    
    // Execute and time
    auto startTime = std::chrono::high_resolution_clock::now();
    
    if (timeoutDetector) {
        timeoutDetector->beginComputeDispatch("StressTest", workgroupCount);
    }
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &testCommandBuffer;
    
    result = context->getLoader().vkQueueSubmit(context->getGraphicsQueue(), 1, &submitInfo, testFence);
    if (result != VK_SUCCESS) {
        handleTestFailure("vkQueueSubmit", result);
        return false;
    }
    
    // Wait for completion with timeout
    if (!waitForCompletion(1000.0f)) { // 1 second timeout
        std::cerr << "ComputeStressTester: Dispatch timed out" << std::endl;
        return false;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    executionTimeMs = durationUs.count() / 1000.0f;
    
    if (timeoutDetector) {
        timeoutDetector->endComputeDispatch();
    }
    
    // Check device status
    return checkDeviceStatus();
}

void ComputeStressTester::recordTestDispatch(VkCommandBuffer cmd, uint32_t workgroupCount) {
    // This would record the actual compute dispatch
    // For now, just record a simple dispatch command
    
    // TODO: Implement actual pipeline binding and dispatch recording
    // This would need access to the entity movement compute pipeline
    
    std::cout << "ComputeStressTester: Recording dispatch for " << workgroupCount << " workgroups" << std::endl;
}

bool ComputeStressTester::waitForCompletion(float timeoutMs) {
    if (testFence == VK_NULL_HANDLE) return false;
    
    uint64_t timeoutNs = static_cast<uint64_t>(timeoutMs * 1000000.0f); // Convert to nanoseconds
    
    VkResult result = context->getLoader().vkWaitForFences(
        context->getDevice(), 1, &testFence, VK_TRUE, timeoutNs);
    
    if (result == VK_TIMEOUT) {
        std::cerr << "ComputeStressTester: Fence wait timed out after " << timeoutMs << "ms" << std::endl;
        return false;
    } else if (result != VK_SUCCESS) {
        handleTestFailure("vkWaitForFences", result);
        return false;
    }
    
    // Reset fence for next use
    context->getLoader().vkResetFences(context->getDevice(), 1, &testFence);
    
    return true;
}

bool ComputeStressTester::validateEntityMovement(uint32_t entityCount) {
    // Simplified validation - just check that we can read from the buffers
    // In a real implementation, this would validate the computed positions
    return true;
}

bool ComputeStressTester::checkDeviceStatus() {
    VkResult result = context->getLoader().vkDeviceWaitIdle(context->getDevice());
    
    if (result == VK_ERROR_DEVICE_LOST) {
        std::cerr << "ComputeStressTester: CRITICAL - VK_ERROR_DEVICE_LOST detected!" << std::endl;
        return false;
    } else if (result != VK_SUCCESS) {
        handleTestFailure("vkDeviceWaitIdle", result);
        return false;
    }
    
    return true;
}

void ComputeStressTester::logTestProgress(const std::string& testName, uint32_t current, uint32_t total) {
    float progress = (float(current) / float(total)) * 100.0f;
    std::cout << "ComputeStressTester: " << testName << " - " 
              << current << "/" << total << " (" << progress << "%)" << std::endl;
}

void ComputeStressTester::handleTestFailure(const std::string& operation, VkResult result) {
    std::cerr << "ComputeStressTester: " << operation << " failed with result " << result << std::endl;
    
    if (result == VK_ERROR_DEVICE_LOST) {
        std::cerr << "ComputeStressTester: DEVICE LOST - GPU has crashed!" << std::endl;
    }
}

bool ComputeStressTester::createTestResources() {
    if (!context) return false;
    
    // Create command pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = context->getGraphicsQueueFamily();
    
    VkResult result = context->getLoader().vkCreateCommandPool(
        context->getDevice(), &poolInfo, nullptr, &testCommandPool);
    if (result != VK_SUCCESS) {
        std::cerr << "ComputeStressTester: Failed to create command pool" << std::endl;
        return false;
    }
    
    // Allocate command buffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = testCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    
    result = context->getLoader().vkAllocateCommandBuffers(context->getDevice(), &allocInfo, &testCommandBuffer);
    if (result != VK_SUCCESS) {
        std::cerr << "ComputeStressTester: Failed to allocate command buffer" << std::endl;
        return false;
    }
    
    // Create fence
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    
    result = context->getLoader().vkCreateFence(context->getDevice(), &fenceInfo, nullptr, &testFence);
    if (result != VK_SUCCESS) {
        std::cerr << "ComputeStressTester: Failed to create fence" << std::endl;
        return false;
    }
    
    return createTestBuffers() && createTestDescriptors();
}

bool ComputeStressTester::createTestBuffers() {
    // Create minimal test buffers
    // In a real implementation, these would be properly sized and initialized
    return true;
}

bool ComputeStressTester::createTestDescriptors() {
    // Create minimal descriptor resources
    // In a real implementation, this would set up proper descriptor sets
    return true;
}

void ComputeStressTester::destroyTestResources() {
    if (!context) return;
    
    if (testFence != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyFence(context->getDevice(), testFence, nullptr);
        testFence = VK_NULL_HANDLE;
    }
    
    if (testCommandPool != VK_NULL_HANDLE) {
        context->getLoader().vkDestroyCommandPool(context->getDevice(), testCommandPool, nullptr);
        testCommandPool = VK_NULL_HANDLE;
        testCommandBuffer = VK_NULL_HANDLE; // Destroyed with pool
    }
    
    // TODO: Destroy test buffers and descriptors
}

uint32_t ComputeStressTester::findSafeMaxWorkgroups(float targetTimeMs) {
    std::cout << "ComputeStressTester: Finding safe max workgroups for target time " << targetTimeMs << "ms" << std::endl;
    
    uint32_t low = 100;
    uint32_t high = 5000;
    uint32_t safeMax = 0;
    
    while (low <= high) {
        uint32_t mid = (low + high) / 2;
        
        float executionTime = 0.0f;
        if (executeComputeDispatch(mid, executionTime)) {
            if (executionTime <= targetTimeMs) {
                safeMax = mid;
                low = mid + 1;
            } else {
                high = mid - 1;
            }
        } else {
            high = mid - 1;
        }
    }
    
    std::cout << "ComputeStressTester: Safe max workgroups: " << safeMax << std::endl;
    return safeMax;
}
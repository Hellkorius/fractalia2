#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <iostream>

// Centralized validation utilities to eliminate DRY violations
class ValidationUtils {
public:
    // Dependency validation
    template<typename... Args>
    static bool validateDependencies(const std::string& context, Args... dependencies) {
        std::vector<const void*> deps = {static_cast<const void*>(dependencies)...};
        return validateDependenciesImpl(context, deps);
    }
    
    // Resource handle validation
    static bool validateResourceHandle(const void* handle, const std::string& handleName, const std::string& context);
    
    // Buffer operation validation
    static bool validateBufferCopy(const void* srcBuffer, const void* dstBuffer, 
                                  VkDeviceSize size, const std::string& context);
    
    // Memory validation
    static bool validateMemoryMapping(VkDevice device, VkDeviceMemory memory, 
                                     VkDeviceSize size, const std::string& context);
    
    // Vulkan result validation
    static bool validateVulkanResult(VkResult result, const std::string& operation, 
                                    const std::string& context);
    
    // Error logging utilities
    static void logError(const std::string& component, const std::string& operation, 
                        const std::string& details = "");
    
    static void logInitializationError(const std::string& component, 
                                      const std::string& reason = "");
    
    static void logValidationFailure(const std::string& context, 
                                    const std::string& validation, 
                                    const std::string& details = "");

private:
    static bool validateDependenciesImpl(const std::string& context, 
                                        const std::vector<const void*>& dependencies);
};
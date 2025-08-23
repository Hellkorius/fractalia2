#include "validation_utils.h"

bool ValidationUtils::validateDependenciesImpl(const std::string& context, 
                                               const std::vector<const void*>& dependencies) {
    for (size_t i = 0; i < dependencies.size(); ++i) {
        if (!dependencies[i]) {
            logValidationFailure(context, "dependency " + std::to_string(i), "null pointer");
            return false;
        }
    }
    return true;
}

bool ValidationUtils::validateResourceHandle(const void* handle, const std::string& handleName, 
                                            const std::string& context) {
    if (!handle) {
        logValidationFailure(context, handleName, "null handle");
        return false;
    }
    return true;
}

bool ValidationUtils::validateBufferCopy(const void* srcBuffer, const void* dstBuffer, 
                                        VkDeviceSize size, const std::string& context) {
    if (!srcBuffer) {
        logValidationFailure(context, "source buffer", "null handle");
        return false;
    }
    
    if (!dstBuffer) {
        logValidationFailure(context, "destination buffer", "null handle");
        return false;
    }
    
    if (size == 0) {
        logValidationFailure(context, "copy size", "zero bytes");
        return false;
    }
    
    return true;
}

bool ValidationUtils::validateMemoryMapping(VkDevice device, VkDeviceMemory memory, 
                                           VkDeviceSize size, const std::string& context) {
    if (device == VK_NULL_HANDLE) {
        logValidationFailure(context, "device", "null handle");
        return false;
    }
    
    if (memory == VK_NULL_HANDLE) {
        logValidationFailure(context, "memory", "null handle");
        return false;
    }
    
    if (size == 0) {
        logValidationFailure(context, "memory size", "zero bytes");
        return false;
    }
    
    return true;
}

bool ValidationUtils::validateVulkanResult(VkResult result, const std::string& operation, 
                                          const std::string& context) {
    if (result != VK_SUCCESS) {
        logError(context, operation, "Vulkan result: " + std::to_string(static_cast<int>(result)));
        return false;
    }
    return true;
}

void ValidationUtils::logError(const std::string& component, const std::string& operation, 
                              const std::string& details) {
    std::cerr << "ERROR [" << component << "] " << operation;
    if (!details.empty()) {
        std::cerr << ": " << details;
    }
    std::cerr << std::endl;
}

void ValidationUtils::logInitializationError(const std::string& component, 
                                            const std::string& reason) {
    std::string details = reason.empty() ? "initialization failed" : reason;
    logError(component, "initialize", details);
}

void ValidationUtils::logValidationFailure(const std::string& context, 
                                          const std::string& validation, 
                                          const std::string& details) {
    std::string message = "validation failed for " + validation;
    if (!details.empty()) {
        message += " (" + details + ")";
    }
    logError(context, "validate", message);
}
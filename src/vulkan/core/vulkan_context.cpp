#include "vulkan_context.h"
#include "vulkan_function_loader.h"
#include <iostream>
#include <set>
#include <algorithm>

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    // Low-latency optimization extension (optional)
    VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

VulkanContext::VulkanContext() {
}

VulkanContext::~VulkanContext() {
    cleanup();
}

bool VulkanContext::initialize(SDL_Window* window) {
    this->window = window;
    
    // Create the loader
    loader = std::make_unique<VulkanFunctionLoader>();
    if (!loader->initialize(window)) {
        std::cerr << "Failed to initialize VulkanFunctionLoader" << std::endl;
        return false;
    }
    
    if (!createInstance()) {
        std::cerr << "Failed to create Vulkan instance" << std::endl;
        return false;
    }
    
    // Update shared function loader with instance handle
    loader->setInstance(instance.get());
    loader->loadPostInstanceFunctions();
    
    // Setup debug messenger for surface and swapchain monitoring
    setupDebugMessenger();
    
    if (!createSurface()) {
        std::cerr << "Failed to create surface" << std::endl;
        return false;
    }
    
    if (!pickPhysicalDevice()) {
        std::cerr << "Failed to pick physical device" << std::endl;
        return false;
    }
    
    if (!createLogicalDevice()) {
        std::cerr << "Failed to create logical device" << std::endl;
        return false;
    }
    
    // Update shared function loader with device handles
    loader->setDevice(device.get(), physicalDevice);
    if (!loader->loadPostDeviceFunctions()) {
        std::cerr << "Failed to load device functions in shared loader" << std::endl;
        return false;
    }
    
    // Now that device functions are loaded, we can get the queues
    getDeviceQueues();
    
    return true;
}

void VulkanContext::cleanup() {
    cleanupBeforeContextDestruction();
    
    if (loader) {
        loader->cleanup();
        loader.reset();
    }
}

void VulkanContext::cleanupBeforeContextDestruction() {
    // RAII wrappers handle automatic cleanup
    debugMessenger.reset();
    device.reset();
    surface.reset();
    instance.reset();
}

bool VulkanContext::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Fractalia2";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine"; 
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredExtensions();
    if (extensions.empty()) {
        std::cerr << "No required extensions available from SDL" << std::endl;
        return false;
    }
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (!loader->vkCreateInstance) {
        std::cerr << "vkCreateInstance is null!" << std::endl;
        return false;
    }
    VkInstance rawInstance;
    if (loader->vkCreateInstance(&createInfo, nullptr, &rawInstance) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance" << std::endl;
        return false;
    }
    
    instance = vulkan_raii::make_instance(rawInstance, this);

    return true;
}

bool VulkanContext::createSurface() {
    VkSurfaceKHR rawSurface;
    if (!SDL_Vulkan_CreateSurface(window, instance.get(), nullptr, &rawSurface)) {
        std::cerr << "Failed to create Vulkan surface: " << SDL_GetError() << std::endl;
        return false;
    }
    
    surface = vulkan_raii::make_surface_khr(rawSurface, this);
    return true;
}

bool VulkanContext::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    loader->vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        std::cerr << "Failed to find GPUs with Vulkan support" << std::endl;
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    loader->vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            physicalDevice = device;
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        std::cerr << "Failed to find a suitable GPU" << std::endl;
        return false;
    }

    return true;
}

bool VulkanContext::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        indices.graphicsFamily.value(), 
        indices.presentFamily.value(),
        indices.computeFamily.value()
    };
    
    // Add transfer queue family if it's different from graphics
    if (indices.transferFamily.has_value()) {
        uniqueQueueFamilies.insert(indices.transferFamily.value());
    }

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    // Build list of actually supported extensions
    uint32_t extensionCount;
    loader->vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    loader->vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());
    
    std::vector<const char*> enabledExtensions;
    enabledExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME); // Always required
    
    // Add optional extensions if supported
    for (const auto& extension : availableExtensions) {
        if (std::string(extension.extensionName) == VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME) {
            enabledExtensions.push_back(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
            break;
        }
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
    createInfo.ppEnabledExtensionNames = enabledExtensions.data();
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;

    VkDevice rawDevice;
    VkResult result = loader->vkCreateDevice(physicalDevice, &createInfo, nullptr, &rawDevice);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create logical device (VkResult: " << result << ")" << std::endl;
        return false;
    }
    
    device = vulkan_raii::make_device(rawDevice, this);
    

    // Store queue family indices for later use after device functions are loaded
    queueFamilyIndices = indices;

    return true;
}

void VulkanContext::getDeviceQueues() {
    if (!device) {
        std::cerr << "Cannot get device queues: device not created" << std::endl;
        return;
    }
    
    if (!queueFamilyIndices.isComplete()) {
        std::cerr << "Cannot get device queues: queue family indices not set" << std::endl;
        return;
    }
    
    loader->vkGetDeviceQueue(device.get(), queueFamilyIndices.graphicsFamily.value(), 0, &graphicsQueue);
    loader->vkGetDeviceQueue(device.get(), queueFamilyIndices.presentFamily.value(), 0, &presentQueue);
    loader->vkGetDeviceQueue(device.get(), queueFamilyIndices.computeFamily.value(), 0, &computeQueue);
    
    // Get transfer queue if available (otherwise will use graphics queue fallback)
    if (queueFamilyIndices.transferFamily.has_value()) {
        loader->vkGetDeviceQueue(device.get(), queueFamilyIndices.transferFamily.value(), 0, &transferQueue);
    }
    
    std::cout << "VulkanContext: Queue families - Graphics: " << queueFamilyIndices.graphicsFamily.value() 
              << ", Present: " << queueFamilyIndices.presentFamily.value()
              << ", Compute: " << queueFamilyIndices.computeFamily.value();
    if (queueFamilyIndices.transferFamily.has_value()) {
        std::cout << ", Transfer: " << queueFamilyIndices.transferFamily.value() << " (dedicated)";
    } else {
        std::cout << ", Transfer: " << queueFamilyIndices.graphicsFamily.value() << " (graphics fallback)";
    }
    std::cout << std::endl;
}

QueueFamilyIndices VulkanContext::findQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilyIndices indices;

    if (!loader) {
        std::cerr << "loader is null!" << std::endl;
        return indices;
    }
    if (!loader->vkGetPhysicalDeviceQueueFamilyProperties) {
        std::cerr << "vkGetPhysicalDeviceQueueFamilyProperties is null!" << std::endl;
        return indices;
    }

    uint32_t queueFamilyCount = 0;
    loader->vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    loader->vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        // Find graphics queue
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        // Find present queue
        VkBool32 presentSupport = false;
        if (!loader->vkGetPhysicalDeviceSurfaceSupportKHR) {
            std::cerr << "vkGetPhysicalDeviceSurfaceSupportKHR is null!" << std::endl;
            return indices;
        }
        if (!surface) {
            std::cerr << "surface is VK_NULL_HANDLE!" << std::endl;
            return indices;
        }
        loader->vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface.get(), &presentSupport);

        if (presentSupport) {
            indices.presentFamily = i;
        }
        
        // Find dedicated compute queue (prefer compute-only, fall back to graphics+compute)
        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            // Prefer dedicated compute queue (compute without graphics)
            if (!indices.computeFamily.has_value() || 
                !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                indices.computeFamily = i;
            }
        }
        
        // Find dedicated transfer queue (prefer transfer-only, fall back to any transfer-capable)
        if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) {
            // Prefer dedicated transfer queue (transfer without graphics/compute)
            if (!indices.transferFamily.has_value() ||
                (!(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) && 
                 !(queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT))) {
                indices.transferFamily = i;
            }
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }
    
    // Fallback: if no dedicated compute queue, use graphics queue for compute
    if (!indices.computeFamily.has_value() && indices.graphicsFamily.has_value()) {
        std::cout << "VulkanContext: No dedicated compute queue found, using graphics queue for compute" << std::endl;
        indices.computeFamily = indices.graphicsFamily;
    }
    
    // Note: Transfer queue fallback is handled in getTransferQueue() - no dedicated queue is acceptable
    
    return indices;
}

bool VulkanContext::isDeviceSuitable(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties deviceProperties;
    loader->vkGetPhysicalDeviceProperties(device, &deviceProperties);
    
    // Check extension support
    uint32_t extensionCount;
    loader->vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    loader->vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
    
    // Required extensions
    std::set<std::string> requiredExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    
    // Optional extensions for better performance
    std::set<std::string> optionalExtensions = { VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME };
    
    // Check which extensions are available
    std::set<std::string> supportedExtensions;
    for (const auto& extension : availableExtensions) {
        supportedExtensions.insert(extension.extensionName);
        requiredExtensions.erase(extension.extensionName);
        optionalExtensions.erase(extension.extensionName);
    }
    
    // Log extension support for diagnostics
    if (supportedExtensions.count(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME)) {
        std::cout << "VK_EXT_swapchain_maintenance1 supported - enabling low-latency optimizations" << std::endl;
    } else {
        std::cout << "VK_EXT_swapchain_maintenance1 not supported - using standard presentation" << std::endl;
    }
    
    bool extensionsSupported = requiredExtensions.empty();
    QueueFamilyIndices indices = findQueueFamilies(device);
    
    return indices.isComplete() && extensionsSupported;
}

std::vector<const char*> VulkanContext::getRequiredExtensions() {
    uint32_t extensionCount = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    
    if (extensions == nullptr) {
        std::cerr << "Failed to get required Vulkan instance extensions from SDL" << std::endl;
        return {};
    }
    
    std::vector<const char*> requiredExtensions(extensions, extensions + extensionCount);
    
    // Add debug utils extension for validation layer callbacks
    requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    
    return requiredExtensions;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    
    // Enhanced validation for resize issue investigation
    // Show ALL messages during resize troubleshooting
    const char* severityStr = "";
    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: severityStr = "VERBOSE"; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: severityStr = "INFO"; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: severityStr = "WARNING"; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: severityStr = "ERROR"; break;
        default: severityStr = "UNKNOWN"; break;
    }
    
    const char* typeStr = "";
    switch (messageType) {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT: typeStr = "GENERAL"; break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT: typeStr = "VALIDATION"; break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: typeStr = "PERFORMANCE"; break;
        default: typeStr = "UNKNOWN"; break;
    }
    
    // Priority keywords for resize investigation
    bool isResizeRelated = pCallbackData && pCallbackData->pMessage && 
        (strstr(pCallbackData->pMessage, "surface") || 
         strstr(pCallbackData->pMessage, "swapchain") ||
         strstr(pCallbackData->pMessage, "queue family") ||
         strstr(pCallbackData->pMessage, "command buffer") ||
         strstr(pCallbackData->pMessage, "fence") ||
         strstr(pCallbackData->pMessage, "semaphore") ||
         strstr(pCallbackData->pMessage, "pipeline") ||
         strstr(pCallbackData->pMessage, "descriptor") ||
         strstr(pCallbackData->pMessage, "memory") ||
         strstr(pCallbackData->pMessage, "buffer") ||
         strstr(pCallbackData->pMessage, "image"));
    
    // Show important messages or resize-related messages
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT || isResizeRelated) {
        std::cerr << "[VULKAN " << severityStr << " " << typeStr << "] " << pCallbackData->pMessage << std::endl;
    }
    
    return VK_FALSE;
}

bool VulkanContext::setupDebugMessenger() {
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;

    VkDebugUtilsMessengerEXT rawDebugMessenger;
    if (loader->vkCreateDebugUtilsMessengerEXT(instance.get(), &createInfo, nullptr, &rawDebugMessenger) != VK_SUCCESS) {
        std::cerr << "Failed to set up debug messenger!" << std::endl;
        return false;
    }
    
    debugMessenger = vulkan_raii::make_debug_utils_messenger_ext(rawDebugMessenger, this);
    
    std::cout << "Debug messenger setup successfully" << std::endl;
    return true;
}

void VulkanContext::cleanupDebugMessenger() {
    // RAII wrapper handles cleanup automatically
    debugMessenger.reset();
}


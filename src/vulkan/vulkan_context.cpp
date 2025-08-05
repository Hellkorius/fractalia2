#include "vulkan_context.h"
#include <iostream>
#include <set>
#include <algorithm>

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
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
    
    std::cout << "Loading Vulkan functions..." << std::endl;
    if (!loadVulkanFunctions()) {
        std::cerr << "Failed to load Vulkan functions" << std::endl;
        return false;
    }
    
    std::cout << "Creating Vulkan instance..." << std::endl;
    if (!createInstance()) {
        std::cerr << "Failed to create Vulkan instance" << std::endl;
        return false;
    }
    
    std::cout << "Creating surface..." << std::endl;
    if (!createSurface()) {
        std::cerr << "Failed to create surface" << std::endl;
        return false;
    }
    
    std::cout << "Picking physical device..." << std::endl;
    if (!pickPhysicalDevice()) {
        std::cerr << "Failed to pick physical device" << std::endl;
        return false;
    }
    
    std::cout << "Creating logical device..." << std::endl;
    if (!createLogicalDevice()) {
        std::cerr << "Failed to create logical device" << std::endl;
        return false;
    }
    
    return true;
}

void VulkanContext::cleanup() {
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }
    
    if (surface != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }
    
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }
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

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance" << std::endl;
        return false;
    }

    loadInstanceFunctions();
    return true;
}

bool VulkanContext::createSurface() {
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
        std::cerr << "Failed to create Vulkan surface: " << SDL_GetError() << std::endl;
        return false;
    }
    return true;
}

bool VulkanContext::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        std::cerr << "Failed to find GPUs with Vulkan support" << std::endl;
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

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
    
    std::cout << "Queue families found - Graphics: " << indices.graphicsFamily.value() 
              << ", Present: " << indices.presentFamily.value() << std::endl;

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
        std::cout << "Added queue family: " << queueFamily << std::endl;
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;

    std::cout << "Creating logical device with " << deviceExtensions.size() << " extensions..." << std::endl;
    for (const auto& ext : deviceExtensions) {
        std::cout << "  Device extension: " << ext << std::endl;
    }

    VkResult result = vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create logical device (VkResult: " << result << ")" << std::endl;
        return false;
    }

    std::cout << "Logical device created successfully" << std::endl;

    std::cout << "Getting graphics queue (family " << indices.graphicsFamily.value() << ")..." << std::endl;
    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    std::cout << "Graphics queue retrieved" << std::endl;

    std::cout << "Getting present queue (family " << indices.presentFamily.value() << ")..." << std::endl;
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
    std::cout << "Present queue retrieved" << std::endl;

    std::cout << "Device queues retrieved successfully" << std::endl;

    std::cout << "Loading device functions..." << std::endl;
    loadDeviceFunctions();
    std::cout << "Device functions loaded successfully" << std::endl;

    return true;
}

QueueFamilyIndices VulkanContext::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}

bool VulkanContext::isDeviceSuitable(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    
    std::cout << "Checking device: " << deviceProperties.deviceName << std::endl;
    
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
    
    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }
    
    bool extensionsSupported = requiredExtensions.empty();
    std::cout << "  Required extensions supported: " << (extensionsSupported ? "YES" : "NO") << std::endl;
    if (!extensionsSupported) {
        std::cout << "  Missing extensions:" << std::endl;
        for (const auto& ext : requiredExtensions) {
            std::cout << "    " << ext << std::endl;
        }
    }
    
    QueueFamilyIndices indices = findQueueFamilies(device);
    
    bool suitable = indices.isComplete() && extensionsSupported;
    
    std::cout << "Device suitable: " << (suitable ? "YES" : "NO") << std::endl;
    std::cout << "  Queue families complete: " << (indices.isComplete() ? "YES" : "NO") << std::endl;
    std::cout << "  Extensions supported: " << (extensionsSupported ? "YES" : "NO") << std::endl;
    
    return suitable;
}

std::vector<const char*> VulkanContext::getRequiredExtensions() {
    uint32_t extensionCount = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    
    if (extensions == nullptr) {
        std::cerr << "Failed to get required Vulkan instance extensions from SDL" << std::endl;
        return {};
    }
    
    std::vector<const char*> requiredExtensions(extensions, extensions + extensionCount);
    return requiredExtensions;
}

bool VulkanContext::loadVulkanFunctions() {
    vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
    if (!vkGetInstanceProcAddr) {
        std::cerr << "Failed to load vkGetInstanceProcAddr from SDL3" << std::endl;
        std::cerr << "Make sure Vulkan is installed and SDL3 has Vulkan support" << std::endl;
        return false;
    }

    std::cout << "Successfully loaded vkGetInstanceProcAddr" << std::endl;

    vkCreateInstance = (PFN_vkCreateInstance)vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance");
    if (!vkCreateInstance) {
        std::cerr << "Failed to load vkCreateInstance" << std::endl;
        return false;
    }

    std::cout << "Successfully loaded global Vulkan functions" << std::endl;
    return true;
}

void VulkanContext::loadInstanceFunctions() {
    vkDestroyInstance = (PFN_vkDestroyInstance)vkGetInstanceProcAddr(instance, "vkDestroyInstance");
    vkEnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)vkGetInstanceProcAddr(instance, "vkEnumeratePhysicalDevices");
    vkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties");
    vkGetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties");
    vkCreateDevice = (PFN_vkCreateDevice)vkGetInstanceProcAddr(instance, "vkCreateDevice");
    vkDestroySurfaceKHR = (PFN_vkDestroySurfaceKHR)vkGetInstanceProcAddr(instance, "vkDestroySurfaceKHR");
    vkGetPhysicalDeviceSurfaceSupportKHR = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceSupportKHR");
    vkEnumerateDeviceExtensionProperties = (PFN_vkEnumerateDeviceExtensionProperties)vkGetInstanceProcAddr(instance, "vkEnumerateDeviceExtensionProperties");
    vkGetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceMemoryProperties");
    
    vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)vkGetInstanceProcAddr(instance, "vkGetDeviceProcAddr");
    vkGetDeviceQueue = (PFN_vkGetDeviceQueue)vkGetInstanceProcAddr(instance, "vkGetDeviceQueue");
}

void VulkanContext::loadDeviceFunctions() {
    std::cout << "Loading device-level functions..." << std::endl;
    
    if (!vkGetDeviceProcAddr) {
        std::cerr << "vkGetDeviceProcAddr is null!" << std::endl;
        return;
    }
    
    vkDestroyDevice = (PFN_vkDestroyDevice)vkGetDeviceProcAddr(device, "vkDestroyDevice");
    std::cout << "  vkDestroyDevice loaded" << std::endl;
    
    vkDeviceWaitIdle = (PFN_vkDeviceWaitIdle)vkGetDeviceProcAddr(device, "vkDeviceWaitIdle");
    
    std::cout << "All device functions loaded" << std::endl;
}
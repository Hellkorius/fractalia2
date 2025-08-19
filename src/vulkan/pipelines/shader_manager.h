#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <functional>
#include <fstream>
#include <filesystem>
#include "../core/vulkan_context.h"
#include "../core/vulkan_raii.h"
#include "../core/vulkan_constants.h"

// Shader compilation types
enum class ShaderSourceType {
    SPIRV_BINARY,    // Pre-compiled SPIR-V
    GLSL_SOURCE,     // GLSL source code
    HLSL_SOURCE      // HLSL source code (for future DirectX compatibility)
};

// Shader stage information
struct ShaderStageInfo {
    VkShaderStageFlagBits stage = VK_SHADER_STAGE_VERTEX_BIT;
    std::string entryPoint = "main";
    
    // Specialization constants
    std::vector<uint32_t> specializationData;
    std::vector<VkSpecializationMapEntry> specializationMap;
    
    // Debug information
    std::string debugName;
    bool enableDebugInfo = false;
    bool enableOptimization = true;
};

// Shader module specification
struct ShaderModuleSpec {
    std::string filePath;
    ShaderSourceType sourceType = ShaderSourceType::SPIRV_BINARY;
    ShaderStageInfo stageInfo;
    
    // Include paths for GLSL compilation
    std::vector<std::string> includePaths;
    
    // Preprocessor definitions
    std::unordered_map<std::string, std::string> defines;
    
    // Hot reload support
    bool enableHotReload = false;
    std::filesystem::file_time_type lastModified{};
    
    // Comparison for caching
    bool operator==(const ShaderModuleSpec& other) const;
    size_t getHash() const;
};

// Hash specialization for shader caching
struct ShaderModuleSpecHash {
    std::size_t operator()(const ShaderModuleSpec& spec) const {
        return spec.getHash();
    }
};

// Cached shader module with metadata
struct CachedShaderModule {
    vulkan_raii::ShaderModule module;  // RAII wrapper for automatic cleanup
    ShaderModuleSpec spec;
    std::vector<uint32_t> spirvCode;  // Store SPIR-V for reflection
    
    // Usage tracking
    uint64_t lastUsedFrame = 0;
    uint32_t useCount = 0;
    
    // Compilation metadata
    std::chrono::nanoseconds compilationTime{0};
    std::filesystem::file_time_type sourceModified{};
    bool isHotReloadable = false;
    
    // Reflection data (for descriptor set layout generation)
    struct ReflectionData {
        std::vector<VkDescriptorSetLayoutBinding> descriptorBindings;
        std::vector<VkPushConstantRange> pushConstantRanges;
        uint32_t localSizeX = 1;  // For compute shaders
        uint32_t localSizeY = 1;
        uint32_t localSizeZ = 1;
    } reflection;
};

// Shader compilation result
struct ShaderCompilationResult {
    bool success = false;
    std::vector<uint32_t> spirvCode;
    std::string errorMessage;
    std::chrono::nanoseconds compilationTime{0};
};

// AAA Shader Manager with advanced features
class ShaderManager {
public:
    ShaderManager();
    ~ShaderManager();

    // Initialization
    bool initialize(const VulkanContext& context);
    void cleanup();
    
    // Explicit cleanup before context destruction
    void cleanupBeforeContextDestruction();

    // Shader loading and compilation
    VkShaderModule loadShader(const ShaderModuleSpec& spec);
    VkShaderModule loadShaderFromFile(const std::string& filePath, 
                                     VkShaderStageFlagBits stage,
                                     const std::string& entryPoint = "main");
    VkShaderModule loadSPIRVFromFile(const std::string& filePath);
    
    // Batch shader compilation for reduced overhead
    std::vector<VkShaderModule> loadShadersBatch(const std::vector<ShaderModuleSpec>& specs);
    
    // GLSL compilation (requires external compiler like glslc or shaderc)
    ShaderCompilationResult compileGLSL(const std::string& source,
                                       VkShaderStageFlagBits stage,
                                       const std::string& fileName = "shader.glsl",
                                       const std::unordered_map<std::string, std::string>& defines = {});
    
    ShaderCompilationResult compileGLSLFromFile(const std::string& filePath,
                                               const std::unordered_map<std::string, std::string>& defines = {});
    
    // Pipeline shader stage creation
    VkPipelineShaderStageCreateInfo createShaderStage(VkShaderModule module, 
                                                      VkShaderStageFlagBits stage,
                                                      const std::string& entryPoint = "main",
                                                      const VkSpecializationInfo* specializationInfo = nullptr);
    
    std::vector<VkPipelineShaderStageCreateInfo> createGraphicsShaderStages(
        VkShaderModule vertexShader,
        VkShaderModule fragmentShader,
        VkShaderModule geometryShader = VK_NULL_HANDLE,
        VkShaderModule tessControlShader = VK_NULL_HANDLE,
        VkShaderModule tessEvalShader = VK_NULL_HANDLE);
    
    VkPipelineShaderStageCreateInfo createComputeShaderStage(VkShaderModule computeShader,
                                                            const std::string& entryPoint = "main");
    
    // Hot reloading support
    void enableHotReload(bool enable) { hotReloadEnabled = enable; }
    void checkForShaderReloads();
    bool reloadShader(const ShaderModuleSpec& spec);
    void registerReloadCallback(const std::string& shaderPath, 
                               std::function<void(VkShaderModule)> callback);
    
    // Shader reflection and analysis
    struct ShaderReflection {
        std::vector<VkDescriptorSetLayoutBinding> descriptorBindings;
        std::vector<VkPushConstantRange> pushConstantRanges;
        VkShaderStageFlags stageFlags = 0;
        
        // Compute shader specific
        uint32_t localSizeX = 1;
        uint32_t localSizeY = 1;
        uint32_t localSizeZ = 1;
        
        // Resources used
        uint32_t uniformBuffers = 0;
        uint32_t storageBuffers = 0;
        uint32_t sampledImages = 0;
        uint32_t storageImages = 0;
        uint32_t samplers = 0;
    };
    
    ShaderReflection reflectShader(VkShaderModule module) const;
    ShaderReflection reflectSPIRV(const std::vector<uint32_t>& spirvCode) const;
    
    // Cache management
    void warmupCache(const std::vector<ShaderModuleSpec>& commonShaders);
    void optimizeCache(uint64_t currentFrame);
    void clearCache();
    
    // Debug and development features
    bool validateShader(VkShaderModule module) const;
    void dumpShaderDisassembly(VkShaderModule module, const std::string& outputPath) const;
    
    // Utility functions
    VkShaderStageFlagBits getShaderStageFromFilename(const std::string& filename) const;
    
    // Statistics and debugging
    struct ShaderStats {
        uint32_t totalShaders = 0;
        uint32_t cacheHits = 0;
        uint32_t cacheMisses = 0;
        uint32_t compilationsThisFrame = 0;
        uint32_t hotReloadsThisFrame = 0;
        std::chrono::nanoseconds totalCompilationTime{0};
        float hitRatio = 0.0f;
    };
    
    ShaderStats getStats() const { return stats; }
    void resetFrameStats();
    void debugPrintCache() const;
    
    // Shader source management
    std::string loadShaderSource(const std::string& filePath) const;
    bool saveCompiledShader(const std::string& outputPath, const std::vector<uint32_t>& spirvCode) const;
    
    // Include path management for GLSL
    void addIncludePath(const std::string& path);
    void removeIncludePath(const std::string& path);
    void clearIncludePaths();
    
    // Preprocessor define management
    void addGlobalDefine(const std::string& name, const std::string& value = "");
    void removeGlobalDefine(const std::string& name);
    void clearGlobalDefines();

private:
    // Core Vulkan objects
    const VulkanContext* context_ = nullptr;
    
    // Shader cache
    std::unordered_map<ShaderModuleSpec, std::unique_ptr<CachedShaderModule>, ShaderModuleSpecHash> shaderCache_;
    
    // Hot reload tracking
    std::unordered_map<std::string, std::vector<std::function<void(VkShaderModule)>>> reloadCallbacks_;
    std::unordered_map<std::string, std::filesystem::file_time_type> fileWatchList_;
    
    // Global include paths and defines
    std::vector<std::string> globalIncludePaths_;
    std::unordered_map<std::string, std::string> globalDefines_;
    
    // Statistics
    mutable ShaderStats stats;
    
    // Configuration
    bool hotReloadEnabled = false;
    uint32_t maxCacheSize_ = DEFAULT_SHADER_CACHE_SIZE;
    uint64_t cacheCleanupInterval_ = CACHE_CLEANUP_INTERVAL;
    
    // External compiler integration
    std::string glslcPath = "glslc";  // Path to glslc compiler
    std::string spirvOptPath = "spirv-opt";  // Path to SPIR-V optimizer
    
    // Internal shader creation
    std::unique_ptr<CachedShaderModule> createShaderInternal(const ShaderModuleSpec& spec);
    vulkan_raii::ShaderModule createVulkanShaderModule(const std::vector<uint32_t>& spirvCode);
    
    // SPIR-V loading and validation
    std::vector<uint32_t> loadSPIRVBinaryFromFile(const std::string& filePath) const;
    bool validateSPIRV(const std::vector<uint32_t>& spirvCode) const;
    
    // Compilation helpers
    std::vector<uint32_t> compileSPIRVWithGlslc(const std::string& source,
                                               VkShaderStageFlagBits stage,
                                               const std::string& fileName,
                                               const std::unordered_map<std::string, std::string>& defines) const;
    
    std::string getGlslcStageArgument(VkShaderStageFlagBits stage) const;
    std::vector<std::string> buildCompilerArguments(VkShaderStageFlagBits stage,
                                                   const std::string& fileName,
                                                   const std::unordered_map<std::string, std::string>& defines) const;
    
    // Cache management helpers
    void evictLeastRecentlyUsed();
    bool shouldEvictShader(const CachedShaderModule& shader, uint64_t currentFrame) const;
    
    // File system helpers
    bool fileExists(const std::string& path) const;
    std::filesystem::file_time_type getFileModifiedTime(const std::string& path) const;
    bool isFileNewer(const std::string& path, std::filesystem::file_time_type lastModified) const;
    
    // Reflection helpers (basic implementation)
    void performBasicReflection(CachedShaderModule& cachedModule) const;
    
    // Error handling and logging
    void logShaderCompilation(const ShaderModuleSpec& spec, 
                            std::chrono::nanoseconds compilationTime,
                            bool success) const;
    void logShaderError(const std::string& shaderPath, const std::string& error) const;
};

// Utility functions for common shader operations
namespace ShaderPresets {
    // Create common shader specs for your engine
    ShaderModuleSpec createVertexShaderSpec(const std::string& path);
    ShaderModuleSpec createFragmentShaderSpec(const std::string& path);
    ShaderModuleSpec createComputeShaderSpec(const std::string& path);
    
    // Entity rendering shaders (for your use case)
    ShaderModuleSpec createEntityVertexShaderSpec();
    ShaderModuleSpec createEntityFragmentShaderSpec();
    ShaderModuleSpec createEntityComputeShaderSpec();
    
    // Common graphics shaders
    ShaderModuleSpec createFullscreenTriangleVertexShaderSpec();
    ShaderModuleSpec createUIVertexShaderSpec();
    ShaderModuleSpec createUIFragmentShaderSpec();
    
    // Post-processing shaders
    ShaderModuleSpec createTonemappingFragmentShaderSpec();
    ShaderModuleSpec createBloomFragmentShaderSpec();
}

// Helper class for shader compilation with external tools
class ShaderCompiler {
public:
    static bool isGlslcAvailable();
    static bool isSpirvOptAvailable();
    
    static std::vector<uint32_t> compileGLSLToSPIRV(const std::string& source,
                                                   VkShaderStageFlagBits stage,
                                                   const std::string& fileName = "shader.glsl");
    
    static std::vector<uint32_t> optimizeSPIRV(const std::vector<uint32_t>& spirvCode);
    
    static bool validateSPIRV(const std::vector<uint32_t>& spirvCode);
};
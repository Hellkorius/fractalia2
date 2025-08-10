#include "compute_pipeline.h"
#include "vulkan_function_loader.h"
#include <iostream>
#include <fstream>
#include <stdexcept>

ComputePipeline::ComputePipeline() {
}

ComputePipeline::~ComputePipeline() {
    cleanup();
}

bool ComputePipeline::initialize(VulkanContext* context, VulkanFunctionLoader* loader) {
    this->context = context;
    this->loader = loader;
    
    if (!loader) {
        std::cerr << "ComputePipeline requires VulkanFunctionLoader" << std::endl;
        return false;
    }
    
    return true;
}

void ComputePipeline::cleanup() {
    if (movementPipeline != VK_NULL_HANDLE) {
        loader->vkDestroyPipeline(context->getDevice(), movementPipeline, nullptr);
        movementPipeline = VK_NULL_HANDLE;
    }
    
    if (movementPipelineLayout != VK_NULL_HANDLE) {
        loader->vkDestroyPipelineLayout(context->getDevice(), movementPipelineLayout, nullptr);
        movementPipelineLayout = VK_NULL_HANDLE;
    }
}

bool ComputePipeline::createMovementPipeline(VkDescriptorSetLayout descriptorSetLayout) {
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    
    // Add push constants for time and entity count
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(float) + sizeof(uint32_t); // deltaTime + entityCount
    
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (loader->vkCreatePipelineLayout(context->getDevice(), &pipelineLayoutInfo, nullptr, &movementPipelineLayout) != VK_SUCCESS) {
        std::cerr << "Failed to create compute pipeline layout!" << std::endl;
        return false;
    }

    // Load compute shader
    auto computeShaderCode = readFile("shaders/compiled/movement.spv");
    VkShaderModule computeShaderModule = createShaderModule(computeShaderCode);

    if (computeShaderModule == VK_NULL_HANDLE) {
        std::cerr << "Failed to create compute shader module!" << std::endl;
        return false;
    }

    // Create compute pipeline
    VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
    computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeShaderStageInfo.module = computeShaderModule;
    computeShaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = computeShaderStageInfo;
    pipelineInfo.layout = movementPipelineLayout;

    if (loader->vkCreateComputePipelines(context->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &movementPipeline) != VK_SUCCESS) {
        std::cerr << "Failed to create compute pipeline!" << std::endl;
        loader->vkDestroyShaderModule(context->getDevice(), computeShaderModule, nullptr);
        return false;
    }

    loader->vkDestroyShaderModule(context->getDevice(), computeShaderModule, nullptr);
    return true;
}


VkShaderModule ComputePipeline::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (loader->vkCreateShaderModule(context->getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        std::cerr << "Failed to create shader module!" << std::endl;
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}

std::vector<char> ComputePipeline::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}
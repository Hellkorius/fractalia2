#include "vulkan_pipeline.h"
#include "vulkan_function_loader.h"
#include "vulkan_utils.h"
#include "../ecs/gpu_entity_manager.h"
#include <iostream>
#include <fstream>
#include <array>
#include <cstddef>
#include <glm/glm.hpp>

VulkanPipeline::VulkanPipeline() {
}

VulkanPipeline::~VulkanPipeline() {
    cleanup();
}

bool VulkanPipeline::initialize(VulkanContext* context, VkFormat swapChainImageFormat, VulkanFunctionLoader* loader) {
    this->context = context;
    this->loader = loader;
    
    if (!loader) {
        std::cerr << "VulkanPipeline requires VulkanFunctionLoader" << std::endl;
        return false;
    }
    
    std::cout << "Creating descriptor set layout..." << std::endl;
    if (!createDescriptorSetLayout()) {
        std::cerr << "Failed to create descriptor set layout" << std::endl;
        return false;
    }
    std::cout << "Descriptor set layout created successfully" << std::endl;
    
    std::cout << "Creating render pass..." << std::endl;
    if (!createRenderPass(swapChainImageFormat)) {
        std::cerr << "Failed to create render pass" << std::endl;
        return false;
    }
    std::cout << "Render pass created successfully" << std::endl;
    
    std::cout << "Creating graphics pipeline..." << std::endl;
    if (!createGraphicsPipeline()) {
        std::cerr << "Failed to create graphics pipeline" << std::endl;
        return false;
    }
    std::cout << "Graphics pipeline created successfully" << std::endl;
    
    return true;
}

void VulkanPipeline::cleanup() {
    if (!loader || !context) return;
    
    if (graphicsPipeline != VK_NULL_HANDLE) {
        loader->vkDestroyPipeline(context->getDevice(), graphicsPipeline, nullptr);
        graphicsPipeline = VK_NULL_HANDLE;
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        loader->vkDestroyPipelineLayout(context->getDevice(), pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }
    if (renderPass != VK_NULL_HANDLE) {
        loader->vkDestroyRenderPass(context->getDevice(), renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
    }
    if (descriptorSetLayout != VK_NULL_HANDLE) {
        loader->vkDestroyDescriptorSetLayout(context->getDevice(), descriptorSetLayout, nullptr);
        descriptorSetLayout = VK_NULL_HANDLE;
    }
}

bool VulkanPipeline::recreate(VkFormat swapChainImageFormat) {
    cleanup();
    
    if (!createDescriptorSetLayout()) {
        std::cerr << "Failed to recreate descriptor set layout!" << std::endl;
        return false;
    }
    
    if (!createRenderPass(swapChainImageFormat)) {
        std::cerr << "Failed to recreate render pass!" << std::endl;
        return false;
    }
    
    if (!createGraphicsPipeline()) {
        std::cerr << "Failed to recreate graphics pipeline!" << std::endl;
        return false;
    }

    return true;
}

bool VulkanPipeline::createRenderPass(VkFormat swapChainImageFormat) {
    std::array<VkAttachmentDescription, 3> attachments{};
    
    attachments[0].format = swapChainImageFormat;
    attachments[0].samples = VK_SAMPLE_COUNT_2_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    attachments[1].format = swapChainImageFormat;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    attachments[2].format = VK_FORMAT_D32_SFLOAT;
    attachments[2].samples = VK_SAMPLE_COUNT_2_BIT;
    attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[2].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference msaaColorAttachmentRef{};
    msaaColorAttachmentRef.attachment = 0;
    msaaColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference resolveColorAttachmentRef{};
    resolveColorAttachmentRef.attachment = 1;
    resolveColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 2;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &msaaColorAttachmentRef;
    subpass.pResolveAttachments = &resolveColorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    
    std::array<VkSubpassDependency, 2> dependencies{};
    
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = 0;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    
    dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].dstSubpass = 0;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].srcAccessMask = 0;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    if (loader->vkCreateRenderPass(context->getDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        std::cerr << "Failed to create render pass" << std::endl;
        return false;
    }

    return true;
}

bool VulkanPipeline::createDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 1> bindings{};
    
    // UBO binding (binding = 0)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[0].pImmutableSamplers = nullptr;
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = bindings.size();
    layoutInfo.pBindings = bindings.data();
    
    if (loader->vkCreateDescriptorSetLayout(context->getDevice(), &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "Failed to create descriptor set layout!" << std::endl;
        return false;
    }
    
    return true;
}

std::array<VkVertexInputBindingDescription, 2> VulkanPipeline::getVertexBindingDescriptions() {
    std::array<VkVertexInputBindingDescription, 2> bindingDescriptions{};
    
    bindingDescriptions[0].binding = 0;
    bindingDescriptions[0].stride = sizeof(glm::vec3) + sizeof(glm::vec3);
    bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    bindingDescriptions[1].binding = 1;
    bindingDescriptions[1].stride = sizeof(GPUEntity);
    bindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    
    return bindingDescriptions;
}

std::array<VkVertexInputAttributeDescription, 10> VulkanPipeline::getVertexAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 10> attributeDescriptions{};
    
    // Vertex attributes (binding 0)
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = 0;
    
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = sizeof(glm::vec3);
    
    // GPUEntity instance attributes (binding 1) - matches GPUEntity struct layout
    // modelMatrix - locations 2-5 (64 bytes)
    attributeDescriptions[2].binding = 1;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(GPUEntity, modelMatrix) + 0 * sizeof(glm::vec4);
    
    attributeDescriptions[3].binding = 1;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[3].offset = offsetof(GPUEntity, modelMatrix) + 1 * sizeof(glm::vec4);
    
    attributeDescriptions[4].binding = 1;
    attributeDescriptions[4].location = 4;
    attributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[4].offset = offsetof(GPUEntity, modelMatrix) + 2 * sizeof(glm::vec4);
    
    attributeDescriptions[5].binding = 1;
    attributeDescriptions[5].location = 5;
    attributeDescriptions[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[5].offset = offsetof(GPUEntity, modelMatrix) + 3 * sizeof(glm::vec4);
    
    // color - location 6 (16 bytes)
    attributeDescriptions[6].binding = 1;
    attributeDescriptions[6].location = 6;
    attributeDescriptions[6].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[6].offset = offsetof(GPUEntity, color);
    
    // movementParams0 - location 7 (16 bytes)
    attributeDescriptions[7].binding = 1;
    attributeDescriptions[7].location = 7;
    attributeDescriptions[7].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[7].offset = offsetof(GPUEntity, movementParams0);
    
    // movementParams1 - location 8 (16 bytes)
    attributeDescriptions[8].binding = 1;
    attributeDescriptions[8].location = 8;
    attributeDescriptions[8].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[8].offset = offsetof(GPUEntity, movementParams1);
    
    // runtimeState - location 9 (16 bytes)
    attributeDescriptions[9].binding = 1;
    attributeDescriptions[9].location = 9;
    attributeDescriptions[9].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[9].offset = offsetof(GPUEntity, runtimeState);
    
    return attributeDescriptions;
}

bool VulkanPipeline::createGraphicsPipeline() {
    std::vector<char> vertShaderCode, fragShaderCode;
    
    try {
        vertShaderCode = VulkanUtils::readFile("shaders/compiled/vertex.spv");
        fragShaderCode = VulkanUtils::readFile("shaders/compiled/fragment.spv");
    } catch (const std::exception& e) {
        std::cerr << "Failed to load shader files: " << e.what() << std::endl;
        return false;
    }

    VkShaderModule vertShaderModule = VulkanUtils::createShaderModule(context->getDevice(), *loader, vertShaderCode);
    VkShaderModule fragShaderModule = VulkanUtils::createShaderModule(context->getDevice(), *loader, fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    auto bindingDescriptions = getVertexBindingDescriptions();
    auto attributeDescriptions = getVertexAttributeDescriptions();
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_2_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    if (loader->vkCreatePipelineLayout(context->getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline layout" << std::endl;
        return false;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (loader->vkCreateGraphicsPipelines(context->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        std::cerr << "Failed to create graphics pipeline" << std::endl;
        return false;
    }

    loader->vkDestroyShaderModule(context->getDevice(), fragShaderModule, nullptr);
    loader->vkDestroyShaderModule(context->getDevice(), vertShaderModule, nullptr);

    return true;
}

// Note: loadFunctions removed - now using centralized VulkanFunctionLoader

// Note: createShaderModule and readFile removed - now using VulkanUtils
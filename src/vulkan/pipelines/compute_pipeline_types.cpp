#include "compute_pipeline_types.h"
#include "hash_utils.h"
#include <cmath>

// ComputePipelineState implementation
bool ComputePipelineState::operator==(const ComputePipelineState& other) const {
    // Manual comparison for pushConstantRanges since VkPushConstantRange doesn't have operator==
    if (pushConstantRanges.size() != other.pushConstantRanges.size()) {
        return false;
    }
    for (size_t i = 0; i < pushConstantRanges.size(); ++i) {
        const auto& a = pushConstantRanges[i];
        const auto& b = other.pushConstantRanges[i];
        if (a.stageFlags != b.stageFlags || a.offset != b.offset || a.size != b.size) {
            return false;
        }
    }
    
    return shaderPath == other.shaderPath &&
           specializationConstants == other.specializationConstants &&
           descriptorSetLayouts == other.descriptorSetLayouts &&
           workgroupSizeX == other.workgroupSizeX &&
           workgroupSizeY == other.workgroupSizeY &&
           workgroupSizeZ == other.workgroupSizeZ;
}

size_t ComputePipelineState::getHash() const {
    VulkanHash::HashCombiner hasher;
    
    hasher.combine(shaderPath)
          .combineContainer(specializationConstants)
          .combineContainer(descriptorSetLayouts)
          .combine(workgroupSizeX)
          .combine(workgroupSizeY)
          .combine(workgroupSizeZ);
    
    for (const auto& range : pushConstantRanges) {
        hasher.combine(range.stageFlags)
              .combine(range.offset)
              .combine(range.size);
    }
    
    return hasher.get();
}

// ComputeDispatch implementation
void ComputeDispatch::calculateOptimalDispatch(uint32_t dataSize, const glm::uvec3& workgroupSize) {
    // Calculate number of workgroups needed
    uint32_t elementsPerWorkgroup = workgroupSize.x * workgroupSize.y * workgroupSize.z;
    groupCountX = (dataSize + elementsPerWorkgroup - 1) / elementsPerWorkgroup;
    groupCountY = 1;
    groupCountZ = 1;
    
    // For very large dispatches, consider using 2D or 3D layout for better cache performance
    if (groupCountX > 65535) {  // Vulkan limit
        uint32_t sqrtGroups = static_cast<uint32_t>(std::sqrt(groupCountX));
        groupCountX = sqrtGroups;
        groupCountY = (dataSize + (sqrtGroups * elementsPerWorkgroup) - 1) / (sqrtGroups * elementsPerWorkgroup);
        
        if (groupCountY > 65535) {
            uint32_t cbrtGroups = static_cast<uint32_t>(std::cbrt(dataSize / elementsPerWorkgroup));
            groupCountX = cbrtGroups;
            groupCountY = cbrtGroups;
            groupCountZ = (dataSize + (cbrtGroups * cbrtGroups * elementsPerWorkgroup) - 1) / 
                         (cbrtGroups * cbrtGroups * elementsPerWorkgroup);
        }
    }
}
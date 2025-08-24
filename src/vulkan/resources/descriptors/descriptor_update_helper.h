#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <array>

class VulkanContext;

/**
 * DescriptorUpdateHelper - Pure utility for descriptor set updates (DRY principle)
 * 
 * Single Responsibility: Eliminate code duplication in descriptor set updates
 * - Provides templated helpers for common descriptor update patterns
 * - Handles VkWriteDescriptorSet array construction
 * - Manages buffer info arrays and validation
 * - No state, no lifecycle - pure utility functions
 */
class DescriptorUpdateHelper {
public:
    // Descriptor binding specification
    struct BufferBinding {
        uint32_t binding;
        VkBuffer buffer;
        VkDeviceSize offset = 0;
        VkDeviceSize range = VK_WHOLE_SIZE;
        VkDescriptorType type;
        
        BufferBinding(uint32_t binding, VkBuffer buffer, VkDescriptorType type)
            : binding(binding), buffer(buffer), type(type) {}
            
        BufferBinding(uint32_t binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range, VkDescriptorType type)
            : binding(binding), buffer(buffer), offset(offset), range(range), type(type) {}
    };

    // Single descriptor set update with multiple bindings
    static bool updateDescriptorSet(
        const VulkanContext& context,
        VkDescriptorSet descriptorSet,
        const std::vector<BufferBinding>& bindings
    );

    // Multiple descriptor sets with same binding pattern (for MAX_FRAMES_IN_FLIGHT)
    template<size_t N>
    static bool updateDescriptorSets(
        const VulkanContext& context,
        const std::array<VkDescriptorSet, N>& descriptorSets,
        const std::vector<BufferBinding>& bindingTemplate
    ) {
        for (size_t i = 0; i < N; ++i) {
            if (!updateDescriptorSet(context, descriptorSets[i], bindingTemplate)) {
                return false;
            }
        }
        return true;
    }

    // Specialized helper for per-frame uniform buffer updates
    template<size_t N>
    static bool updateUniformBufferBinding(
        const VulkanContext& context,
        const std::array<VkDescriptorSet, N>& descriptorSets,
        uint32_t binding,
        const std::array<VkBuffer, N>& uniformBuffers,
        VkDeviceSize bufferSize
    ) {
        for (size_t i = 0; i < N; ++i) {
            BufferBinding uniformBinding(binding, uniformBuffers[i], 0, bufferSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            std::vector<BufferBinding> bindings = {uniformBinding};
            
            if (!updateDescriptorSet(context, descriptorSets[i], bindings)) {
                return false;
            }
        }
        return true;
    }

    // Array binding support for unified descriptor indexing system
    static bool updateDescriptorSetWithBufferArray(
        const VulkanContext& context,
        VkDescriptorSet descriptorSet,
        uint32_t binding,
        const std::vector<VkBuffer>& buffers,
        VkDescriptorType descriptorType,
        VkDeviceSize bufferSize = VK_WHOLE_SIZE
    );
    
    // Validation helpers
    static bool validateBinding(const BufferBinding& binding);
    static bool validateDescriptorSet(VkDescriptorSet descriptorSet);

private:
    // No instantiation - pure utility class
    DescriptorUpdateHelper() = delete;
    ~DescriptorUpdateHelper() = delete;
};
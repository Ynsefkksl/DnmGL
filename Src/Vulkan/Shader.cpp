#include "DnmGL/Vulkan/Shader.hpp"

#include <spirv_reflect.h>

#include <fstream>

namespace DnmGL::Vulkan {
    static std::vector<uint32_t> LoadShaderFile(const std::filesystem::path& filePath) {
        std::ifstream file(filePath, std::ios::ate | std::ios::binary);

        const auto fileSize = file.tellg();
        std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t)); //SPIR-V consists of 32 bit words 

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

        file.close();

        return buffer;
    }

    static constexpr DnmGL::ResourceType GetResourceType(const SpvReflectDescriptorBinding *binding) {
        switch (binding->resource_type) {
            case SPV_REFLECT_RESOURCE_FLAG_UNDEFINED: return {};
            case SPV_REFLECT_RESOURCE_FLAG_SAMPLER: return DnmGL::ResourceType::eSampler;
            case SPV_REFLECT_RESOURCE_FLAG_CBV: return DnmGL::ResourceType::eUniformBuffer;
            case SPV_REFLECT_RESOURCE_FLAG_SRV: return (binding->descriptor_type == SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER) 
                        ? DnmGL::ResourceType::eReadonlyBuffer : DnmGL::ResourceType::eReadonlyImage;
            case SPV_REFLECT_RESOURCE_FLAG_UAV:
                return (binding->descriptor_type == SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER) 
                        ? DnmGL::ResourceType::eWritableBuffer : DnmGL::ResourceType::eWritableImage;
        }
    }

    Shader::Shader(Vulkan::Context& ctx, std::string_view filename)
        : DnmGL::Shader(ctx, filename) {
        //create shader reflection
        spv_reflect::ShaderModule reflection(LoadShaderFile(context->GetShaderPath(filename)));
        
        m_vk_entry_points.reserve(reflection.GetEntryPointCount());

        {
            uint32_t count;
            reflection.EnumerateDescriptorBindings(&count, nullptr);
            std::vector<SpvReflectDescriptorBinding *> bindings(count);
            reflection.EnumerateDescriptorBindings(&count, bindings.data());

            for (const auto *binding : bindings) {
                const uint32_t set_index = std::floor(binding->binding / 256);
                const uint32_t binding_index = binding->binding - (256 * set_index);
                reflection.ChangeDescriptorBindingNumbers(binding, binding_index, set_index);
            }
        }

        for (const auto entry_index : Counter(reflection.GetEntryPointCount())) {
            const auto entry_point_name = static_cast<std::string_view>(reflection.GetEntryPointName(entry_index));
            auto& vk_entry_point_info = m_vk_entry_points.emplace_back();
            auto& entry_point_info = m_entry_point_infos.emplace_back();
            
            vk_entry_point_info.name = entry_point_name;
            entry_point_info.name = entry_point_name;
            vk_entry_point_info.stage = static_cast<vk::ShaderStageFlagBits>(reflection.GetEntryPointShaderStage(entry_index));
                                            //same bit with vulkan
            entry_point_info.shader_stage = static_cast<DnmGL::ShaderStageBits>(reflection.GetEntryPointShaderStage(entry_index));

            // Readonly resource
            {
                const auto *dst_set = reflection.GetEntryPointDescriptorSet(entry_point_name.data(), 0);
                if (dst_set != nullptr) {
                    const auto bindings = std::span(dst_set->bindings, dst_set->binding_count);
    
                    vk_entry_point_info.readonly_resources.reserve(dst_set->binding_count);
                    for (const auto *binding : bindings) {
                        vk_entry_point_info.readonly_resources.emplace_back(VkBindingInfo{
                            .binding = binding->binding,
                            .descriptor_count = binding->count,
                            .type = (vk::DescriptorType)binding->descriptor_type
                        });
                    }
                    
                    vk_entry_point_info.access_flags |= vk::AccessFlagBits::eShaderRead;
    
                    //DnmGL reflection
                    {
                        entry_point_info.readonly_resources.reserve(dst_set->binding_count);
                        for (const auto *binding : bindings) {
                            entry_point_info.readonly_resources.emplace_back(
                                binding->binding,
                                binding->count,
                                GetResourceType(binding)
                            );
                        }
                    }
                }
            }

            // Writable resource
            {
                const auto *dst_set = reflection.GetEntryPointDescriptorSet(entry_point_name.data(), 1);
                if (dst_set != nullptr) {
                    const auto bindings = std::span(dst_set->bindings, dst_set->binding_count);

                    vk_entry_point_info.writable_resources.reserve(dst_set->binding_count);
                    for (const auto *binding : bindings) {
                        vk_entry_point_info.writable_resources.emplace_back(VkBindingInfo{
                            .binding = binding->binding,
                            .descriptor_count = binding->count,
                            .type = (vk::DescriptorType)binding->descriptor_type
                        });
                    }
                    
                    vk_entry_point_info.access_flags |= vk::AccessFlagBits::eShaderWrite;

                    //DnmGL reflection
                    {
                        entry_point_info.writable_resources.reserve(dst_set->binding_count);
                        for (const auto *binding : bindings) {
                            entry_point_info.writable_resources.emplace_back(
                                binding->binding,
                                binding->count,
                                GetResourceType(binding)
                            );
                        }
                    }
                }
            }

            // uniform buffer resource
            {
                const auto *dst_set = reflection.GetEntryPointDescriptorSet(entry_point_name.data(), 2);
                if (dst_set != nullptr) {
                    const auto bindings = std::span(dst_set->bindings, dst_set->binding_count);

                    vk_entry_point_info.uniform_buffer_resources.reserve(dst_set->binding_count);
                    for (const auto *binding : bindings) {
                        vk_entry_point_info.uniform_buffer_resources.emplace_back(VkBindingInfo{
                            .binding = binding->binding,
                            .descriptor_count = binding->count,
                            .type = vk::DescriptorType::eUniformBuffer
                        });
                    }

                    vk_entry_point_info.access_flags |= vk::AccessFlagBits::eUniformRead;

                    //DnmGL reflection
                    {
                        entry_point_info.uniform_buffer_resources.reserve(dst_set->binding_count);
                        for (const auto *binding : bindings) {
                            entry_point_info.uniform_buffer_resources.emplace_back(
                                binding->binding,
                                binding->count,
                                GetResourceType(binding)
                            );
                        }
                    }
                }
            }

            // sampler resource
            {
                const auto *dst_set = reflection.GetEntryPointDescriptorSet(entry_point_name.data(), 3);
                if (dst_set != nullptr) {
                    const auto bindings = std::span(dst_set->bindings, dst_set->binding_count);

                    vk_entry_point_info.sampler_resources.reserve(dst_set->binding_count);
                    for (const auto *binding : bindings) {
                        vk_entry_point_info.sampler_resources.emplace_back(VkBindingInfo{
                            .binding = binding->binding,
                            .descriptor_count = binding->count,
                            .type = vk::DescriptorType::eSampler
                        });
                    }

                    //DnmGL reflection
                    {
                        entry_point_info.sampler_resources.reserve(dst_set->binding_count);
                        for (const auto *binding : bindings) {
                            entry_point_info.sampler_resources.emplace_back(
                                binding->binding,
                                binding->count,
                                GetResourceType(binding)
                            );
                        }
                    }
                }
            }

            //push constants
            {
                uint32_t count;
                reflection.EnumerateEntryPointPushConstantBlocks(entry_point_name.data(), &count, nullptr);
            
                std::vector<SpvReflectBlockVariable *> pushConstants(count);
                reflection.EnumerateEntryPointPushConstantBlocks(entry_point_name.data(), &count, pushConstants.data());
    
                if (count != 0) {
                    vk_entry_point_info.push_constant = PushConstantInfo {
                    pushConstants[0]->size,
                    pushConstants[0]->offset,
                    };

                    entry_point_info.push_constant = PushConstantInfo {
                    pushConstants[0]->size,
                    pushConstants[0]->offset,
                    };

                    DnmGLAssert(
                        pushConstants[0]->offset + pushConstants[0]->size < 128, 
                        "push constant offset + size should be small 128 byte")
                }
            }
        }
        
        //create shader module
        m_shader_module = VulkanContext->GetDevice().createShaderModule(
            vk::ShaderModuleCreateInfo{}
                .setPCode(reflection.GetCode())
                .setCodeSize(reflection.GetCodeSize())
        );
    }
}
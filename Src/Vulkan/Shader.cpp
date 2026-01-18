#include "DnmGL/Vulkan/Shader.hpp"

#include <spirv_reflect.h>

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

    static constexpr DnmGL::ShaderStageBits GetShaderStage(SpvReflectShaderStageFlagBits spv_stage) {
        switch (spv_stage) {
            case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT: return ShaderStageBits::eVertex;
            case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT: return ShaderStageBits::eFragment;
            case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT: return ShaderStageBits::eCompute;

            case SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
            case SPV_REFLECT_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
            case SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT:
            case SPV_REFLECT_SHADER_STAGE_TASK_BIT_NV:
            case SPV_REFLECT_SHADER_STAGE_MESH_BIT_NV:
            case SPV_REFLECT_SHADER_STAGE_RAYGEN_BIT_KHR:
            case SPV_REFLECT_SHADER_STAGE_ANY_HIT_BIT_KHR:
            case SPV_REFLECT_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
            case SPV_REFLECT_SHADER_STAGE_MISS_BIT_KHR:
            case SPV_REFLECT_SHADER_STAGE_INTERSECTION_BIT_KHR:
            case SPV_REFLECT_SHADER_STAGE_CALLABLE_BIT_KHR: return ShaderStageBits::eNone;
        }
    }

    Shader::Shader(Vulkan::Context& ctx, std::string_view filename)
        : DnmGL::Shader(ctx, filename) {
        //create shader reflection
        spv_reflect::ShaderModule reflection(LoadShaderFile(context->GetShaderPath(filename)));
        
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
            auto& entry_point_info = m_entry_point_infos.emplace_back();
            
                                            //same bit with vulkan
            entry_point_info.shader_stage = GetShaderStage(reflection.GetEntryPointShaderStage(entry_index));
            entry_point_info.name = entry_point_name;            
            DnmGLAssert(entry_point_info.shader_stage != ShaderStageBits::eNone, 
                "unsupported shader stage; entry point: {}", entry_point_name);

            // Readonly resource
            {
                const auto *dst_set = reflection.GetEntryPointDescriptorSet(entry_point_name.data(), 0);
                if (dst_set != nullptr) {
                    const auto bindings = std::span(dst_set->bindings, dst_set->binding_count);
    
                    //DnmGL reflection
                    {
                        entry_point_info.readonly_resources.reserve(dst_set->binding_count);
                        for (const auto *binding : bindings) {
                            entry_point_info.readonly_resources.emplace_back(
                                binding->binding,
                                binding->count,
                                (binding->descriptor_type == SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER) 
                                    ? DnmGL::ResourceType::eReadonlyBuffer : DnmGL::ResourceType::eReadonlyImage
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

                    //DnmGL reflection
                    {
                        entry_point_info.writable_resources.reserve(dst_set->binding_count);
                        for (const auto *binding : bindings) {
                            entry_point_info.writable_resources.emplace_back(
                                binding->binding,
                                binding->count,
                                (binding->descriptor_type == SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER) 
                                    ? DnmGL::ResourceType::eWritableBuffer : DnmGL::ResourceType::eWritableImage
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

                    //DnmGL reflection
                    {
                        entry_point_info.uniform_buffer_resources.reserve(dst_set->binding_count);
                        for (const auto *binding : bindings) {
                            entry_point_info.uniform_buffer_resources.emplace_back(
                                binding->binding,
                                binding->count,
                                DnmGL::ResourceType::eUniformBuffer
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

                    //DnmGL reflection
                    {
                        entry_point_info.sampler_resources.reserve(dst_set->binding_count);
                        for (const auto *binding : bindings) {
                            entry_point_info.sampler_resources.emplace_back(
                                binding->binding,
                                binding->count,
                                DnmGL::ResourceType::eSampler
                            );
                        }
                    }
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
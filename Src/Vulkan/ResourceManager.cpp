#include "DnmGL/Vulkan/ResourceManager.hpp"
#include "DnmGL/Vulkan/Shader.hpp"
#include "DnmGL/Vulkan/Image.hpp"
#include "DnmGL/Vulkan/Buffer.hpp"
#include "DnmGL/Vulkan/Sampler.hpp"

namespace DnmGL::Vulkan {
    static constexpr vk::ShaderStageFlagBits GetVkShaderStage(ShaderStageBits stage) {
        switch (stage) {
            // ShaderStageBits::eNone handled in Shader constructer
            case ShaderStageBits::eNone: std::unreachable();
            case ShaderStageBits::eVertex: return vk::ShaderStageFlagBits::eVertex;
            case ShaderStageBits::eFragment: return vk::ShaderStageFlagBits::eFragment;
            case ShaderStageBits::eCompute: return vk::ShaderStageFlagBits::eCompute;
        }
    }

    static constexpr vk::DescriptorType GetVkDescriptorType(ResourceType type) {
        switch (type) {
            // ResourceType::eNone handled in Shader constructer
            case ResourceType::eNone: std::unreachable();
            case ResourceType::eReadonlyBuffer: return vk::DescriptorType::eStorageBuffer;
            //I did this relying on SDLGPU.
            case ResourceType::eReadonlyImage: return vk::DescriptorType::eSampledImage;
            case ResourceType::eWritableBuffer: return vk::DescriptorType::eStorageBuffer;
            case ResourceType::eWritableImage: return vk::DescriptorType::eStorageImage;
            case ResourceType::eUniformBuffer: return vk::DescriptorType::eUniformBuffer;
            case ResourceType::eSampler: return vk::DescriptorType::eSampler;
          break;
        }
    }

    ResourceManager::ResourceManager(DnmGL::Vulkan::Context& ctx, std::span<const DnmGL::Shader*> shaders)
        : DnmGL::ResourceManager(ctx, shaders) {
        const auto device = VulkanContext->GetDevice();

        std::vector<vk::DescriptorSetLayoutBinding> readonly_bindings{};
        std::vector<vk::DescriptorSetLayoutBinding> writable_bindings{};
        std::vector<vk::DescriptorSetLayoutBinding> uniform_bindings{};
        std::vector<vk::DescriptorSetLayoutBinding> sampler_bindings{};

        for (const auto* shader : shaders) {
            const auto* typed_shader = reinterpret_cast<const Shader*>(shader);
            for (const auto& entry_point :  typed_shader->GetEntryPoints()) {
                for (const auto& shader_binding : entry_point.readonly_resources) {
                    const auto it = std::ranges::find_if(
                            readonly_bindings,
                            [&shader_binding] (vk::DescriptorSetLayoutBinding& binding) {
                            return binding.binding == shader_binding.binding;
                        });

                    //if this binding exists
                    if (it != readonly_bindings.end()) {
                        it->stageFlags |= GetVkShaderStage(entry_point.shader_stage);
                        continue;
                    }

                    readonly_bindings.emplace_back(
                        shader_binding.binding,
                        GetVkDescriptorType(shader_binding.resource_type),
                        shader_binding.resource_count,
                        GetVkShaderStage(entry_point.shader_stage),
                        nullptr
                    );

                    m_readonly_resource_bindings.emplace_back(shader_binding);
                }

                for (const auto& shader_binding : entry_point.writable_resources) {
                    const auto it = std::ranges::find_if(
                            writable_bindings,
                            [&shader_binding] (vk::DescriptorSetLayoutBinding& binding) {
                            return binding.binding == shader_binding.binding;
                        });

                    //if this binding exists
                    if (it != writable_bindings.end()) {
                        it->stageFlags |= GetVkShaderStage(entry_point.shader_stage);
                        continue;
                    }

                    writable_bindings.emplace_back(
                        shader_binding.binding,
                        GetVkDescriptorType(shader_binding.resource_type),
                        shader_binding.resource_count,
                        GetVkShaderStage(entry_point.shader_stage),
                        nullptr
                    );

                    m_writable_resource_bindings.emplace_back(shader_binding);
                }

                for (const auto& shader_binding : entry_point.uniform_buffer_resources) {
                    const auto it = std::ranges::find_if(
                            uniform_bindings,
                            [&shader_binding] (vk::DescriptorSetLayoutBinding& binding) {
                            return binding.binding == shader_binding.binding;
                        });

                    //if this binding exists
                    if (it != uniform_bindings.end()) {
                        it->stageFlags |= GetVkShaderStage(entry_point.shader_stage);
                        continue;
                    }

                    uniform_bindings.emplace_back(
                        shader_binding.binding,
                        GetVkDescriptorType(shader_binding.resource_type),
                        shader_binding.resource_count,
                        GetVkShaderStage(entry_point.shader_stage),
                        nullptr
                    );

                    m_uniform_resource_bindings.emplace_back(shader_binding);
                }

                for (const auto& shader_binding : entry_point.sampler_resources) {
                    const auto it = std::ranges::find_if(
                            sampler_bindings,
                            [&shader_binding] (vk::DescriptorSetLayoutBinding& binding) {
                            return binding.binding == shader_binding.binding;
                        });

                    //if this binding exists
                    if (it != sampler_bindings.end()) {
                        it->stageFlags |= GetVkShaderStage(entry_point.shader_stage);
                        continue;
                    }

                    sampler_bindings.emplace_back(
                        shader_binding.binding,
                        GetVkDescriptorType(shader_binding.resource_type),
                        shader_binding.resource_count,
                        GetVkShaderStage(entry_point.shader_stage),
                        nullptr
                    );

                    m_sampler_resource_bindings.emplace_back(shader_binding);
                }
            }
        }

        m_dst_set_layouts[0] = device.createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo{}
            .setBindings(readonly_bindings)
            .setFlags({})
        );

        m_dst_set_layouts[1] = device.createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo{}
            .setBindings(writable_bindings)
            .setFlags({})
        );

        m_dst_set_layouts[2] = device.createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo{}
            .setBindings(uniform_bindings)
            .setFlags({})
        );

        m_dst_set_layouts[3] = device.createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo{}
            .setBindings(sampler_bindings)
            .setFlags({})
        );

        {
            vk::DescriptorSetAllocateInfo alloc_info;
            alloc_info.setSetLayouts(m_dst_set_layouts)
                        .setDescriptorPool(VulkanContext->GetDescriptorPool())
                        ;

            std::ranges::copy(
                device.allocateDescriptorSets(alloc_info), 
                m_dst_sets.begin());
        }
    }

    void ResourceManager::ISetReadonlyResource(std::span<const ResourceDesc> update_resource) {        
        const auto supported_features = VulkanContext->GetSupportedFeatures();
        const auto context_state = VulkanContext->GetContextState();

        // usually these three are supported together
        const bool buffer_uab = supported_features.storage_buffer_update_after_bind;
        const bool image_uab = supported_features.storage_image_update_after_bind;
        const bool texture_uab = supported_features.sampled_image_update_after_bind;

        if ((context_state == ContextState::eNone || context_state == ContextState::eCommandBufferRecording)
        || (buffer_uab && image_uab && texture_uab)) {
            std::vector<vk::WriteDescriptorSet> writes{};
            std::vector<vk::DescriptorImageInfo> image_infos{};
            std::vector<vk::DescriptorBufferInfo> buffer_infos{};

            writes.reserve(update_resource.size());
            image_infos.reserve(update_resource.size());
            buffer_infos.reserve(update_resource.size());

            vk::DescriptorBufferInfo *buffer_info{};
            vk::DescriptorImageInfo *image_info{};
            vk::DescriptorType type;

            for (const auto &resource : update_resource) {
                if (resource.buffer) {
                    const auto *typed_buffer
                        = static_cast<const Vulkan::Buffer *>(resource.buffer);

                    buffer_info = &buffer_infos.emplace_back(
                        typed_buffer->GetBuffer(),
                        resource.offset,
                        resource.size
                    );

                    type = vk::DescriptorType::eStorageBuffer;
                }
                else if (resource.image) {
                    auto *typed_image
                        = static_cast<Vulkan::Image *>(resource.image);

                    image_info = &image_infos.emplace_back(
                        nullptr,
                        typed_image->CreateGetImageView(resource.subresource),
                        typed_image->GetIdealImageLayout()
                    );

                    //I did this relying on SDLGPU.
                    type = vk::DescriptorType::eSampledImage;
                }
                else continue;
                
                writes.emplace_back(
                    GetReadonlySet(),
                    resource.binding,
                    resource.array_element,
                    1,
                    type,
                    image_info,
                    buffer_info,
                    nullptr
                );
            }

            VulkanContext->GetDevice().updateDescriptorSets(writes, {});
        }
        else {
            std::vector<Context::InternalBufferResource> buffer_defer_updates{};
            std::vector<Context::InternalImageResource> image_defer_updates{};

            buffer_defer_updates.reserve(update_resource.size());
            image_defer_updates.reserve(update_resource.size());

            for (const auto &resource : update_resource) {
                if (resource.buffer) {
                    const auto *typed_buffer
                        = static_cast<const Vulkan::Buffer *>(resource.buffer);
    
                    buffer_defer_updates.emplace_back(
                        typed_buffer->GetBuffer(),
                        vk::DescriptorType::eStorageBuffer,
                        resource.offset,
                        resource.size,
                        GetReadonlySet(),
                        resource.binding,       
                        resource.array_element
                    );
                }
                else if (resource.image) {
                    auto *typed_image
                        = static_cast<Vulkan::Image *>(resource.image);
    
                    image_defer_updates.emplace_back(
                        typed_image->CreateGetImageView(resource.subresource),
                        typed_image->GetIdealImageLayout(),
                        vk::DescriptorType::eSampledImage, //TODO: fix this
                        GetReadonlySet(),
                        resource.binding,       
                        resource.array_element
                    );
                }
            }

            if(!buffer_defer_updates.empty())
                VulkanContext->DeferResourceUpdate(buffer_defer_updates);
            if(!image_defer_updates.empty())
                VulkanContext->DeferResourceUpdate(image_defer_updates);
        }
    }

    void ResourceManager::ISetWritableResource(std::span<const ResourceDesc> update_resource) {
        const auto supported_features = VulkanContext->GetSupportedFeatures();
        const auto context_state = VulkanContext->GetContextState();

        // usually these 2 are supported together
        const bool buffer_uab = supported_features.storage_buffer_update_after_bind;
        const bool image_uab = supported_features.storage_image_update_after_bind;

        if ((context_state == ContextState::eNone || context_state == ContextState::eCommandBufferRecording)
        || (buffer_uab && image_uab)) {
            std::vector<vk::WriteDescriptorSet> writes{};
            std::vector<vk::DescriptorImageInfo> image_infos{};
            std::vector<vk::DescriptorBufferInfo> buffer_infos{};

            writes.reserve(update_resource.size());
            image_infos.reserve(update_resource.size());
            buffer_infos.reserve(update_resource.size());

            vk::DescriptorBufferInfo *buffer_info{};
            vk::DescriptorImageInfo *image_info{};
            vk::DescriptorType type;

            for (const auto &resource : update_resource) {
                if (resource.buffer) {
                    const auto *typed_buffer
                        = static_cast<const Vulkan::Buffer *>(resource.buffer);

                    buffer_info = &buffer_infos.emplace_back(
                        typed_buffer->GetBuffer(),
                        resource.offset,
                        resource.size
                    );

                    type = vk::DescriptorType::eStorageBuffer;
                }
                else if (resource.image) {
                    auto *typed_image
                        = static_cast<Vulkan::Image *>(resource.image);

                    image_info = &image_infos.emplace_back(
                        nullptr,
                        typed_image->CreateGetImageView(resource.subresource),
                        typed_image->GetIdealImageLayout()
                    );

                    type = vk::DescriptorType::eStorageImage;
                }
                else continue;
                
                writes.emplace_back(
                    GetWritableSet(),
                    resource.binding,
                    resource.array_element,
                    1,
                    type,
                    image_info,
                    buffer_info,
                    nullptr
                );
            }

            VulkanContext->GetDevice().updateDescriptorSets(writes, {});
        }
        else {
            std::vector<Context::InternalBufferResource> buffer_defer_updates{};
            std::vector<Context::InternalImageResource> image_defer_updates{};

            buffer_defer_updates.reserve(update_resource.size());
            image_defer_updates.reserve(update_resource.size());

            for (const auto &resource : update_resource) {
                if (resource.buffer) {
                    const auto *typed_buffer
                        = static_cast<const Vulkan::Buffer *>(resource.buffer);
    
                    buffer_defer_updates.emplace_back(
                        typed_buffer->GetBuffer(),
                        vk::DescriptorType::eStorageBuffer,
                        resource.offset,
                        resource.size,
                        GetWritableSet(),
                        resource.binding,       
                        resource.array_element
                    );
                }
                else if (resource.image) {
                    auto *typed_image
                        = static_cast<Vulkan::Image *>(resource.image);
    
                    image_defer_updates.emplace_back(
                        typed_image->CreateGetImageView(resource.subresource),
                        typed_image->GetIdealImageLayout(),
                        vk::DescriptorType::eStorageImage,
                        GetWritableSet(),
                        resource.binding,       
                        resource.array_element
                    );
                }
            }

            if(!buffer_defer_updates.empty())
                VulkanContext->DeferResourceUpdate(buffer_defer_updates);
            if(!image_defer_updates.empty())
                VulkanContext->DeferResourceUpdate(image_defer_updates);
        }
    }

    void ResourceManager::ISetUniformResource(std::span<const UniformResourceDesc> update_resource) {
        const auto supported_features = VulkanContext->GetSupportedFeatures();
        const auto context_state = VulkanContext->GetContextState();

        if ((context_state == ContextState::eNone || context_state == ContextState::eCommandBufferRecording)
        || (supported_features.uniform_buffer_update_after_bind)) {
            std::vector<vk::WriteDescriptorSet> writes{};
            std::vector<vk::DescriptorBufferInfo> buffer_infos{};

            writes.reserve(update_resource.size());
            buffer_infos.reserve(update_resource.size());

            for (const auto &resource : update_resource) {
                if (!resource.buffer) continue;

                const auto *typed_buffer
                    = static_cast<const Vulkan::Buffer *>(resource.buffer);

                const auto *info = &buffer_infos.emplace_back(
                    typed_buffer->GetBuffer(),
                    resource.offset,
                    resource.size
                );

                writes.emplace_back(
                    GetUniformSet(),
                    resource.binding,
                    resource.array_element,
                    1,
                    vk::DescriptorType::eUniformBuffer,
                    nullptr,
                    info,
                    nullptr
                );
            }

            VulkanContext->GetDevice().updateDescriptorSets(writes, {});
        }
        else {
            std::vector<Context::InternalBufferResource> defer_updates{};
            defer_updates.reserve(update_resource.size());

            for (const auto &resource : update_resource) {
                if (resource.buffer) continue;

                const auto *typed_buffer
                    = static_cast<const Vulkan::Buffer *>(resource.buffer);

                defer_updates.emplace_back(
                    typed_buffer->GetBuffer(),
                    vk::DescriptorType::eUniformBuffer,
                    resource.offset,
                    resource.size,
                    GetUniformSet(),
                    resource.binding,       
                    resource.array_element
                );
            }

            if(!defer_updates.empty())
                VulkanContext->DeferResourceUpdate(defer_updates);
        }
    }

    void ResourceManager::ISetSamplerResource(std::span<const SamplerResourceDesc> update_resource) {
        const auto supported_features = VulkanContext->GetSupportedFeatures();
        const auto context_state = VulkanContext->GetContextState();

        if ((context_state == ContextState::eNone || context_state == ContextState::eCommandBufferRecording)
        || (supported_features.sampled_image_update_after_bind)) {
            std::vector<vk::WriteDescriptorSet> writes{};
            std::vector<vk::DescriptorImageInfo> image_infos{};

            writes.reserve(update_resource.size());
            image_infos.reserve(update_resource.size());

            for (const auto &resource : update_resource) {
                if (!resource.sampler) continue;

                const auto *typed_sampler
                    = static_cast<const Vulkan::Sampler *>(resource.sampler);

                const auto *info = &image_infos.emplace_back(
                    typed_sampler->GetSampler(),
                    nullptr,
                    vk::ImageLayout{}
                );

                writes.emplace_back(
                    GetSamplerSet(),
                    resource.binding,
                    resource.array_element,
                    1,
                    vk::DescriptorType::eSampler,
                    info,
                    nullptr,
                    nullptr
                );
            }

            VulkanContext->GetDevice().updateDescriptorSets(writes, {});
        }
        else {
            std::vector<Context::InternalSamplerResource> defer_updates{};
            defer_updates.reserve(update_resource.size());

            for (const auto &resource : update_resource) {
                if (resource.sampler) continue;

                const auto *typed_sampler
                    = static_cast<const Vulkan::Sampler *>(resource.sampler);

                defer_updates.emplace_back(
                    typed_sampler->GetSampler(),
                    GetSamplerSet(),
                    resource.binding,       
                    resource.array_element
                );
            }

            if(!defer_updates.empty())
                VulkanContext->DeferResourceUpdate(defer_updates);
        }   
    }
} // namespace DnmGL::Vulkan
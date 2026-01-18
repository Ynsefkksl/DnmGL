#include "DnmGL/D3D12/ResourceManager.hpp"
#include "DnmGL/D3D12/Buffer.hpp"
#include "DnmGL/D3D12/Image.hpp"
#include "DnmGL/D3D12/Sampler.hpp"
#include "DnmGL/D3D12/ToDxgiFormat.hpp"

namespace DnmGL::D3D12 {
    //TODO: fix this
    static D3D12_SHADER_RESOURCE_VIEW_DESC GetImageSRV(const D3D12::Image *image, const ImageSubresource &subresource) {
        D3D12_SHADER_RESOURCE_VIEW_DESC out;
        out.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        out.Format = ToDxgiFormat(image->GetDesc().format);
        if (subresource.type == ImageSubresourceType::e1D) {
            out.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
            out.Texture1D = D3D12_TEX1D_SRV{
                .MostDetailedMip = subresource.base_mipmap,
                .MipLevels = subresource.mipmap_level,
                .ResourceMinLODClamp = 0,
            };
        }
        else if (subresource.type == ImageSubresourceType::e2D) {
            out.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            out.Texture2D = D3D12_TEX2D_SRV{
                .MostDetailedMip = subresource.base_mipmap,
                .MipLevels = subresource.mipmap_level,
                .PlaneSlice = 0,
                .ResourceMinLODClamp = 0,
            };
        }
        else if (subresource.type == ImageSubresourceType::e2DArray) {
            out.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            out.Texture2DArray = D3D12_TEX2D_ARRAY_SRV{
                .MostDetailedMip = subresource.base_mipmap,
                .MipLevels = subresource.mipmap_level,
                .FirstArraySlice = subresource.base_layer,
                .ArraySize = subresource.layer_count,
                .PlaneSlice = 0,
                .ResourceMinLODClamp = 0,
            };
        }
        else if (subresource.type == ImageSubresourceType::e3D) {
            out.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
            out.Texture3D = D3D12_TEX3D_SRV{
                .MostDetailedMip = subresource.base_mipmap,
                .MipLevels = subresource.mipmap_level,
                .ResourceMinLODClamp = 0,
            };
        }
        else if (subresource.type == ImageSubresourceType::eCube) {
            out.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            out.TextureCube = D3D12_TEXCUBE_SRV{
                .MostDetailedMip = subresource.base_mipmap,
                .MipLevels = subresource.mipmap_level,
                .ResourceMinLODClamp = 0,
            };
        }
        return out;
    }

    ResourceManager::ResourceManager(DnmGL::D3D12::Context& ctx, std::span<const DnmGL::Shader *> shaders)
        : DnmGL::ResourceManager(ctx, shaders) {
        for (const auto* shader : shaders) {
            for (const auto& entry_point :  shader->GetEntryPoints()) {
                for (const auto& shader_binding : entry_point.readonly_resources) {
                    const auto it = std::ranges::find_if(
                            m_readonly_resource_bindings,
                            [&shader_binding] (auto& binding) {
                            return binding.binding == shader_binding.binding;
                        });

                    m_readonly_resource_bindings.emplace_back(shader_binding);
                    m_readonly_resource_count += shader_binding.resource_count;
                }
                for (const auto& shader_binding : entry_point.writable_resources) {
                    const auto it = std::ranges::find_if(
                            m_writable_resource_bindings,
                            [&shader_binding] (auto& binding) {
                            return binding.binding == shader_binding.binding;
                        });

                    m_writable_resource_bindings.emplace_back(shader_binding);
                    m_writable_resource_count += shader_binding.resource_count;
                }
                for (const auto& shader_binding : entry_point.uniform_buffer_resources) {
                    const auto it = std::ranges::find_if(
                            m_uniform_resource_bindings,
                            [&shader_binding] (auto& binding) {
                            return binding.binding == shader_binding.binding;
                        });

                    m_uniform_resource_bindings.emplace_back(shader_binding);
                    m_uniform_resource_count += shader_binding.resource_count;
                }
                for (const auto& shader_binding : entry_point.sampler_resources) {
                    const auto it = std::ranges::find_if(
                            m_uniform_resource_bindings,
                            [&shader_binding] (auto& binding) {
                            return binding.binding == shader_binding.binding;
                        });

                    m_sampler_resource_bindings.emplace_back(shader_binding);
                    m_sampler_resource_count += shader_binding.resource_count;
                }
            }
        }

        D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
        rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        rtv_heap_desc.NumDescriptors = m_readonly_resource_count
                                        + m_writable_resource_count
                                        + m_uniform_resource_count
                                        ;
        rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        D3D12Context->GetDevice()->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&m_descriptor_heap));

        rtv_heap_desc.NumDescriptors = m_sampler_resource_bindings.size();
        rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        D3D12Context->GetDevice()->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&m_sampler_heap));
    }

    void ResourceManager::ISetReadonlyResource(std::span<const ResourceDesc> update_resource) {
        for (const auto &res : update_resource) {
            auto *typed_res = res.buffer ? 
                static_cast<const D3D12::Buffer *>(res.buffer)->GetResource()
                : static_cast<const D3D12::Image *>(res.image)->GetResource();

            const auto heap_cpu_handle = GetReadonlyResourceHeapCpuHandle(res.binding, res.array_element);

            D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
            if (res.buffer) {
                desc = D3D12_SHADER_RESOURCE_VIEW_DESC{
                    .Format = DXGI_FORMAT_UNKNOWN,
                    .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
                    .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                    .Buffer = D3D12_BUFFER_SRV{
                        .FirstElement = 0,
                        .NumElements = res.buffer->GetDesc().element_count,
                        .StructureByteStride = res.buffer->GetDesc().element_size,
                        .Flags = D3D12_BUFFER_SRV_FLAG_NONE,
                    }
                };
            }
            else {
                desc = GetImageSRV(static_cast<const D3D12::Image *>(res.image), res.subresource);
            }

            D3D12Context->GetDevice()->CreateShaderResourceView(
                typed_res, &desc, heap_cpu_handle);   
        }
    }

    void ResourceManager::ISetWritableResource(std::span<const ResourceDesc> update_resource) {
        for (const auto &res : update_resource) {
            auto *typed_res = res.buffer ? 
                static_cast<const D3D12::Buffer *>(res.buffer)->GetResource()
                : static_cast<const D3D12::Image *>(res.image)->GetResource();

            const auto heap_cpu_handle = GetReadonlyResourceHeapCpuHandle(res.binding, res.array_element);

            D3D12_SHADER_RESOURCE_VIEW_DESC desc;
            if (res.buffer) {
                desc = D3D12_SHADER_RESOURCE_VIEW_DESC{
                    .Format = DXGI_FORMAT_UNKNOWN,
                    .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
                    .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                    .Buffer = D3D12_BUFFER_SRV{
                        .FirstElement = 0,
                        .NumElements = res.buffer->GetDesc().element_count,
                        .StructureByteStride = res.buffer->GetDesc().element_size,
                        .Flags = D3D12_BUFFER_SRV_FLAG_NONE,
                    }
                };
            }
            else {
                desc = GetImageSRV(static_cast<const D3D12::Image *>(res.image), res.subresource);
            }

            D3D12Context->GetDevice()->CreateShaderResourceView(
                typed_res, &desc, heap_cpu_handle);
        }
    }

    void ResourceManager::ISetUniformResource(std::span<const UniformResourceDesc> update_resource) {
        for (const auto &res : update_resource) {
            const auto *typed_buffer = static_cast<const D3D12::Buffer *>(res.buffer);
            
            const auto heap_cpu_handle = GetUniformResourceHeapCpuHandle(res.binding, res.array_element);

            const D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {
                .BufferLocation = typed_buffer->GetResource()->GetGPUVirtualAddress() + res.offset,
                .SizeInBytes = static_cast<uint32_t>(std::ceil(res.size / 256.f) * 256)
            };

            D3D12Context->GetDevice()->CreateConstantBufferView(
                &desc, heap_cpu_handle);   
        }
    }

    void ResourceManager::ISetSamplerResource(std::span<const SamplerResourceDesc> update_resource) {
        for (const auto &res : update_resource) {
            const auto *typed_res = static_cast<const D3D12::Sampler *>(res.sampler);

            const auto heap_cpu_handle = GetSamplerResourceHeapCpuHandle(res.binding, res.array_element);

            D3D12Context->GetDevice()->CreateSampler(&typed_res->m_sampler_desc, heap_cpu_handle);
        }
    }

    std::vector<D3D12_DESCRIPTOR_RANGE> ResourceManager::GetDescriptorRangesSRV_UAV_CBV(std::span<const EntryPointInfo *> entry_points) const noexcept {
        bool has_readonly_res{};
        bool has_writable_res{};
        bool has_uniform_res{};

        for (const auto *entry_point : entry_points) {
            has_readonly_res |= entry_point->HasReadonlyResource();
            has_writable_res |= entry_point->HasWritableResource();
            has_uniform_res |= entry_point->HasUniformResource();
        }

        std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
        ranges.reserve(3);

        if (has_readonly_res)
            ranges.emplace_back(
                D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                m_readonly_resource_count,
                0,
                0,
                0
            );
        if (has_writable_res)
            ranges.emplace_back(
                D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
                m_writable_resource_count,
                0,
                0,
                m_readonly_resource_count
            );
        if (has_uniform_res)
            ranges.emplace_back(
                D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
                m_uniform_resource_count,
                0,
                0,
                m_readonly_resource_count + m_writable_resource_count
            );
        return ranges;
    }

    std::optional<D3D12_DESCRIPTOR_RANGE> ResourceManager::GetDescriptorRangeSampler(std::span<const EntryPointInfo *> entry_points) const noexcept {
        bool has_sampler_res{};

        for (const auto *entry_point : entry_points) {
            has_sampler_res |= entry_point->HasSamplerResource();
        }

        if (has_sampler_res) {
            return D3D12_DESCRIPTOR_RANGE{
                .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
                .NumDescriptors = m_sampler_resource_count,
                .BaseShaderRegister = 0,
                .RegisterSpace = 0,
                .OffsetInDescriptorsFromTableStart = 0,
            };
        }
        return std::nullopt;
    }
}
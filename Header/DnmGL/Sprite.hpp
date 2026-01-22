#pragma once

#include "DnmGL.hpp"

namespace DnmGL {
    struct alignas(16) SpriteCameraData {
        Mat4x4 proj_mtx;
        Float3 pos;
    };

    struct alignas(16) SpriteData {
        ColorFloat color = {1,1,1,1};
        Float2 uv_up_right{};
        Float2 uv_bottom_left{};
        Float2 position = {0, 0};
        Float2 scale = {0.1, 0.1};
        float angle = {0};
        float color_factor = {0};
    
        FORCE_INLINE void SetColor(const ColorFloat& c) { color = c; }
        FORCE_INLINE const ColorFloat& GetColor() const { return color; }
    
        FORCE_INLINE void SetSpriteUv(const Float4& v) { uv_up_right.x = v.x; uv_up_right.y = v.y; uv_bottom_left.x = v.z; uv_bottom_left.y = v.w; }
    
        FORCE_INLINE void SetSpriteUpRight(const Float2& v) { uv_up_right = v; }
        FORCE_INLINE Float2 GetSpriteUpRight() const { return uv_up_right; }
    
        FORCE_INLINE void SetSpriteBottomLeft(const Float2& v) { uv_bottom_left = v; }
        FORCE_INLINE Float2 GetSpriteBottomLeft() const { return uv_bottom_left; }
    
        FORCE_INLINE void SetPosition(const Float2& p) { position = p; }
        FORCE_INLINE Float2 GetPosition() const { return position; }
        FORCE_INLINE void AddPosition(const Float2& d) { position += d; }
    
        FORCE_INLINE void SetScale(const Float2& s) { scale = s; }
        FORCE_INLINE Float2 GetScale() const { return scale; }
        FORCE_INLINE void AddScale(const Float2& d) { scale += d; }
    
        FORCE_INLINE void SetAngle(const float a) { angle = a; }
        FORCE_INLINE float GetAngle() const { return angle; }
        FORCE_INLINE void AddAngle(const float d) { angle += d; }
    
        FORCE_INLINE void SetColorFactor(const float f) { color_factor = f; }
        FORCE_INLINE float GetColorFactor() const { return color_factor; }
        FORCE_INLINE void AddColorFactor(const float d) { color_factor += d; }
    };

    class SpriteCamera {
    public:
        // ortho
        SpriteCamera(Float2 camera_extent, float near_, float far_) 
        : m_camera_extent(camera_extent), m_near(near_), m_far(far_), m_perspective(false) {}
        // perspective
        SpriteCamera(Float2 camera_extent, float fov, float near_, float far_) 
        : m_camera_extent(camera_extent), m_fov(fov), m_near(near_), m_far(far_), m_perspective(true) {}

        void SetProjection(float fov, float near_, float far_) {
            m_fov = fov;
            m_near = near_;
            m_far = far_;
            m_perspective = true;
        }

        void SetOrtho(float near_, float far_) {
            m_near = near_;
            m_far = far_;
            m_perspective = false;
        }
        
        Float2 m_camera_extent;
        float m_fov;
        float m_near;
        float m_far;

        SpriteCameraData& CalculateProjMtx() {
            if (m_perspective) {
                m_camera_data.proj_mtx = Perspective(m_fov, m_camera_extent.x / m_camera_extent.y, m_near, m_far);
            }
            else {
                m_camera_data.proj_mtx = Ortho(-m_camera_extent.x, m_camera_extent.x, -m_camera_extent.y, m_camera_extent.y, 0.01f, 1.f);
            }
            return m_camera_data;
        }

        SpriteCameraData& GetCameraData() {
            return m_camera_data;
        }

        void SetPos(Float3 pos) {
            m_camera_data.pos = pos;
        }

        Float3 GetPos() const {
            return m_camera_data.pos;
        }
    private:
        SpriteCameraData m_camera_data{};
        bool m_perspective{};
    };

    //pointers life cycle is managed by SpriteManager
    class SpriteHandle {
    public:
        SpriteHandle() = default;
        bool operator==(const SpriteHandle& other) const {
            return GetValue() == other.GetValue();
        }
    private:
        explicit SpriteHandle(uint32_t value) : value(new uint32_t(value)) {}

        void SetValue(const uint32_t newValue) const {
            *value = newValue;
        }

        [[nodiscard]] uint32_t GetValue() const {
            return *value;
        }

        void Invalidate() {
            delete value;
            value = nullptr;
        }

        bool isNull() const {
            return value == nullptr;
        }

        uint32_t *value{}; // point to spriteBuffer element
        friend class SpriteManager;
    };

    struct SpriteManagerDesc {
        Context *context;
        Image *atlas_texture;
        Sampler *sampler;
        ImageSubresource *atlas_texture_subresource;
        Uint2 extent;
        SampleCount msaa;
        uint32_t init_capacity = 1024*64;
        bool color_blend = true;
    };
    
    class SpriteManager {
    public:
        SpriteManager(const DnmGL::SpriteManagerDesc& desc);
        ~SpriteManager() noexcept {
            for (auto& handle : m_handles) {
                if (handle.isNull())
                    continue;
                handle.Invalidate();
            }
        }

        [[nodiscard]] auto GetSpriteCount() const { return m_sprite_count; }
        [[nodiscard]] auto GetCapacity() const { return m_sprite_buffer->GetDesc().element_count; }

        [[nodiscard]] auto* GetSpriteBuffer() const { return m_sprite_buffer.get(); }
        [[nodiscard]] auto* GetGraphicsPipeline() const  { return m_graphics_pipeline.get(); }
        [[nodiscard]] auto* GetResourceManager() const  { return m_resource_manager.get(); }
        [[nodiscard]] auto* GetVertexShader() const { return m_shader.get(); }
        [[nodiscard]] auto* GetFragmentShader() const { return m_fragment_shader.get(); }
        [[nodiscard]] auto* GetSpriteBufferMappedPtr() const noexcept { return m_sprite_buffer->GetMappedPtr<SpriteData>(); }
        [[nodiscard]] auto* GetContext() const { return m_sprite_buffer->context; }
        [[nodiscard]] auto* GetCamera() const { return m_camera_ptr; }
        void SetCamera(SpriteCamera *camera) { m_camera_ptr = camera; }

        void BeginSpriteRendering(DnmGL::CommandBuffer* command_buffer, const DnmGL::ColorFloat &clear_color, DnmGL::Framebuffer *framebuffer) noexcept;
        void DrawSprites(DnmGL::CommandBuffer* command_buffer) noexcept;

        void ReserveSprite(DnmGL::CommandBuffer* command_buffer, uint32_t reserve_count) noexcept;

        std::optional<SpriteHandle> CreateSprite(const DnmGL::SpriteData& sprite_data) noexcept;
        std::vector<SpriteHandle> CreateSprites(std::span<const DnmGL::SpriteData> sprite_data) noexcept;
        SpriteHandle CreateSprite(DnmGL::CommandBuffer* command_buffer, const DnmGL::SpriteData& sprite_data) noexcept;
        std::vector<SpriteHandle> CreateSprites(DnmGL::CommandBuffer* command_buffer, std::span<const DnmGL::SpriteData> sprite_data) noexcept;

        void DeleteSprite(DnmGL::SpriteHandle& handle) noexcept;

        void SetSprite(DnmGL::SpriteHandle handle, const DnmGL::SpriteData& sprite_data) const noexcept;
        void SetSprite(DnmGL::SpriteHandle handle, const auto& data, auto SpriteData::*member) const noexcept;

        SpriteData GetSprite(DnmGL::SpriteHandle handle) const noexcept;
    private:
        std::vector<SpriteHandle> CreateSpriteBase(std::span<const DnmGL::SpriteData> sprite_data);
        SpriteHandle CreateSpriteBase(const DnmGL::SpriteData& sprite_data);

        std::vector<DnmGL::SpriteHandle> m_handles{};

        DnmGL::GraphicsPipeline::Ptr m_graphics_pipeline{};
        DnmGL::ResourceManager::Ptr m_resource_manager{};
        DnmGL::Shader::Ptr m_shader{};
        DnmGL::Shader::Ptr m_fragment_shader{};

        DnmGL::Buffer::Ptr m_sprite_buffer{};
        DnmGL::Buffer::Ptr m_camera_buffer{};
        SpriteCamera* m_camera_ptr{};
        uint32_t m_sprite_count{};
    };

    inline SpriteManager::SpriteManager(const DnmGL::SpriteManagerDesc& desc) {
        {
            const auto init_capacity = std::max(desc.init_capacity, 1u);
    
            m_sprite_buffer = desc.context->CreateBuffer({
                .element_size = sizeof(SpriteData),
                .element_count = init_capacity,
                .memory_host_access = DnmGL::MemoryHostAccess::eWrite,
                .memory_type = DnmGL::MemoryType::eDeviceMemory,
                .usage_flags = DnmGL::BufferUsageBits::eReadonlyResource,
            });
            
            m_handles.reserve(init_capacity);
        }

        {
            m_camera_buffer = desc.context->CreateBuffer({
                .element_size = sizeof(SpriteCameraData),
                .element_count = 1,
                .memory_host_access = DnmGL::MemoryHostAccess::eWrite,
                .memory_type = DnmGL::MemoryType::eAuto,
                .usage_flags = DnmGL::BufferUsageBits::eUniform,
            });
        }

        {
            m_shader = desc.context->CreateShader("Sprite");

            const DnmGL::Shader* shaders[1] = {m_shader.get()};
            m_resource_manager = desc.context->CreateResourceManager(
                shaders
            );

            GraphicsPipelineDesc pipeline_desc{};
            pipeline_desc.color_attachment_formats =  { ImageFormat::eRGBA8Norm };
            pipeline_desc.depth_stencil_format = GetContext()->GetSwapchainSettings().depth_buffer_format;
            pipeline_desc.vertex_entry_point = "VertMain"; 
            pipeline_desc.vertex_shader = m_shader.get(); 
            pipeline_desc.fragment_entry_point = "FragMain"; 
            pipeline_desc.fragment_shader = m_shader.get(); 
            pipeline_desc.resource_manager = m_resource_manager.get(); 
            pipeline_desc.depth_test_compare_op = DnmGL::CompareOp::eLessOrEqual; 
            pipeline_desc.cull_mode = DnmGL::CullMode::eNone; 
            pipeline_desc.topology = DnmGL::PrimitiveTopology::eTriangleStrip; 
            pipeline_desc.msaa = desc.msaa; 
            pipeline_desc.depth_test = !desc.color_blend; 
            pipeline_desc.depth_write = !desc.color_blend; 
            pipeline_desc.color_blend = desc.color_blend; 

            m_graphics_pipeline = desc.context->CreateGraphicsPipeline(pipeline_desc);
        }

        {
            const ResourceDesc resource_desc[] = {
                {
                    .image = desc.atlas_texture ? desc.atlas_texture : GetContext()->GetPlaceholderImage(),
                    .subresource = desc.atlas_texture_subresource ? *desc.atlas_texture_subresource : ImageSubresource{},
                    .binding = 1,
                    .array_element = 0,
                }
            };
            m_resource_manager->SetReadonlyResource(resource_desc);
        }
        {
            const SamplerResourceDesc resource_desc[] = {
                {
                    .sampler = desc.sampler ? desc.sampler : GetContext()->GetPlaceholderSampler(),
                    .binding = 0,
                    .array_element = 0,
                }
            };
            m_resource_manager->SetSamplerResource(resource_desc);
        }
        {
            const ResourceDesc resource_desc[] = {
                {
                    .buffer = m_sprite_buffer.get(),
                    .offset = 0,
                    .size = static_cast<uint32_t>(m_sprite_buffer->GetDesc().element_count * m_sprite_buffer->GetDesc().element_size),
                    .binding = 0,
                    .array_element = 0,
                },
            };
            m_resource_manager->SetReadonlyResource(resource_desc);
        }
        {
            const UniformResourceDesc resource_desc[] = {
                {
                    .buffer = m_camera_buffer.get(),
                    .offset = 0,
                    .size = static_cast<uint32_t>(m_camera_buffer->GetDesc().element_size),
                    .binding = 0,
                    .array_element = 0,
                },
            };
            m_resource_manager->SetUniformResource(resource_desc);
        }
    }

    inline void SpriteManager::ReserveSprite(DnmGL::CommandBuffer* command_buffer, uint32_t reserve_count) noexcept {
        if (GetCapacity() - GetSpriteCount() >= reserve_count
        || command_buffer == nullptr)
            return;

        m_handles.reserve(reserve_count);

        {
            auto new_buffer = GetContext()->CreateBuffer({
                .element_size = sizeof(SpriteData),
                .element_count = (GetCapacity() + (reserve_count < GetCapacity() ? GetCapacity() : reserve_count)),
                .memory_host_access = DnmGL::MemoryHostAccess::eWrite,
                .memory_type = DnmGL::MemoryType::eHostMemory,
                .usage_flags = DnmGL::BufferUsageBits::eReadonlyResource,
            });

            command_buffer->CopyBufferToBuffer({
                .src_buffer = m_sprite_buffer.get(),
                .dst_buffer = new_buffer.get(),
                .copy_size = GetSpriteCount() * sizeof(SpriteData),
            });

            m_sprite_buffer.swap(new_buffer);

            const ResourceDesc sprite_buffer_resources[] = {
                {
                    .buffer = m_sprite_buffer.get(),
                    .offset = 0,
                    .size = static_cast<uint32_t>(m_sprite_buffer->GetDesc().element_count * m_sprite_buffer->GetDesc().element_size),
                    .binding = 0,
                    .array_element = 0,
                },
            };
            m_resource_manager->SetReadonlyResource(sprite_buffer_resources);
        }
    }

    inline std::optional<SpriteHandle> SpriteManager::CreateSprite(const DnmGL::SpriteData& sprite_data) noexcept {
        if (GetCapacity() - GetSpriteCount() < 1)
            return {};

        return CreateSpriteBase(sprite_data);
    }

    inline std::vector<SpriteHandle> SpriteManager::CreateSprites(std::span<const DnmGL::SpriteData> sprite_data) noexcept {
        const auto element_count = (GetCapacity() - GetSpriteCount()) - sprite_data.size();
        if (element_count == 0)
            return {};

        return CreateSpriteBase(std::span(sprite_data.data(), element_count));
    }

    inline SpriteHandle SpriteManager::CreateSprite(DnmGL::CommandBuffer* command_buffer, const DnmGL::SpriteData& sprite_data) noexcept {
        ReserveSprite(command_buffer, 1);

        return CreateSpriteBase(sprite_data);
    }

    inline std::vector<SpriteHandle> SpriteManager::CreateSprites(DnmGL::CommandBuffer* command_buffer, std::span<const DnmGL::SpriteData> sprite_data) noexcept {
        if (sprite_data.size() == 0)
            return {};

        ReserveSprite(command_buffer, sprite_data.size());

        return CreateSpriteBase(sprite_data);
    }

    inline SpriteHandle SpriteManager::CreateSpriteBase(const DnmGL::SpriteData& sprite_data) {
        std::memcpy(
            &reinterpret_cast<SpriteData*>(GetSpriteBufferMappedPtr())[GetSpriteCount()],
            &sprite_data, 
            sizeof(SpriteData)
        );
        
        return m_handles.emplace_back(SpriteHandle(m_sprite_count++));
    }

    inline std::vector<SpriteHandle> SpriteManager::CreateSpriteBase(std::span<const DnmGL::SpriteData> sprite_data) {
        std::memcpy(
            &GetSpriteBufferMappedPtr()[GetSpriteCount()],
            sprite_data.data(), 
            sizeof(SpriteData) * sprite_data.size()
        );

        m_handles.reserve(sprite_data.size());

        std::vector<SpriteHandle> out_handles(sprite_data.size());
        for (auto& handle : out_handles) {
            handle = m_handles.emplace_back(SpriteHandle(GetSpriteCount()));
            ++m_sprite_count;
        }
        return out_handles;
    }

    inline void SpriteManager::DeleteSprite(SpriteHandle& handle) noexcept {
        if (handle.isNull()) {
            return;
        }

        if (handle == m_handles.back()) {
            --m_sprite_count;
            handle.Invalidate();
            m_handles.pop_back();
            return;
        }

        {
            const auto *end_ptr = GetSpriteBufferMappedPtr() + --m_sprite_count;
            auto *deleted_sprite = GetSpriteBufferMappedPtr() + handle.GetValue();

            memcpy(deleted_sprite, end_ptr, sizeof(SpriteData));
        }

        const auto handle_it = m_handles.begin() + handle.GetValue();
        *handle_it = std::move(m_handles.back());
        handle_it->SetValue(handle.GetValue());
        handle.Invalidate();
        m_handles.pop_back();
    }

    inline void SpriteManager::SetSprite(DnmGL::SpriteHandle handle, const auto& data, auto SpriteData::*member) const noexcept {
        if (handle.isNull()) {
            return;
        }

        auto &sprite_data = *(GetSpriteBufferMappedPtr() + handle.GetValue());

        std::memcpy(
            &(sprite_data.*member),
            &data,
            sizeof(data)
        );
    }

    inline void SpriteManager::SetSprite(DnmGL::SpriteHandle handle, const DnmGL::SpriteData& sprite_data) const noexcept {
        if (handle.isNull()) {
            return;
        }

        std::memcpy(
            GetSpriteBufferMappedPtr() + handle.GetValue(),
            &sprite_data,
            sizeof(SpriteData)
        );
    }

    inline SpriteData SpriteManager::GetSprite(DnmGL::SpriteHandle handle) const noexcept {
        return *(GetSpriteBufferMappedPtr() + handle.GetValue());   
    }

    inline void SpriteManager::BeginSpriteRendering(DnmGL::CommandBuffer *command_buffer, const DnmGL::ColorFloat &clear_color, DnmGL::Framebuffer *framebuffer) noexcept {
        BeginRenderingDesc begin_desc;
        begin_desc.pipeline = m_graphics_pipeline.get();
        begin_desc.framebuffer = framebuffer;
        begin_desc.color_clear_values = std::span(&clear_color, 1);
        begin_desc.depth_stencil_clear_value = DnmGL::DepthStencilClearValue{.depth = 0, .stencil = 0};
        begin_desc.attachment_ops.color_load[0] = {DnmGL::AttachmentLoadOp::eClear};
        begin_desc.attachment_ops.color_store[0] = {DnmGL::AttachmentStoreOp::eStore};
        begin_desc.attachment_ops.depth_load = DnmGL::AttachmentLoadOp::eClear;
        begin_desc.attachment_ops.depth_store = DnmGL::AttachmentStoreOp::eStore;

        command_buffer->BeginRendering(begin_desc);
    }

    inline void SpriteManager::DrawSprites(DnmGL::CommandBuffer *command_buffer) noexcept {
        if (!m_sprite_count) return;
        DnmGLAssert(m_camera_ptr , "camera cannot be null");

        auto *mapped_ptr = m_camera_buffer->GetMappedPtr();
        std::memcpy(mapped_ptr, &m_camera_ptr->GetCameraData(), sizeof(SpriteCameraData));

        command_buffer->Draw(4, m_sprite_count);
    }
}

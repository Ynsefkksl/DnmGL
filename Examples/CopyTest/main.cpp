#include "DnmGL/DnmGL.hpp"
#include "DnmGL/ContextLoader.hpp"
#include "DnmGL/Sprite.hpp"
#include "DnmGL/Utility/Container.hpp"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <exception>
#include <print>
#include <array>

constexpr DnmGL::Uint2 WindowExtent = {1280, 720};

int main(int argc, char** args) {
    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(WindowExtent.x, WindowExtent.y, "Copy Test", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    try {
        DnmGL::ContextLoader ctx_loader("../Vulkan/DnmGL_Vulkan");
        auto* context = ctx_loader.GetContext();

        DnmGLAssert(context, "context is null!!!")

        context->SetCallbackFunc([] (std::string_view message, DnmGL::MessageType error, std::string_view source) {
            std::println("message: {}", message);
        });

        context->Init({
                .window_extent = WindowExtent,
                .window_handle = DnmGL::WinWindowHandle {
                    .hwnd = glfwGetWin32Window(window),
                    .hInstance = GetModuleHandle(nullptr),
                },
                .shader_directory = "./Shaders/",
                .Vsync = false,
            });
            
        DnmGL::Image::Ptr atlas_texture;

        context->ExecuteCommands([&] (DnmGL::CommandBuffer* command_buffer) -> bool {
            auto image = context->CreateImage({
                .extent = {1024, 1024, 1},
                .format = DnmGL::ImageFormat::eRGBA8Norm,
                .usage_flags = DnmGL::ImageUsageBits::eSampled,
                .type = DnmGL::ImageType::e2D, 
                .mipmap_levels = 1,
                .sample_count = DnmGL::SampleCount::e1,
            });

            std::vector<uint32_t> data(1024 * 1024);
            std::ranges::fill(data, -1);

            command_buffer->UploadData<uint32_t>(
                image.get(), 
                DnmGL::ImageSubresource{}, 
                std::span(data), 
                {1024, 1024, 1},
                {0,0,0});

            auto buffer = context->CreateBuffer({
                .size = 1024 * 4,
                .memory_host_access = DnmGL::MemoryHostAccess::eWrite,
                .memory_type = DnmGL::MemoryType::eAuto
            });

            data.resize(1024);
            std::ranges::fill(data, 0);
            command_buffer->UploadData<uint32_t>(buffer.get(), data, 0);
            
            command_buffer->CopyBufferToImage({
                .src_buffer = buffer.get(), 
                .dst_image = image.get(),
                .image_subresource = {},
                .buffer_offset = 0,
                .copy_offset = {32, 0, 0},
                .copy_extent = {32, 32, 1}
            });

            atlas_texture = context->CreateImage({
                .extent = {1024, 1024, 1},
                .format = DnmGL::ImageFormat::eRGBA8Norm,
                .usage_flags = DnmGL::ImageUsageBits::eSampled,
                .type = DnmGL::ImageType::e2D, 
                .mipmap_levels = 1,
                .sample_count = DnmGL::SampleCount::e1,
            });

            command_buffer->CopyImageToImage({
                .src_image = image.get(),
                .dst_image = atlas_texture.get(),
                .src_image_subresource = {},
                .dst_image_subresource = {},
                .src_offset = {0, 0, 0},
                .dst_offset = {0, 0, 0},
                .copy_extent = {1024, 1024, 1},
            });

            return true;
        });

        auto texture_resource = DnmGL::ImageResource {
            .image = atlas_texture.get(),
            .subresource = {},
        };
        
        DnmGL::SpriteManager sprite_manager({
            .context = context,
            .atlas_texture = atlas_texture.get(),
            .sampler = nullptr,
            .atlas_texture_subresource = {},
            .extent = WindowExtent,
            .msaa = DnmGL::SampleCount::e1,
            .init_capacity = 1024 * 512,
        });
        return 0;

        DnmGL::SpriteCamera camera({float(WindowExtent.x)/WindowExtent.y, 1}, 0.1, 10);
        camera.CalculateProjMtx();
        sprite_manager.SetCamera(&camera);

        sprite_manager.CreateSprite(DnmGL::SpriteData{
            .uv_up_right = {0, 0},
            .uv_bottom_left = {1, 1},
            .scale = {1, 1},
        });

        {
            while (!glfwWindowShouldClose(window)) {
                if (glfwGetKey(window, GLFW_KEY_ESCAPE))
                    break;

                context->Render([&] (DnmGL::CommandBuffer* command_buffer) -> bool {
                    command_buffer->SetScissor({WindowExtent.x, WindowExtent.y}, {0, 0});
                    command_buffer->SetViewport({WindowExtent.x, WindowExtent.y}, {0, 0}, 0.f, 1.f);
    
                    sprite_manager.RenderSprites(command_buffer, {0, 0, 0, 0}, nullptr);
                    return true;
                });

                glfwPollEvents();
            }
        }
    }
    catch (const std::exception& e) {
        std::println("graphics error: {}", e.what());
    }

    glfwTerminate();
}

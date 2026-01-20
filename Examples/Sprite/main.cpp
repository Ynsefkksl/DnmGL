#include "DnmGL/DnmGL.hpp"
#include "DnmGL/Sprite.hpp"
#include "DnmGL/Utility/Container.hpp"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <exception>
#include <print>
#include <array>

constexpr DnmGL::Uint2 WindowExtent = {1280, 720};
constexpr DnmGL::SampleCount MsaaValue = DnmGL::SampleCount::e1;

constexpr DnmGL::Float2 SpaceshipScale = {0.1, 0.1};
constexpr DnmGL::Float2 EnemySpaceshipScale = {0.1, 0.1};
constexpr DnmGL::Float2 BulletScale = {0.03, 0.03};
constexpr DnmGL::Float2 EnemyBulletScale = {0.03, 0.03};

constexpr DnmGL::Float4 SpaceshipUvCoords = {0, 0.5, 0.49, 1};
constexpr DnmGL::Float4 EnemySpaceshipUvCoords = {0, 0.5, 0.49, 0.0};
constexpr DnmGL::Float4 BulletUvCoords = {0.75, 1.0, 1.0, 0.75};
constexpr DnmGL::Float4 EnemyBulletUvCoords = {0.5, 1.0, 0.75, 0.75};

constexpr float SpaceshipSpeed = 0.005;
constexpr float EnemySpeed = -0.002;
constexpr float BulletSpeed = 0.002;
constexpr float EnemyBulletSpeed = -0.002;

enum class ObjectType {
    eSpaceship = 0,
    eEnemySpaceship = 1,
    eBullet = 2,
    eEnemyBullet = 3,
};

constexpr std::array speeds {
    SpaceshipSpeed,
    EnemySpeed,
    BulletSpeed,
    EnemyBulletSpeed
};

constexpr std::array sprite_template {
    DnmGL::SpriteData {
        .color = {0,0,0,0},
        .uv_up_right = {SpaceshipUvCoords.x, SpaceshipUvCoords.y},
        .uv_bottom_left = {SpaceshipUvCoords.z, SpaceshipUvCoords.w},
        .position = {0, -2},
        .scale = SpaceshipScale,
        .angle = 0,
        .color_factor = 0.f,
    },
    DnmGL::SpriteData {
        .color = {0,0,0,0},
        .uv_up_right = {EnemySpaceshipUvCoords.x, EnemySpaceshipUvCoords.y},
        .uv_bottom_left = {EnemySpaceshipUvCoords.z, EnemySpaceshipUvCoords.w},
        .position = {0, -2},
        .scale = EnemySpaceshipScale,
        .angle = 0,
        .color_factor = 0.f,
    },
    DnmGL::SpriteData {
        .color = {0,0,0,0},
        .uv_up_right = {BulletUvCoords.x, BulletUvCoords.y},
        .uv_bottom_left = {BulletUvCoords.z, BulletUvCoords.w},
        .position = {0, -2},
        .scale = BulletScale,
        .angle = 0,
        .color_factor = 0.f,
    },
    DnmGL::SpriteData {
        .color = {0,0,0,0},
        .uv_up_right = {EnemyBulletUvCoords.x, EnemyBulletUvCoords.y},
        .uv_bottom_left = {EnemyBulletUvCoords.z, EnemyBulletUvCoords.w},
        .position = {0, -2},
        .scale = EnemyBulletScale,
        .angle = 0,
        .color_factor = 0.f,
    },
};

static uint32_t createdObjectCount = 0;
static uint32_t updatedObjectCount = 0;
static uint32_t deletedObjectCount = 0;

static uint64_t totalCreatedObjectCount = 0;
static uint64_t totalUpdatedObjectCount = 0;
static uint64_t totalDeletedObjectCount = 0;

static DnmGL::SpriteManager *global_sprite_manager;

class Object {
public:
    Object(ObjectType obj_type, DnmGL::Float2 pos) 
    : m_pos(pos), m_obj_type(obj_type) {
        m_handle = global_sprite_manager->CreateSprite(sprite_template[int(obj_type)]).value();
        ++createdObjectCount;
    }

    Object(DnmGL::CommandBuffer *command_buffer, ObjectType obj_type, DnmGL::Float2 pos) 
    : m_pos(pos), m_obj_type(obj_type) {
        m_handle = global_sprite_manager->CreateSprite(command_buffer, sprite_template[int(obj_type)]);
        ++createdObjectCount;
    }

    void Delete() {
        global_sprite_manager->DeleteSprite(m_handle);
        ++deletedObjectCount;
    }

    void UpdatePos() {
        global_sprite_manager->SetSprite(m_handle, m_pos, &DnmGL::SpriteData::position);
        ++updatedObjectCount;
    }

    DnmGL::Float2 m_pos{};
    ObjectType m_obj_type;
    DnmGL::SpriteHandle m_handle;
    Container<Object>::Handle m_object_handle;
};

int main(int argc, char** args) {
    bool use_d3d12 = false;
    if (argc > 1)
        if (std::string(args[1]) == "D3D12")
            use_d3d12 = true;

    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow *window = glfwCreateWindow(WindowExtent.x, WindowExtent.y, "Sprite Demo", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    try {
        DnmGL::Context *context{};
        if (use_d3d12) {
            context = DnmGL::CreateD3D12Context();
        }
        else {
            context = DnmGL::CreateVulkanContext();
        }

        DnmGLAssert(context, "context is null!!!")

        context->SetCallbackFunc([] (std::string_view message, DnmGL::MessageType error, std::string_view source) {
            std::println("message: {}", message);
        });

        context->Init({
                .window_handle = DnmGL::WinWindowHandle {
                    .hwnd = glfwGetWin32Window(window),
                    .hInstance = GetModuleHandle(nullptr),
                },
                .shader_directory = "./Shaders/",
                .swapchain_settings {
                    .window_extent = WindowExtent,
                    .depth_buffer_format = DnmGL::ImageFormat::eD16Norm,
                    .msaa = MsaaValue,
                    .Vsync = false,
                }
            });

        DnmGL::Image::Ptr atlas_texture;

        context->ExecuteCommands([&] (DnmGL::CommandBuffer* command_buffer) -> bool {
            command_buffer->BeginCopyPass();
            int x, y;
            auto *image_data = stbi_load("./Examples/Sprite/dnm.png", &x, &y, nullptr, 4);
            atlas_texture = context->CreateImage({
                .extent = {static_cast<uint32_t>(x), static_cast<uint32_t>(y), 1},
                .format = DnmGL::ImageFormat::eRGBA8Norm,
                .usage_flags = DnmGL::ImageUsageBits::eReadonlyResource,
                .type = DnmGL::ImageType::e2D, 
                .mipmap_levels = 1
            });

            command_buffer->UploadData<uint32_t>(
                atlas_texture.get(), 
                DnmGL::ImageSubresource{}, 
                std::span((uint32_t *)image_data, x*y), 
                {static_cast<uint32_t>(x), static_cast<uint32_t>(y), 1}, 
                0);
            stbi_image_free(image_data);
            return true;
        });

        auto texture_resource = DnmGL::ResourceDesc {
            .image = atlas_texture.get(),
            .subresource = {},
        };

        DnmGL::SpriteManager sprite_manager({
            .context = context,
            .atlas_texture = atlas_texture.get(),
            .sampler = nullptr,
            .atlas_texture_subresource = {},
            .extent = WindowExtent,
            .msaa = MsaaValue,
            .init_capacity = 512 * 1024,
            .color_blend = true
        });

        DnmGL::SpriteCamera camera({float(WindowExtent.x)/WindowExtent.y, 1}, 0.1, 10);
        camera.CalculateProjMtx();
        sprite_manager.SetCamera(&camera);

        global_sprite_manager = &sprite_manager;

        {
            auto player = Object(ObjectType::eSpaceship, DnmGL::Float2{});
    
            Container<Object> bullets{};
            std::vector<Container<Object>::Handle> m_deleted_bullets{};
            Container<Object> enemys{};
            std::vector<Container<Object>::Handle> m_deleted_enemys{};
    
            uint32_t i{};
            while (!glfwWindowShouldClose(window)) {
                if (glfwGetKey(window, GLFW_KEY_ESCAPE))
                    break;

                if (i == 200) {
                    std::println("created object count: {}\nupdated object count: {}\ndeleted object count: {}", 
                        createdObjectCount, updatedObjectCount, deletedObjectCount);
                    i = 0;
                }
                i++;

                createdObjectCount = 0; 
                updatedObjectCount = 0;
                deletedObjectCount = 0;

                {
                    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
                        player.m_pos.y += SpaceshipSpeed;
                    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
                        player.m_pos.y +=-SpaceshipSpeed;
                    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                        player.m_pos.x += SpaceshipSpeed;
                    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                        player.m_pos.x +=-SpaceshipSpeed;
                }
    
                context->Render([&] (DnmGL::CommandBuffer* command_buffer) -> bool {
                    command_buffer->BeginCopyPass();
                    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
                        auto handle = bullets.AddElement(Object(command_buffer, ObjectType::eBullet, player.m_pos + DnmGL::Float2{0, 0.1}));
                        bullets.GetElement(handle).m_object_handle = handle;
                    }
                    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
                        auto handle = enemys.AddElement(Object(command_buffer,ObjectType::eEnemySpaceship, DnmGL::Float2{0, 1}));
                        enemys.GetElement(handle).m_object_handle = handle;
                    }
                    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
                        for (auto& enemy : enemys) {
                            auto handle = bullets.AddElement(Object(command_buffer,ObjectType::eEnemyBullet, enemy.m_pos - DnmGL::Float2{0, 0.1}));
                            bullets.GetElement(handle).m_object_handle = handle;
                        }
                    }
                    command_buffer->EndCopyPass();
                    
                    player.UpdatePos();

                    for (auto& object : enemys) {
                        object.m_pos.y += EnemySpeed;
                        if (object.m_pos.y < -1) {
                            m_deleted_enemys.emplace_back(object.m_object_handle);
                        }
                        object.UpdatePos();
                    }
                    for (auto& object : bullets) {
                        object.m_pos.y += speeds[int(object.m_obj_type)];
                        if (object.m_obj_type == ObjectType::eBullet 
                        && object.m_pos.y > 1) {
                            m_deleted_bullets.emplace_back(object.m_object_handle);
                        }
                        else if (object.m_obj_type == ObjectType::eEnemyBullet 
                        && object.m_pos.y < -1) {
                            m_deleted_bullets.emplace_back(object.m_object_handle);
                        }
                        object.UpdatePos();
                    }

                    sprite_manager.BeginSpriteRendering(command_buffer, {0, 0, 0, 0}, nullptr);

                    command_buffer->SetScissor({WindowExtent.x, WindowExtent.y}, {0, 0});
                    command_buffer->SetViewport({WindowExtent.x, WindowExtent.y}, {0, 0}, 0.f, 1.f);
                    
                    sprite_manager.DrawSprites(command_buffer);

                    return true;
                });

                for (auto& handle : m_deleted_bullets) {
                    bullets.GetElement(handle).Delete();
                    bullets.DeleteElement(handle);
                }
                for (auto& handle : m_deleted_enemys) {
                    enemys.GetElement(handle).Delete();
                    enemys.DeleteElement(handle);
                }

                m_deleted_enemys.clear();
                m_deleted_bullets.clear();

                glfwPollEvents();
            }
        }
    }
    catch (const std::exception& e) {
        std::println("graphics error: {}", e.what());
    }

    glfwTerminate();
}


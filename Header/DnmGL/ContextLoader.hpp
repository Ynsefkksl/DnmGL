#include "DnmGL/DnmGL.hpp"

#ifdef OS_WIN
    # define SHARED_LIB_EXT ".dll"
#elif OS_LINUX
    # define SHARED_LIB_EXT ".so"
#else
    #error currently just supported linux and windows
#endif

#if __has_include(<SDL3/SDL.h>) || __has_include(<SDL2/SDL.h>)
    #if __has_include(<SDL3/SDL.h>) 
        #include <SDL3/SDL_loadso.h>
    #elif __has_include(<SDL2/SDL.h>) 
        #include <SDL2/SDL_loadso.h>
    #endif
namespace DnmGL {
    class ContextLoader : DnmGL::ContextLoader {
    public:
        ContextLoader(std::string_view shared_path) {
            auto path = std::string(shared_path) + SHARED_LIB_EXT;
            shared = SDL_LoadObject(path.data());
            if (!shared) {
                //throw DnmGL::Error("failed to context lib load");
                return;
            }

            const auto load_func = (DnmGL::Context*((*)()))SDL_LoadFunction(shared, "LoadContext");
            if (!load_func) {
                SDL_UnloadObject(shared);
                //throw DnmGL::Error("failed to context load function");
                return;
            }
            
            context = load_func();
        }

        ~ContextLoader() {
            if (context) {
                delete context;
                SDL_UnloadObject(shared);
            }
        }

        [[nodiscard]] DnmGL::Context *GetContext() const noexcept { return context; }

        ContextLoader(ContextLoader&&) = delete;
        ContextLoader(ContextLoader&) = delete;
        ContextLoader& operator=(ContextLoader&&) = delete;
        ContextLoader& operator=(ContextLoader&) = delete;
    private:
        DnmGL::Context *context = nullptr;
        SDL_SharedObject *shared = nullptr;
    };
};
#elif defined(OS_WIN)

    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #include <libloaderapi.h>

    #undef UpdateResource
    #undef far
    #undef near

namespace DnmGL {
    class ContextLoader {
    public:
        ContextLoader(std::string_view shared_path) {
            auto path = std::string(shared_path) + SHARED_LIB_EXT;
            //TODO: choose dx and vulkan api 
            shared = LoadLibrary(path.data());
            if (!shared) {
                std::println("windows error: {}", GetLastError());
                //throw DnmGL::Error("failed to context lib load");
                return;
            }

            const auto load_func = (DnmGL::Context*((*)()))GetProcAddress(shared, "LoadContext");
            if (!load_func) {
                FreeLibrary(shared);
                //throw DnmGL::Error("failed to context load function");
                return;
            }
            
            context = load_func();
        }

        ~ContextLoader() {
            if (context) {
                delete context;
                FreeLibrary(shared);
            }
        }

        [[nodiscard]] DnmGL::Context *GetContext() const noexcept { return context; }

        ContextLoader(ContextLoader&&) = delete;
        ContextLoader(ContextLoader&) = delete;
        ContextLoader& operator=(ContextLoader&&) = delete;
        ContextLoader& operator=(ContextLoader&) = delete;
    private:
        DnmGL::Context *context = nullptr;
        HMODULE shared = nullptr;
    };
};
#endif
//TODO: make for posix 
#pragma once

enum class OS {
    eLinux,
    eWin,
    eMac,
    eAndroid,
    eIos,
};

#ifdef _WIN32
    constexpr OS _os = OS::eWin;
    #define OS_WIN
#elif defined(__APPLE__)
#include "TargetConditionals.h"
#if TARGET_OS_IPHONE
    constexpr OS _os = OS::eIos;
    #define OS_IOS
#else
    constexpr OS _os = OS::eMac;
    #define OS_MAC
#endif
#elif defined(__ANDROID__)
    constexpr OS _os = OS::eAndroid;
    #define OS_ANDROID
#elif defined(__linux__)
    constexpr OS _os = OS::eLinux;
    #define OS_LINUX
#else
#   error "unsupported OS"
#endif

#ifdef ENGINE_EXPORT
    #define ENGINE_API __declspec(dllexport)
#else
    #define ENGINE_API __declspec(dllimport)
//TODO: add linux, mac, ios etc. support
#endif

#ifdef _DEBUG
    constexpr bool _debug = true;
#else
    constexpr bool _debug = false;
#endif

#ifdef _WIN32
    #define SHARED_LIB_EXT ".dll"
#elif __linux__
    #define SHARED_LIB_EXT ".so"
//TODO: generate this ios, mac, android
#else
    #error unsupported os please use win or linux
#endif

#ifdef __clang__
    #define FORCE_INLINE [[clang::always_inline]]
#elif defined(__GNUC__)
    #define FORCE_INLINE [[gnu::always_inline]]
#endif
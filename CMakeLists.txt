cmake_minimum_required(VERSION 3.26)

project("DnmGL")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

option(Examples ON)
option(Vulkan ON)
option(D3D12 ON)

if(Vulkan)
    file(GLOB Vulkan_Sources
        ${CMAKE_CURRENT_SOURCE_DIR}/Src/Vulkan/*.cpp
    )

    find_package(Vulkan REQUIRED)
    find_package(VulkanMemoryAllocator CONFIG REQUIRED)
    find_package(unofficial-spirv-reflect CONFIG REQUIRED)

    add_library(DnmGL_Vulkan SHARED ${Vulkan_Sources})

    target_link_libraries(DnmGL_Vulkan PRIVATE
        Vulkan::Vulkan
        GPUOpen::VulkanMemoryAllocator
        unofficial::spirv-reflect
    )

    target_include_directories(DnmGL_Vulkan PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Header/
    )

    set_target_properties(DnmGL_Vulkan PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Vulkan
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Vulkan
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Vulkan
    )
endif(Vulkan)

if(D3D12)
    file(GLOB D3D12_Sources
        ${CMAKE_CURRENT_SOURCE_DIR}/Src/D3D12/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/D3D12MA/*.cpp
    )
    find_package(directx-headers CONFIG REQUIRED)

    add_library(DnmGL_D3D12 SHARED ${D3D12_Sources})

    target_link_libraries(DnmGL_D3D12 PRIVATE
        Microsoft::DirectX-Headers
        D3d12.lib
        dxcompiler.lib
        dxgi.lib
    )
    target_include_directories(DnmGL_D3D12 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Header/
        ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/
    )

    set_target_properties(DnmGL_D3D12 PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/D3D12
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/D3D12
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/D3D12
    )
endif(D3D12)


if(Examples)
    add_subdirectory(Examples)
endif()
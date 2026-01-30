cmake_minimum_required(VERSION 3.26)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

option(Examples ON)
option(Vulkan ON)
option(D3D12 ON)

project("DnmGL")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(Sources)
set(Libraries)
set(Includes)

function(copy_dlls_to_runtime target dll_dir)
    file(GLOB DLLS "${dll_dir}/*.dll")

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${DLLS}
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    )
endfunction()

list(APPEND Includes 
        ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/Slang/include)

if (Vulkan)
    find_package(Vulkan REQUIRED)
    find_package(VulkanMemoryAllocator CONFIG REQUIRED)
    find_package(unofficial-spirv-reflect CONFIG REQUIRED)

    file(GLOB Vulkan_Sources
        Src/Vulkan/*.cpp
    )
    set(Vulkan_Libraries
        Vulkan::Vulkan
        GPUOpen::VulkanMemoryAllocator
        unofficial::spirv-reflect
    )
    
    list(APPEND Sources ${Vulkan_Sources})
    list(APPEND Libraries ${Vulkan_Libraries})
endif()

if (D3D12 AND WIN32)
    find_package(directx-headers CONFIG REQUIRED)
    find_package(directx-dxc CONFIG REQUIRED)

    file(GLOB D3D12_Sources
        Src/D3D12/*.cpp
        ThirdParty/D3D12MA/*.cpp
    )
    set(D3D12_Libraries
        Microsoft::DirectXShaderCompiler
        Microsoft::DirectX-Headers
        D3d12
        dxcompiler
        dxgi
    )
    
    list(APPEND Sources ${D3D12_Sources})
    list(APPEND Libraries ${D3D12_Libraries})
    list(APPEND Includes 
        ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/D3D12MA)
endif()

add_library(${PROJECT_NAME} SHARED 
    Src/DnmGL.cpp
    ${Sources})

add_library(DnmGL::DnmGL ALIAS ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${Libraries}
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Header/
    ${Includes}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE DNMGL_EXPORTS)

if(Vulkan)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DnmGL_Vulkan_Backend)
endif(Vulkan)

if(D3D12)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DnmGL_D3D12_Backend)
endif(D3D12)

find_file(DXIL_DLL 
    NAMES dxil.dll 
    PATHS ${CMAKE_PREFIX_PATH}/bin ${CMAKE_PREFIX_PATH}/tools/directxshadercompiler
    NO_DEFAULT_PATH
)

list(APPEND DLLS ${DXIL_DLL})

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${DLLS}
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

if(Examples)
    add_subdirectory(Examples)
endif()

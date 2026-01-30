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
=======
project("DnmGL")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(Slang_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/Slang)

set(Sources)
set(Libraries)
set(Includes)

find_package(slang REQUIRED PATHS ThirdParty/Slang NO_DEFAULT_PATH)

function(copy_dlls_to_runtime target dll_dir)
    file(GLOB DLLS "${dll_dir}/*.dll")

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${DLLS}
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
>>>>>>> Stashed changes
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
<<<<<<< Updated upstream
=======
    
    list(APPEND Sources ${D3D12_Sources})
    list(APPEND Libraries ${D3D12_Libraries})
    list(APPEND Includes 
        ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/D3D12MA)
endif()

add_library(${PROJECT_NAME} SHARED 
    Src/DnmGL.cpp
    Src/ShaderCompiler.cpp
    ${Sources})

add_library(DnmGL::DnmGL ALIAS ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${Libraries}
    slang::slang
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Header/
    ${Includes}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE DNMGL_EXPORTS)

if(Vulkan)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DnmGL_Vulkan_Backend)
>>>>>>> Stashed changes
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

<<<<<<< Updated upstream
=======
find_file(DXIL_DLL 
    NAMES dxil.dll 
    PATHS ${CMAKE_PREFIX_PATH}/bin ${CMAKE_PREFIX_PATH}/tools/directxshadercompiler
    NO_DEFAULT_PATH
)
file(GLOB DLLS ThirdParty/Slang/bin/*.dll)

list(APPEND DLLS ${DXIL_DLL})

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${DLLS}
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
>>>>>>> Stashed changes

if(Examples)
    add_subdirectory(Examples)
endif()
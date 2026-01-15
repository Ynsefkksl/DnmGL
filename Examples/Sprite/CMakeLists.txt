cmake_minimum_required(VERSION 3.26)

project("Sprite")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

find_package(glfw3 CONFIG REQUIRED)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    glfw
    )

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Header/
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Examples_Executables
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Examples_Executables
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Examples_Executables
)
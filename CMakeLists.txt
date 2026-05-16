cmake_minimum_required(VERSION 3.24)

project(spaceguy
    VERSION 0.1.0
    LANGUAGES CXX
)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(spaceguy
    src/main.cpp
)

target_compile_features(spaceguy PRIVATE cxx_std_20)

target_include_directories(spaceguy
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(spaceguy
    PRIVATE
    Vulkan::Vulkan
    glfw
)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
cmake_minimum_required(VERSION 3.24)
include(FetchContent)

project(spaceguy
    VERSION 0.1.0
    LANGUAGES CXX
)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.3
)

FetchContent_MakeAvailable(glm)

add_executable(spaceguy
    src/main.cpp
    src/engine/vulkanBackend.cpp
    src/engine/predefined/vulkanGraphicPipelines.cpp
    src/engine/vulkanRenderer.cpp
    src/engine/predefined/vulkanDescriptorSetLayouts.cpp
    src/engine/blender/importer.cpp
    src/utils/buffers.cpp
    src/systems/animationSystem.cpp
    src/utils/input.cpp
    src/utils/time.cpp
    src/systems/sceneContext.cpp
    src/utils/math.cpp
    src/systems/resourceManagementSystem.cpp
    src/systems/projectileSystem.cpp
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
    glm::glm
)

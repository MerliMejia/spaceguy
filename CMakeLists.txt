set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
cmake_minimum_required(VERSION 3.24)
include(FetchContent)

project(spaceguy
    VERSION 0.1.0
    LANGUAGES C CXX
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
    src/systems/lightsSystem.cpp
    src/utils/input.cpp
    src/utils/time.cpp
    src/systems/sceneContext.cpp
    src/utils/math.cpp
    src/systems/resourceManagementSystem.cpp
    src/systems/projectileSystem.cpp
    src/systems/wizardBehaviorSystem.cpp
    src/systems/spacialGridHashSystem.cpp
    src/systems/particleLifeSystem.cpp
    src/systems/ogreBehaviorSystem.cpp
)

target_sources(spaceguy PRIVATE
    src/utils/textMeshGenerator.cpp

    external/libtess2/Source/bucketalloc.c
    external/libtess2/Source/dict.c
    external/libtess2/Source/geom.c
    external/libtess2/Source/mesh.c
    external/libtess2/Source/priorityq.c
    external/libtess2/Source/sweep.c
    external/libtess2/Source/tess.c
)

target_compile_features(spaceguy PRIVATE cxx_std_20)

target_include_directories(spaceguy
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/external/libtess2/Include
)

target_link_libraries(spaceguy
    PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
)

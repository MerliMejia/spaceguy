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
    src/systems/animationSystem.cpp
    src/utils/time.cpp
    src/systems/sceneContext.cpp
    src/utils/math.cpp
    src/systems/resourceManagementSystem.cpp
    src/systems/spacialGridHashSystem.cpp
    src/systems/gravitySystem.cpp
    external/stb/stb_image.cpp
    src/engine/renderer/shaders/banksManager.cpp
    src/engine/blender/v2/importer.cpp
    src/engine/input/orbitCamera.cpp
)

# Slang -> SPIR-V
find_program(SLANGC slangc
    HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
    REQUIRED
)

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders/v2)

add_custom_command(
    OUTPUT ${SHADER_DIR}/mainNode.spv
    COMMAND ${SLANGC} shaders/v2/mainNode.slang -o shaders/v2/mainNode.spv
    OUTPUT ${SHADER_DIR}/depthTest.spv
    COMMAND ${SLANGC} shaders/v2/depthTest.slang -o shaders/v2/depthTest.spv
    OUTPUT ${SHADER_DIR}/particlesCompute.spv
    COMMAND ${SLANGC} shaders/v2/particlesCompute.slang -o shaders/v2/particlesCompute.spv
    OUTPUT ${SHADER_DIR}/particlesDrawNode.spv
    COMMAND ${SLANGC} shaders/v2/particlesDrawNode.slang -o shaders/v2/particlesDrawNode.spv
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS
        ${SHADER_DIR}/mainNode.slang
        ${SHADER_DIR}/banks.slang
        ${SHADER_DIR}/types.slang
        ${SHADER_DIR}/depthTest.slang
        ${SHADER_DIR}/particlesCompute.slang
        ${SHADER_DIR}/particlesDrawNode.slang
        ${SHADER_DIR}/banks_shared.h
    COMMENT "Compiling shaders to SPIR-V"
    VERBATIM
)

add_custom_target(shaders DEPENDS ${SHADER_DIR}/mainNode.spv)
add_dependencies(spaceguy shaders)

target_compile_features(spaceguy PRIVATE cxx_std_20)

target_compile_definitions(spaceguy
    PRIVATE
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_RADIANS
    VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
)

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

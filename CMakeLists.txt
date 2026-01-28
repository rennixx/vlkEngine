cmake_minimum_required(VERSION 3.20)
project(VulkanEngine C VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Platform detection
if(WIN32)
    set(PLATFORM_WINDOWS TRUE)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
elseif(UNIX AND NOT APPLE)
    set(PLATFORM_LINUX TRUE)
elseif(APPLE)
    set(PLATFORM_MACOS TRUE)
    message(FATAL_ERROR "macOS is not currently supported")
endif()

# Options
option(ENABLE_VALIDATION "Enable Vulkan validation layers" ON)
option(ENABLE_PROFILING "Enable Tracy profiling" OFF)
option(BUILD_TESTS "Build unit tests" ON)

# Dependencies
find_package(Vulkan REQUIRED)

# GLFW (we'll use the one in external/)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(external/glfw)

# stb libraries
set(STB_DIR ${CMAKE_SOURCE_DIR}/external/stb)

# External include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external/Vulkan-Headers/include
    ${CMAKE_SOURCE_DIR}/external/Volk
    ${CMAKE_SOURCE_DIR}/external/cglm/include
    ${CMAKE_SOURCE_DIR}/external/tinygltf
    ${STB_DIR}
)

# Engine library
set(ENGINE_SOURCES
    # Core
    src/core/logger.c
    src/core/memory.c
    src/core/assert.c
    src/core/timer.c
    src/core/thread.c

    # Platform
    src/platform/platform.c
    src/platform/platform.h

    # Renderer
    src/renderer/vulkan_core.c
    src/renderer/swapchain.c
    src/renderer/render_pass.c
    src/renderer/framebuffer.c
    src/renderer/pipeline.c
    src/renderer/command_buffer.c
    src/renderer/descriptor.c
    src/renderer/sync.c
    src/renderer/buffer.c
    src/renderer/image.c
    src/renderer/shader.c

    # ECS
    src/ecs/ecs.c
    src/ecs/components.c
    src/ecs/systems.c

    # Scene
    src/scene/scene.c
    src/scene/node.c
    src/scene/camera.c

    # Assets
    src/assets/asset_manager.c
    src/assets/model_loader.c
    src/assets/texture_loader.c
)

add_library(vulkan_engine STATIC ${ENGINE_SOURCES})
target_link_libraries(vulkan_engine
    PUBLIC
        Vulkan::Vulkan
        glfw
)

# Volk implementation
target_sources(vulkan_engine PRIVATE
    external/Volk/volk.c
)

# Platform-specific sources
if(PLATFORM_WINDOWS)
    target_sources(vulkan_engine PRIVATE src/platform/windows.c)
    target_link_libraries(vulkan_engine PUBLIC
        user32
        gdi32
        shell32
    )
elseif(PLATFORM_LINUX)
    target_sources(vulkan_engine PRIVATE src/platform/linux.c)
    target_link_libraries(vulkan_engine PUBLIC
        dl
        pthread
        X11
    )
endif()

# Validation layers define
if(ENABLE_VALIDATION)
    target_compile_definitions(vulkan_engine PUBLIC VE_ENABLE_VALIDATION)
endif()

# Profiling define
if(ENABLE_PROFILING)
    target_compile_definitions(vulkan_engine PUBLIC VE_ENABLE_PROFILING)
    target_link_libraries(vulkan_engine PUBLIC Tracy::TracyClient)
endif()

# Executable
set(EXECUTABLE_SOURCES
    src/main.c
)

add_executable(vulkan_engine_exe ${EXECUTABLE_SOURCES})
target_link_libraries(vulkan_engine_exe
    PRIVATE
        vulkan_engine
)

set_target_properties(vulkan_engine_exe PROPERTIES
    OUTPUT_NAME "vulkan_engine"
)

# Copy Vulkan DLLs on Windows
if(PLATFORM_WINDOWS)
    add_custom_command(TARGET vulkan_engine_exe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE_DIR:Vulkan::Vulkan>/vulkan-1.dll"
            $<TARGET_FILE_DIR:vulkan_engine_exe>
        COMMAND_EXPAND_LISTS
    )
endif()

# Shader compilation
find_program(GLSLC_EXECUTABLE glslc)
if(GLSLC_EXECUTABLE)
    set(SHADER_DIR ${CMAKE_SOURCE_DIR}/shaders)
    set(SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/shaders)

    file(GLOB SHADER_SOURCES ${SHADER_DIR}/*.vert ${SHADER_DIR}/*.frag ${SHADER_DIR}/*.comp ${SHADER_DIR}/*.rchit ${SHADER_DIR}/*.rahit ${SHADER_DIR}/*.rgen ${SHADER_DIR}/*.rmiss)

    foreach(SHADER ${SHADER_SOURCES})
        get_filename_component(SHADER_NAME ${SHADER} NAME)
        set(SPIRV_OUTPUT ${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv)

        add_custom_command(
            OUTPUT ${SPIRV_OUTPUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
            COMMAND ${GLSLC_EXECUTABLE} ${SHADER} -o ${SPIRV_OUTPUT}
            DEPENDS ${SHADER}
            COMMENT "Compiling shader ${SHADER_NAME}"
        )

        list(APPEND SPIRV_OUTPUTS ${SPIRV_OUTPUT})
    endforeach()

    add_custom_target(shaders DEPENDS ${SPIRV_OUTPUTS})
    add_dependencies(vulkan_engine_exe shaders)
else()
    message(WARNING "glslc not found - shaders will not be compiled automatically")
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()

    set(TEST_SOURCES
        tests/test_main.c
        tests/test_memory.c
        tests/test_ecs.c
    )

    add_executable(vulkan_engine_tests ${TEST_SOURCES})
    target_link_libraries(vulkan_engine_tests PRIVATE vulkan_engine)

    add_test(NAME EngineTests COMMAND vulkan_engine_tests)
endif()

# Installation
install(TARGETS vulkan_engine_exe
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY shaders/
    DESTINATION shaders
    FILES_MATCHING PATTERN "*.vert" PATTERN "*.frag" PATTERN "*.comp"
)

# Compiler settings
if(MSVC)
    target_compile_options(vulkan_engine PRIVATE /W4 /WX-)
    target_compile_definitions(vulkan_engine PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(vulkan_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

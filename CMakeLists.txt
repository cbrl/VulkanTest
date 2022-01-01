cmake_minimum_required(VERSION 3.20)
project(VulkanTest)

set(CMAKE_CXX_SYSROOT_FLAG_CODE "list(APPEND CMAKE_CXX_SOURCE_FILE_EXTENSIONS ixx)")

# Find required packages
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
set(VULKAN_SDK_INCLUDE_DIRS $ENV{VULKAN_SDK}/Include)

# Release/Debug options
if (MSVC)
	set(DEBUG_OPTIONS /W4 /Od)
	set(RELEASE_OPTIONS /W4 /O2 /DNDEBUG)
	set(DEFINES NOMINMAX VK_USE_PLATFORM_WIN32_KHR)
else()
	set(DEBUG_OPTIONS -Wall -Wextra -O0 -g)
	set(RELEASE_OPTIONS -Wall -Wextra -O3 -DNDEBUG)
	set(DEFINES VK_USE_PLATFORM_XCB_KHR)
endif()

add_subdirectory(glslang)

# Create target
add_executable(${PROJECT_NAME}
	src/main.cpp
	src/utils/utils.cpp
	src/utils/shaders.cpp
	src/utils/math.cpp
	src/vulkan_wrapper/vkw.ixx
	src/vulkan_wrapper/buffer.ixx
	src/vulkan_wrapper/commands.ixx
	src/vulkan_wrapper/debug.ixx
	src/vulkan_wrapper/descriptor.ixx
	src/vulkan_wrapper/image.ixx
	src/vulkan_wrapper/instance.ixx
	src/vulkan_wrapper/logical_device.ixx
	src/vulkan_wrapper/pipeline.ixx
	src/vulkan_wrapper/pipeline_layout.ixx
	src/vulkan_wrapper/queue.ixx
	src/vulkan_wrapper/render_pass.ixx
	src/vulkan_wrapper/render_pass_single.ixx
	src/vulkan_wrapper/shader.ixx
	src/vulkan_wrapper/subpass.ixx
	src/vulkan_wrapper/swapchain.ixx
	src/vulkan_wrapper/util.ixx
	src/vulkan_wrapper/window.ixx
	#src/glslang/StandAlone/ResourceLimits.cpp
)

# Set target compile options
target_compile_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:DEBUG>:${DEBUG_OPTIONS}>")
target_compile_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:RELEASE>:${RELEASE_OPTIONS}>")
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_compile_definitions(${PROJECT_NAME} PRIVATE ${DEFINES} VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)

# Include folders
target_include_directories(${PROJECT_NAME} PRIVATE
	src
	${Vulkan_INCLUDE_DIRS}
	${VULKAN_SDK_INCLUDE_DIRS}
	${VULKAN_HPP_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
	glfw
	glm::glm
	SPIRV
	glslang
	glslang-default-resource-limits
	#$ENV{VULKAN_SDK}/Lib
)

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:
        /permissive- /MP /experimental:module>
)
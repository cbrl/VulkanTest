cmake_minimum_required(VERSION 3.15)
project(VulkanTest)

# Find required packages
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
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
	#src/glslang/StandAlone/ResourceLimits.cpp
)

# Set target compile options
target_compile_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:DEBUG>:${DEBUG_OPTIONS}>")
target_compile_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:RELEASE>:${RELEASE_OPTIONS}>")
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
target_compile_definitions(${PROJECT_NAME} PRIVATE ${DEFINES} VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)

# Include folders
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_include_directories(${PROJECT_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${VULKAN_SDK_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${VULKAN_HPP_INCLUDE_DIRS})

# Link libraries
#target_link_directories(${PROJECT_NAME} PRIVATE $ENV{VULKAN_SDK}/Lib)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
target_link_libraries(${PROJECT_NAME} PRIVATE SPIRV)
target_link_libraries(${PROJECT_NAME} PRIVATE glslang glslang-default-resource-limits)

#pragma once

#include <memory>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.hpp>


namespace vkw {

class window {
public:
	window(vk::raii::Instance& instance, const std::string& name, const vk::Extent2D& size) : name(name), size(size) {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		handle = glfwCreateWindow(size.width, size.height, name.c_str(), nullptr, nullptr);
		if (not handle) {
			throw std::runtime_error("Failed to create window");
		}

		auto surface_khr = VkSurfaceKHR{};

		const auto err = glfwCreateWindowSurface(static_cast<VkInstance>(*instance), handle, nullptr, &surface_khr);
		if (err != VK_SUCCESS) {
			throw std::runtime_error("Failed to create window surface");
		}

		surface = std::make_unique<vk::raii::SurfaceKHR>(instance, surface_khr);
	}

	~window() {
		glfwDestroyWindow(handle);
	}


	[[nodiscard]]
	auto get_handle() noexcept -> GLFWwindow* {
		return handle;
	}

	[[nodiscard]]
	auto get_handle() const noexcept -> const GLFWwindow* {
		return handle;
	}

	[[nodiscard]]
	auto get_surface() -> vk::raii::SurfaceKHR& {
		return *surface;
	}

	[[nodiscard]]
	auto get_surface() const -> const vk::raii::SurfaceKHR& {
		return *surface;
	}

	[[nodiscard]]
	auto get_name() const noexcept -> const std::string& {
		return name;
	}

	[[nodiscard]]
	auto get_size() const noexcept -> const vk::Extent2D& {
		return size;
	}

private:
	std::string name;
	vk::Extent2D size;

	GLFWwindow* handle;
	std::unique_ptr<vk::raii::SurfaceKHR> surface;
};

} //namespace vkw

#pragma once

#include <memory>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.hpp>


namespace vkw {

class Window {
public:
	Window(vk::raii::Instance& instance, const std::string& name, const vk::Extent2D& size) : name(name), size(size) {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		handle = glfwCreateWindow(size.width, size.height, name.c_str(), nullptr, nullptr);
		if (!handle) {
			throw std::runtime_error("Failed to create window");
		}

		auto surface_khr = VkSurfaceKHR{};

		const auto err = glfwCreateWindowSurface(static_cast<VkInstance>(*instance), handle, nullptr, &surface_khr);
		if (err != VK_SUCCESS) {
			throw std::runtime_error("Failed to create window surface");
		}

		surface = std::make_unique<vk::raii::SurfaceKHR>(instance, surface_khr);
	}

	~Window() {
		glfwDestroyWindow(handle);
	}

	[[nodiscard]]
	auto getName() const -> const std::string& {
		return name;
	}

	[[nodiscard]]
	auto getSize() const -> const vk::Extent2D& {
		return size;
	}

	[[nodiscard]]
	auto getHandle() noexcept -> GLFWwindow* {
		return handle;
	}

	[[nodiscard]]
	auto getHandle() const noexcept -> const GLFWwindow* {
		return handle;
	}

	[[nodiscard]]
	auto getSurface() -> vk::raii::SurfaceKHR& {
		return *surface;
	}

	[[nodiscard]]
	auto getSurface() const -> const vk::raii::SurfaceKHR& {
		return *surface;
	}

private:
	std::string name;
	vk::Extent2D size;

	GLFWwindow* handle;
	std::unique_ptr<vk::raii::SurfaceKHR> surface;
};

} //namespace vkw

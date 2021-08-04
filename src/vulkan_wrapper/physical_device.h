#pragma once

#include <cassert>
#include <iterator>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

#include <vulkan/vulkan.hpp>


namespace vkw::util {

auto findGraphicsAndPresentQueueFamilies(vk::raii::PhysicalDevice& physical_device, vk::raii::SurfaceKHR& surface) -> std::pair<std::optional<uint32_t>, std::optional<uint32_t>> {
	const auto queue_families = findQueueFamilyIndices(physical_device, vk::QueueFlagBits::eGraphics);
	assert(not queue_families.empty());

	auto graphics_queue = std::optional<uint32_t>{};
	auto present_queue  = std::optional<uint32_t>{};

	// Try to find a queue that supports both graphics and present
	for (auto family : queue_families) {
		if (physical_device.getSurfaceSupportKHR(family, *surface)) {
			graphics_queue = family;
			present_queue  = family;
			break;
		}
	}

	// If no queue supported both graphics and present, use the first queue that supports graphics,
	// and look for a separate queue that supports present.
	if (not graphics_queue) {
		graphics_queue = queue_families[0];

		for (auto family : queue_families) {
			if (physical_device.getSurfaceSupportKHR(family, *surface)) {
				present_queue = family;
			}
		}
	}

	return {graphics_queue, present_queue};
}

} //namespace vkw::util

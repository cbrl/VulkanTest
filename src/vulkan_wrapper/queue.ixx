module;

#include <cstdint>
#include <exception>
#include <format>
#include <iostream>
#include <optional>
#include <ranges>
#include <span>

#include <vulkan/vulkan_raii.hpp>

export module vkw.queue;

import vkw.util;


namespace vkw::detail {

[[nodiscard]]
auto index_of(const auto& list, auto&& test) -> std::optional<uint32_t> {
	const auto it = std::ranges::find_if(list, test);
	if (it == list.end()) {
		return {};
	}
	else {
		return static_cast<uint32_t>(std::distance(list.begin(), it));
	}
}

} //namespace vkw::detail


export namespace vkw {

struct queue_info {
	float priority = 1.0f;
};

struct queue_family_info {
	uint32_t family_idx;
	vk::QueueFlags flags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eTransfer;
	std::vector<queue_info> queues;
};

class queue : public vk::raii::Queue {
public:
	queue(const vk::raii::Device& device, uint32_t family_idx, uint32_t queue_idx) :
		vk::raii::Queue(device, family_idx, queue_idx),
		family_index(family_idx),
		queue_index(queue_idx) {
	}

	uint32_t family_index;
	uint32_t queue_index;
};


namespace util {

// Find the first queue family which has at least the specified flags
[[nodiscard]]
auto find_queue_family_index_weak(const std::vector<vk::QueueFamilyProperties>& queue_family_properties, vk::QueueFlags flags) -> std::optional<uint32_t> {
	return detail::index_of(queue_family_properties, [&](const auto& qfp) { return (qfp.queueFlags & flags) == flags; });
}

// Find the first queue family which has at least the specified flags
[[nodiscard]]
auto find_queue_family_index_weak(vk::PhysicalDevice physical_device, vk::QueueFlags flags) -> std::optional<uint32_t> {
	return find_queue_family_index_weak(physical_device.getQueueFamilyProperties(), flags);
}


// Find the first queue family which exactly matches the specified flags
[[nodiscard]]
auto find_queue_family_index_strong(const std::vector<vk::QueueFamilyProperties>& queue_family_properties, vk::QueueFlags flags) -> std::optional<uint32_t> {
	return detail::index_of(queue_family_properties, [&](const auto& qfp) { return qfp.queueFlags == flags; });
}

// Find the first queue family which exactly matches the specified flags
[[nodiscard]]
auto find_queue_family_index_strong(vk::PhysicalDevice physical_device, vk::QueueFlags flags) -> std::optional<uint32_t> {
	return find_queue_family_index_strong(physical_device.getQueueFamilyProperties(), flags);
}


// Find all queue families which have at least the specified flags
[[nodiscard]]
auto find_queue_family_indices_weak(const std::vector<vk::QueueFamilyProperties>& queue_family_properties, vk::QueueFlags flags) -> std::vector<uint32_t> {
	const auto is_valid = [&](uint32_t idx) {
		return (queue_family_properties[idx].queueFlags & flags) == flags;
	};

	auto indices = std::views::iota(uint32_t{0}, static_cast<uint32_t>(queue_family_properties.size()));
	auto valid   = indices | std::views::filter(is_valid);

	return std::vector<uint32_t>(valid.begin(), valid.end());
}

// Find all queue families which have at least the specified flags
[[nodiscard]]
auto find_queue_family_indices_weak(vk::PhysicalDevice physical_device, vk::QueueFlags flags) -> std::vector<uint32_t> {
	return find_queue_family_indices_weak(physical_device.getQueueFamilyProperties(), flags);
}


// Find all queue families which exactly match the specified flags
[[nodiscard]]
auto find_queue_family_indices_strong(const std::vector<vk::QueueFamilyProperties>& queue_family_properties, vk::QueueFlags flags) -> std::vector<uint32_t> {
	const auto is_valid = [&](uint32_t idx) {
		return queue_family_properties[idx].queueFlags == flags;
	};

	auto indices = std::views::iota(uint32_t{0}, static_cast<uint32_t>(queue_family_properties.size()));
	auto valid   = indices | std::views::filter(is_valid);

	return std::vector<uint32_t>(valid.begin(), valid.end());
}

// Find all queue families which exactly match the specified flags
[[nodiscard]]
auto find_queue_family_indices_strong(vk::PhysicalDevice physical_device, vk::QueueFlags flags) -> std::vector<uint32_t> {
	return find_queue_family_indices_strong(physical_device.getQueueFamilyProperties(), flags);
}


// Find the first queue with present support for the specified surface
[[nodiscard]]
auto find_present_queue_index(vk::PhysicalDevice physical_device, const vk::SurfaceKHR& surface) -> std::optional<uint32_t> {
	for (auto family_idx : std::views::iota(uint32_t{0}, physical_device.getQueueFamilyProperties().size())) {
		if (physical_device.getSurfaceSupportKHR(family_idx, surface)) {
			return family_idx;
		}
	}
	return {};
}

// Find the first queue with present support for the specified surface
[[nodiscard]]
auto find_present_queue_indices(vk::PhysicalDevice physical_device, const vk::SurfaceKHR& surface) -> std::vector<uint32_t> {
	const auto is_valid = [&](uint32_t idx) {
		return physical_device.getSurfaceSupportKHR(idx, surface);
	};

	auto indices = std::views::iota(uint32_t{0}, static_cast<uint32_t>(physical_device.getQueueFamilyProperties().size()));
	auto valid   = indices | std::views::filter(is_valid);

	return std::vector<uint32_t>(valid.begin(), valid.end());
}
} //namespace util


namespace debug {
auto validate_queues(std::span<const queue_family_info> queue_family_info_list, const std::vector<vk::QueueFamilyProperties>& queue_family_properties) -> void {
	if (queue_family_properties.empty()) {
		std::cout << "No queue family properties" << std::endl;
		throw std::runtime_error{"No queue family properties"};
	}

	bool invalid_queues = false;
	for (const auto& family : queue_family_info_list) {
		const auto& property = queue_family_properties[family.family_idx];

		if (family.family_idx >= queue_family_properties.size()) {
			std::cout << "Queue family index out of range:\n"
			          << std::format("\tIndex = {}\n", family.family_idx)
			          << std::format("\tLimit = {}\n", queue_family_properties.size() - 1);
			invalid_queues = true;
		}

		if (family.queues.empty()) {
			std::cout << std::format("Empty queue list for queue family {}\n", family.family_idx);
			invalid_queues = true;
		}

		if (family.queues.size() > property.queueCount) {
			std::cout << std::format("Too many queues specified for family {}:\n", family.family_idx)
			          << std::format("\tTotal = {}\n", family.queues.size())
			          << std::format("\tLimit = {}\n", property.queueCount);
			invalid_queues = true;
		}

		if ((property.queueFlags & family.flags) != family.flags) {
			std::cout << std::format("Queue family {} does not support the requested flags\n", family.family_idx)
			          << std::format("\tRequested: {}\n", vk::to_string(family.flags))
			          << std::format("\tAvailable: {}\n", vk::to_string(property.queueFlags));
			invalid_queues = true;
		}

		for (auto idx : std::views::iota(size_t{0}, family.queues.size())) {
			const auto& queue = family.queues[idx];

			if ((queue.priority < 0) || (queue.priority > 1)) {
				std::cout << std::format("Invalid priority for queue {} in family {}: {}\n", idx, family.family_idx, queue.priority);
				invalid_queues = true;
			}
		}
	}

	if (invalid_queues) {
		throw std::runtime_error("Invalid queues specified");
	}
}
} //namespace debug
} //namespace vkw

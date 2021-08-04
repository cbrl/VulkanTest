#pragma once

#include <vulkan/vulkan.hpp>

#include "util.h"


namespace vkw {

struct QueueInfo {
	float priority = 1.0f;
};

struct QueueFamilyInfo {
	uint32_t family_idx;
	vk::QueueFlags flags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eTransfer;
	std::vector<QueueInfo> queues;
};

class Queue : public vk::raii::Queue {
public:
	Queue(vk::raii::Device& device, uint32_t family_idx, uint32_t queue_idx) :
		vk::raii::Queue(device, family_idx, queue_idx),
		family_index(family_idx),
		queue_index(queue_idx) {
	}

	uint32_t family_index;
	uint32_t queue_index;
};


namespace detail {
auto indexOf(const auto& list, auto&& test) -> std::optional<uint32_t> {
	const auto it = std::ranges::find_if(list, test);
	if (it == list.end()) {
		return {};
	}
	else {
		return static_cast<uint32_t>(std::distance(list.begin(), it));
	}
}
} //namespace detail


namespace util {
// Find the first queue family which has the desired flags
auto findQueueFamilyIndexWeak(const std::vector<vk::QueueFamilyProperties>& queue_family_properties, vk::QueueFlags flags) -> std::optional<uint32_t> {
	return detail::indexOf(queue_family_properties, [&](const auto& qfp) { return (qfp.queueFlags & flags) == flags; });
}
auto findQueueFamilyIndexWeak(const vk::raii::PhysicalDevice& physical_device, vk::QueueFlags flags) -> std::optional<uint32_t> {
	return findQueueFamilyIndexWeak(physical_device.getQueueFamilyProperties(), flags);
}

// Find the first queue family which has the exact flags
auto findQueueFamilyIndexStrong(const std::vector<vk::QueueFamilyProperties>& queue_family_properties, vk::QueueFlags flags) -> std::optional<uint32_t> {
	return detail::indexOf(queue_family_properties, [&](const auto& qfp) { return qfp.queueFlags == flags; });
}
auto findQueueFamilyIndexStrong(const vk::raii::PhysicalDevice& physical_device, vk::QueueFlags flags) -> std::optional<uint32_t> {
	return findQueueFamilyIndexStrong(physical_device.getQueueFamilyProperties(), flags);
}


// Find all queue families which have the desired flags
auto findQueueFamilyIndices(const std::vector<vk::QueueFamilyProperties>& queue_family_properties, vk::QueueFlags flags) -> std::vector<uint32_t> {
	const auto is_valid = [&](uint32_t idx) {
		return (queue_family_properties[idx].queueFlags & flags) == flags;
	};

	auto indices = std::views::iota(uint32_t{0}, static_cast<uint32_t>(queue_family_properties.size()));
	auto valid   = indices | std::views::filter(is_valid);

	return std::vector<uint32_t>(valid.begin(), valid.end());
}

auto findQueueFamilyIndices(const vk::raii::PhysicalDevice& physical_device, vk::QueueFlags flags) -> std::vector<uint32_t> {
	return findQueueFamilyIndices(physical_device.getQueueFamilyProperties(), flags);
}
} //namespace util

namespace debug {

auto validateQueues(const std::vector<QueueFamilyInfo>& queue_family_info_list, const std::vector<vk::QueueFamilyProperties>& queue_family_properties) -> void {
	if (queue_family_properties.empty()) {
		std::cout << "No queue family properties" << std::endl;
		throw std::runtime_error("No queue family properties");
	}

	bool invalid_queues = false;
	for (const auto& family : queue_family_info_list) {
		const auto& property = queue_family_properties[family.family_idx];

		if (family.family_idx >= queue_family_properties.size()) {
			std::cout << "Queue family index out of range:\n"
			          << "  Index = " << family.family_idx << '\n'
			          << "  Limit = " << (queue_family_properties.size() - 1) << '\n';
			invalid_queues = true;
		}

		if (family.queues.empty()) {
			std::cout << "Empty queue list for queue family " << family.family_idx << '\n';
			invalid_queues = true;
		}

		if (family.queues.size() > property.queueCount) {
			std::cout << "Too many queues specified for family " << family.family_idx << ":\n"
			          << "  Total = " << family.queues.size() << '\n'
			          << "  Limit = " << property.queueCount << '\n';
			invalid_queues = true;
		}

		if ((property.queueFlags & family.flags) != family.flags) {
			std::cout << "Queue family " << family.family_idx << " does not support the requested flags\n"
			          << "  Requested: " << vk::to_string(family.flags) << '\n'
			          << "  Available: " << vk::to_string(property.queueFlags) << '\n';
			invalid_queues = true;
		}

		for (auto idx : std::views::iota(size_t{0}, family.queues.size())) {
			const auto& queue = family.queues[idx];

			if ((queue.priority < 0) || (queue.priority > 1)) {
				std::cout << "Invalid priority for queue " << idx << " in family " << family.family_idx << ": " << queue.priority << '\n';
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
#pragma once

#include <bit>
#include <memory>
#include <functional>
#include <ranges>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "debug.h"
#include "queue.h"
#include "util.h"


namespace vkw {

class LogicalDevice {
public:
	struct LogicalDeviceInfo {
		auto addQueues(vk::QueueFlags flags, float priority, uint32_t count = 1) -> bool {
			const auto properties = physical_device.get().getQueueFamilyProperties();

			// Enumerate available queue properties, and subtract the number of currently added
			// queues from their queueCounts.
			auto available_props = decltype(properties){};
			available_props.resize(properties.size());

			for (auto family_idx : std::views::iota(size_t{0}, properties.size())) {
				auto prop = properties[family_idx];

				for (const auto& family_info : queue_family_info_list) {
					if (family_info.family_idx == family_idx) {
						prop.queueCount -= static_cast<uint32_t>(family_info.queues.size());
						break;
					}
				}

				available_props.push_back(std::move(prop));
			}

			// Try to find a queue with the exact combination of flags specified
			const auto exact_idx = util::findQueueFamilyIndexStrong(available_props, flags);
			if (exact_idx.has_value() && (available_props[exact_idx.value()].queueCount >= count)) {
				addQueues(exact_idx.value(), flags, priority, count);
				return true;
			}

			// If no queues with the exact flags were found, then add the first one which has a match.
			const auto weak_idx = util::findQueueFamilyIndexWeak(available_props, flags);
			if (weak_idx.has_value() && (available_props[weak_idx.value()].queueCount >= count)) {
				addQueues(weak_idx.value(), flags, priority, count);
				return true;
			}

			return false;
		}

		auto addQueues(uint32_t family_idx, vk::QueueFlags flags, float priority, uint32_t count = 1) -> void {
			for (auto& family_info : queue_family_info_list) {
				if (family_info.family_idx == family_idx) {
					for (auto i : std::views::iota(uint32_t{0}, count)) {
						family_info.queues.emplace_back(flags, priority);
					}
					return;
				}
			}

			// Add a new QueueFamilyInfo if one with the specified family index was not found
			auto& family_info = queue_family_info_list.emplace_back(family_idx);
			for (auto i : std::views::iota(uint32_t{0}, count)) {
				family_info.queues.emplace_back(flags, priority);
			}
		}

		std::reference_wrapper<vk::raii::PhysicalDevice> physical_device;
		vk::PhysicalDeviceFeatures features;
		std::vector<const char*> extensions;
		std::vector<QueueFamilyInfo> queue_family_info_list;
	};

	LogicalDevice(const LogicalDeviceInfo& info) : device_info(info) {
		const auto queue_family_properties = device_info.physical_device.get().getQueueFamilyProperties();

		// Validate the queues and extensions
		debug::validateQueues(device_info.queue_family_info_list, queue_family_properties);
		debug::validateExtensions(device_info.extensions, device_info.physical_device.get().enumerateDeviceExtensionProperties());


		// Build the queue create info list
		auto queue_create_info_list = std::vector<vk::DeviceQueueCreateInfo>{};
		queue_create_info_list.reserve(device_info.queue_family_info_list.size());

		std::vector<std::vector<float>> priorities;
		priorities.reserve(device_info.queue_family_info_list.size());

		for (const auto& family : device_info.queue_family_info_list) {
			auto& priority_list = priorities.emplace_back();

			for (const auto& queue : family.queues) {
				priority_list.push_back(queue.priority);
			}

			auto create_info = vk::DeviceQueueCreateInfo{
				vk::DeviceQueueCreateFlags{},
				family.family_idx,
				priority_list
			};

			queue_create_info_list.push_back(std::move(create_info));
		}


		// Create the device
		const auto device_create_info = vk::DeviceCreateInfo{
			vk::DeviceCreateFlags{},
			queue_create_info_list,
			{},
			device_info.extensions,
			&device_info.features
		};

		device = std::make_unique<vk::raii::Device>(device_info.physical_device, device_create_info);


		// First add the queues which were an exact match for their queue flags
		for (const auto& family : device_info.queue_family_info_list) {			
			for (auto queue_idx : std::views::iota(uint32_t{0}, static_cast<uint32_t>(family.queues.size()))) {
				const auto flags = family.queues[queue_idx].flags;
				const auto flag_mask = static_cast<vk::QueueFlags::MaskType>(flags);

				if (flags == queue_family_properties[family.family_idx].queueFlags) {
					auto queue = std::make_unique<vk::raii::Queue>(*device, family.family_idx, queue_idx);
					queue_map[flag_mask].push_back(std::ref(*queue));
					queues.push_back(std::move(queue));
				}
			}
		}

		// Create the remaining queues that weren't an exact match
		for (const auto& family : device_info.queue_family_info_list) {
			for (auto queue_idx : std::views::iota(uint32_t{0}, static_cast<uint32_t>(family.queues.size()))) {
				const auto flags = family.queues[queue_idx].flags;
				const auto flag_mask = static_cast<vk::QueueFlags::MaskType>(flags);
				
				if (flags != queue_family_properties[family.family_idx].queueFlags) {
					auto queue = std::make_unique<vk::raii::Queue>(*device, family.family_idx, queue_idx);
					queue_map[flag_mask].push_back(std::ref(*queue));
					queues.push_back(std::move(queue));
				}
			}
		}

		// Map every combination of each queue's flags
		for (const auto& family : device_info.queue_family_info_list) {
			for (auto queue_idx : std::views::iota(uint32_t{0}, static_cast<uint32_t>(family.queues.size()))) {
				const auto flags = family.queues[queue_idx].flags;
				const auto separated_flags = util::separateFlags(flags);

				auto& queue = getQueue(flags, 0);

				// Map every combination of the existing queue's flags, other than the full mask,
				// since that mapping already exists.
				for (ptrdiff_t i = (std::bit_ceil(separated_flags.size()) << 1) - 2; i > 0; --i) {
					auto mask = vk::QueueFlags::MaskType{0};

					for (size_t bit = 0; bit < separated_flags.size(); ++bit) {
						if (i & (size_t{1} << bit)) {
							mask |= static_cast<vk::QueueFlags::MaskType>(separated_flags[bit]);
						}
					}

					queue_map[mask].push_back(std::ref(queue));
				}
			}
		}
	}

	[[nodiscard]]
	auto getDeviceInfo() const -> const LogicalDeviceInfo& {
		return device_info;
	}

	[[nodiscard]]
	auto getVkPhysicalDevice() -> vk::raii::PhysicalDevice& {
		return device_info.physical_device;
	}

	[[nodiscard]]
	auto getVkPhysicalDevice() const -> const vk::raii::PhysicalDevice& {
		return device_info.physical_device;
	}

	[[nodiscard]]
	auto getVkDevice() -> vk::raii::Device& {
		return *device;
	}

	[[nodiscard]]
	auto getVkDevice() const -> const vk::raii::Device& {
		return *device;
	}

	[[nodiscard]]
	auto getQueue(vk::QueueFlags flag, uint32_t queue_idx) -> vk::raii::Queue& {
		return getQueues(flag).at(queue_idx);
	}

	[[nodiscard]]
	auto getQueues(vk::QueueFlags flag) -> const std::vector<std::reference_wrapper<vk::raii::Queue>>& {
		return queue_map[static_cast<vk::QueueFlags::MaskType>(flag)];
	}

private:

	LogicalDeviceInfo device_info;
	std::unique_ptr<vk::raii::Device> device;

	std::vector<std::unique_ptr<vk::raii::Queue>> queues;
	std::unordered_map<vk::QueueFlags::MaskType, std::vector<std::reference_wrapper<vk::raii::Queue>>> queue_map;
};

} //namespace vkw

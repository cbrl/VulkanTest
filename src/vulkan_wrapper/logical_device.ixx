module;

#include <bit>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module vkw.logical_device;

import vkw.debug;
import vkw.queue;
import vkw.util;


namespace vkw {
export struct logical_device_info {
	auto add_all_queues(float priority) -> void {
		queue_family_info_list.clear();
		const auto properties = physical_device.get().getQueueFamilyProperties();

		for (auto family_idx : std::views::iota(size_t{0}, properties.size())) {
			const auto& prop = properties[family_idx];
			add_queues(static_cast<uint32_t>(family_idx), priority, prop.queueCount);
		}
	}

	auto add_queues(vk::QueueFlags flags, float priority, uint32_t count = 1) -> std::optional<uint32_t> {
		const auto properties = physical_device.get().getQueueFamilyProperties();

		// Enumerate available queue properties, and subtract the number of currently added
		// queues from their queue counts.
		auto available_props = decltype(properties){};
		available_props.reserve(properties.size());

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
		const auto exact_idx = util::find_queue_family_index_strong(available_props, flags);
		if (exact_idx.has_value() and (available_props[*exact_idx].queueCount >= count)) {
			add_queues(exact_idx.value(), priority, count);
			return exact_idx;
		}

		// If no queues with the exact flags were found, then add the first one which has a match.
		const auto weak_idx = util::find_queue_family_index_weak(available_props, flags);
		if (weak_idx.has_value() and (available_props[*weak_idx].queueCount >= count)) {
			add_queues(weak_idx.value(), priority, count);
			return weak_idx;
		}

		return {};
	}

	auto add_queues(uint32_t family_idx, float priority, uint32_t count = 1) -> void {
		for (auto& family_info : queue_family_info_list) {
			if (family_info.family_idx == family_idx) {
				for (uint32_t i = 0; i < count; ++i) {
					family_info.queues.emplace_back(priority);
				}
				return;
			}
		}

		// Add a new QueueFamilyInfo if one with the specified family index was not found
		const auto properties = physical_device.get().getQueueFamilyProperties();
		auto& family_info = queue_family_info_list.emplace_back(family_idx, properties[family_idx].queueFlags);

		for (uint32_t i = 0; i < count; ++i) {
			family_info.queues.emplace_back(priority);
		}
	}

	std::reference_wrapper<vk::raii::PhysicalDevice> physical_device;
	vk::PhysicalDeviceFeatures features;
	std::vector<const char*> extensions;
	std::vector<queue_family_info> queue_family_info_list;
};


[[nodiscard]]
auto process_config(const logical_device_info& info) -> logical_device_info {
	auto output = info;

	output.extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	output.extensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME); //dynamic rendering required by render_pass_single

	output.features.samplerAnisotropy = VK_TRUE;

	return output;
}

[[nodiscard]]
auto create_device(const logical_device_info& info) -> vk::raii::Device {
	// Validate the queues and extensions
	debug::validate_queues(info.queue_family_info_list, info.physical_device.get().getQueueFamilyProperties());
	debug::validate_extensions(info.extensions, info.physical_device.get().enumerateDeviceExtensionProperties());

	// Build the queue create info list
	auto queue_create_info_list = std::vector<vk::DeviceQueueCreateInfo>{};
	queue_create_info_list.reserve(info.queue_family_info_list.size());

	auto priorities = std::vector<std::vector<float>>{};
	priorities.reserve(info.queue_family_info_list.size());

	for (const auto& family : info.queue_family_info_list) {
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
	const auto device_create_info = vk::StructureChain{
		vk::DeviceCreateInfo{
			vk::DeviceCreateFlags{},
			queue_create_info_list,
			nullptr,
			info.extensions,
			&info.features
		},
		vk::PhysicalDeviceDynamicRenderingFeaturesKHR{
			VK_TRUE
		}
	};

	return vk::raii::Device{info.physical_device, device_create_info.get<vk::DeviceCreateInfo>()};
}


export class logical_device {
public:
	logical_device(const logical_device_info& info) : device_info(process_config(info)), device(create_device(device_info)) {
		// First queue pass: map queues to their exact queue flags.
		// E.g. If a queue is specified which suports only Compute, then map that as the first
		// entry for the Compute flag. This ensures the best match for the requested queue type is
		// the first entry in the list.
		for (const auto& family : device_info.queue_family_info_list) {
			const auto flags = family.flags;
			const auto flag_mask = static_cast<vk::QueueFlags::MaskType>(flags);

			for (auto queue_idx : std::views::iota(uint32_t{0}, family.queues.size())) {
				auto& new_queue = queues.emplace_back(device, family.family_idx, queue_idx);
				queue_map[flag_mask].push_back(std::ref(new_queue));
			}
		}

		// Second queue pass: map every combination of each queue's flags.
		// If only a Graphics|Compute|Transfer queue was requested, and later the user asks for a
		// Graphics|Transfer queue, then this pass will ensure that this queue was mapped to the
		// Graphics|Transfer flag as well.
		for (const auto& family : device_info.queue_family_info_list) {
			const auto flags = family.flags;
			const auto separated_flags = util::separate_flags(flags);
			assert(!separated_flags.empty());

			// Map every combination of the flags, other than the full combination, since that
			// mapping already exists.
			for (size_t permutation = (1ull << separated_flags.size()) - 2; permutation != 0; --permutation) {
				auto mask = vk::QueueFlags::MaskType{0};

				for (auto idx : std::views::iota(size_t{0}, separated_flags.size())) {
					if (permutation & (size_t{1} << idx)) {
						mask |= static_cast<vk::QueueFlags::MaskType>(separated_flags[idx]);
					}
				}

				for (auto queue_idx : std::views::iota(uint32_t{0}, family.queues.size())) {
					auto& queue = get_queue(flags, queue_idx);
					queue_map[mask].push_back(std::ref(queue));
				}
			}
		}
	}

	[[nodiscard]]
	auto get_device_info() const -> const logical_device_info& {
		return device_info;
	}

	[[nodiscard]]
	auto get_vk_physical_device() const -> const vk::raii::PhysicalDevice& {
		return device_info.physical_device;
	}

	[[nodiscard]]
	auto get_vk_device() const -> const vk::raii::Device& {
		return device;
	}

	[[nodiscard]]
	auto get_queue(vk::QueueFlags flag, uint32_t queue_idx) const -> const queue& {
		return queue_map[static_cast<vk::QueueFlags::MaskType>(flag)].at(queue_idx);
	}

	[[nodiscard]]
	auto get_queues(vk::QueueFlags flag) const -> const std::vector<std::reference_wrapper<const queue>>& {
		return queue_map[static_cast<vk::QueueFlags::MaskType>(flag)];
	}

	[[nodiscard]]
	auto get_present_queue(const vk::SurfaceKHR& surface) const -> const queue* {
		for (const auto& q : queues) {
			if (device_info.physical_device.get().getSurfaceSupportKHR(q.family_index, surface)) {
				return &q;
			}
		}
		return nullptr;
	}

	[[nodiscard]]
	auto get_present_queues(const vk::SurfaceKHR& surface) const -> std::vector<std::reference_wrapper<const queue>> {
		auto results = std::vector<std::reference_wrapper<const queue>>{};

		for (auto& q : queues) {
			if (device_info.physical_device.get().getSurfaceSupportKHR(q.family_index, surface)) {
				results.push_back(std::ref(q));
			}
		}

		return results;
	}

	[[nodiscard]]
	auto create_device_memory(const vk::MemoryRequirements& memory_requirements, vk::MemoryPropertyFlags property_flags) const -> vk::raii::DeviceMemory {
		const auto memory_properties    = get_vk_physical_device().getMemoryProperties();
		const auto memory_type_index    = util::find_memory_type(memory_properties, memory_requirements.memoryTypeBits, property_flags);
		const auto memory_allocate_info = vk::MemoryAllocateInfo{memory_requirements.size, memory_type_index};

		return vk::raii::DeviceMemory{get_vk_device(), memory_allocate_info};
	}

private:

	logical_device_info device_info;
	vk::raii::Device device;

	std::vector<queue> queues;

	// Mappings to every queue from each combination of their flags. E.g. a Graphics/Compute/Transfer queue will be
	// mapped to each combination of those 3 types. std::vector doesn't invalidate pointers on move, so storing
	// references to the queues will be fine if an instance of this class is moved.
	mutable std::unordered_map<vk::QueueFlags::MaskType, std::vector<std::reference_wrapper<const queue>>> queue_map;
};

} //namespace vkw

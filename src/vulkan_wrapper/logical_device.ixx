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
import vkw.window;


namespace vkw {
export class logical_device_info {
public:
	logical_device_info(std::shared_ptr<vk::raii::PhysicalDevice> device) : physical_device(std::move(device)) {
	}

	[[nodiscard]]
	auto get_physical_device() const noexcept -> const std::shared_ptr<vk::raii::PhysicalDevice>& {
		return physical_device;
	}

	[[nodiscard]]
	auto get_features() noexcept -> vk::PhysicalDeviceFeatures& {
		return features;
	}

	[[nodiscard]]
	auto get_features() const noexcept -> const vk::PhysicalDeviceFeatures& {
		return features;
	}

	auto set_features(const vk::PhysicalDeviceFeatures& new_features) noexcept -> void {
		features = new_features;
	}

	[[nodiscard]]
	auto get_extensions() const noexcept -> const std::vector<const char*>& {
		return extensions;
	}

	[[nodiscard]]
	auto has_extension(const char* ext) const -> bool {
		const auto it = std::ranges::find_if(extensions, [ext](const char* e) { return strcmp(e, ext) == 0; });
		return it != extensions.end();
	}

	auto add_extension(const char* ext) -> void {
		if (not has_extension(ext)) {
			extensions.push_back(ext);
		}
	}

	[[nodiscard]]
	auto get_queue_family_info_list() const noexcept -> const std::vector<queue_family_info>& {
		return queue_family_info_list;
	}

	auto add_all_queues(float priority) -> void {
		queue_family_info_list.clear();
		const auto properties = physical_device->getQueueFamilyProperties();

		for (auto family_idx : std::views::iota(size_t{0}, properties.size())) {
			const auto& prop = properties[family_idx];
			add_queues(static_cast<uint32_t>(family_idx), priority, prop.queueCount);
		}
	}

	auto add_queues(vk::QueueFlags flags, float priority, uint32_t count = 1) -> std::optional<uint32_t> {
		const auto properties = physical_device->getQueueFamilyProperties();

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

		return std::nullopt;
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

		// Add a new queue_family_info if one with the specified family index was not found
		const auto properties = physical_device->getQueueFamilyProperties();
		auto& family_info = queue_family_info_list.emplace_back(family_idx, properties[family_idx].queueFlags);

		for (uint32_t i = 0; i < count; ++i) {
			family_info.queues.emplace_back(priority);
		}
	}

	// Returns the indices of any queue families added to this device info which support presenting to the specified surface
	[[nodiscard]]
	auto get_present_queue_families(const window& win) const -> std::vector<uint32_t> {
		auto result = std::vector<uint32_t>{};

		for (const auto& family_info : queue_family_info_list) {
			if (physical_device->getSurfaceSupportKHR(family_info.family_idx, *win.get_vk_handle())) {
				result.push_back(family_info.family_idx);
			}
		}

		return result;
	}

private:
	std::shared_ptr<vk::raii::PhysicalDevice> physical_device;
	vk::PhysicalDeviceFeatures features;
	std::vector<const char*> extensions;
	std::vector<queue_family_info> queue_family_info_list;
};


[[nodiscard]]
auto process_config(const logical_device_info& info) -> logical_device_info {
	auto output = info;

	output.add_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	output.add_extension(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME); //dynamic rendering required by render_pass_single

	output.get_features().samplerAnisotropy = VK_TRUE;

	return output;
}

[[nodiscard]]
auto create_device(const logical_device_info& info) -> vk::raii::Device {
	assert(info.get_physical_device() != nullptr);

	// Validate the queues and extensions
	debug::validate_queues(info.get_queue_family_info_list(), info.get_physical_device()->getQueueFamilyProperties());
	debug::validate_extensions(info.get_extensions(), info.get_physical_device()->enumerateDeviceExtensionProperties());

	// Build the queue create info list
	auto queue_create_info_list = std::vector<vk::DeviceQueueCreateInfo>{};
	queue_create_info_list.reserve(info.get_queue_family_info_list().size());

	auto priorities = std::vector<std::vector<float>>{};
	priorities.reserve(info.get_queue_family_info_list().size());

	for (const auto& family : info.get_queue_family_info_list()) {
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
			info.get_extensions(),
			&info.get_features()
		},
		vk::PhysicalDeviceDynamicRenderingFeaturesKHR{
			VK_TRUE
		}
	};

	return vk::raii::Device{*info.get_physical_device(), device_create_info.get<vk::DeviceCreateInfo>()};
}


export class logical_device : public std::enable_shared_from_this<logical_device> {
public:
	[[nodiscard]]
	static auto create(const logical_device_info& info) -> std::shared_ptr<logical_device> {
		return std::make_shared<logical_device>(info);
	}

	logical_device(const logical_device_info& info) : device_info(process_config(info)), device(create_device(device_info)) {
		// First queue pass: map queues to their exact queue flags.
		// E.g. If a queue is specified which suports only Compute, then map that as the first
		// entry for the Compute flag. This ensures the best match for the requested queue type is
		// the first entry in the list.
		for (const auto& family : device_info.get_queue_family_info_list()) {
			const auto flags = family.flags;
			const auto flag_mask = static_cast<vk::QueueFlags::MaskType>(flags);

			for (auto queue_idx : std::views::iota(uint32_t{0}, family.queues.size())) {
				auto& new_queue = queues.emplace_back(device, family.family_idx, queue_idx);
				queue_map[static_cast<vk::QueueFlags>(flag_mask)].push_back(std::ref(new_queue));
			}
		}

		// Second queue pass: map every combination of each queue's flags.
		// If only a Graphics|Compute|Transfer queue was requested, and later the user asks for a
		// Graphics|Transfer queue, then this pass will ensure that this queue was mapped to the
		// Graphics|Transfer flag as well.
		for (const auto& family : device_info.get_queue_family_info_list()) {
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
					queue_map[static_cast<vk::QueueFlags>(mask)].push_back(queue_map[flags].at(queue_idx));
				}
			}
		}
	}

	[[nodiscard]]
	auto get_device_info() const -> const logical_device_info& {
		return device_info;
	}

	[[nodiscard]]
	auto get_physical_device() const -> const std::shared_ptr<vk::raii::PhysicalDevice>& {
		return device_info.get_physical_device();
	}

	[[nodiscard]]
	auto get_vk_handle() const -> const vk::raii::Device& {
		return device;
	}

	[[nodiscard]]
	auto get_queue(vk::QueueFlags flag, uint32_t queue_idx) const -> std::shared_ptr<queue> {
		return std::shared_ptr<queue>{shared_from_this(), &queue_map[flag].at(queue_idx).get()};
	}

	[[nodiscard]]
	auto get_queues(vk::QueueFlags flag) const -> std::vector<std::shared_ptr<queue>> {
		return ranges::to<std::vector>(std::views::transform(queue_map[flag], [this](queue& q) {
			return std::shared_ptr<queue>{shared_from_this(), &q};
		}));
	}

	[[nodiscard]]
	auto get_present_queue(const window& win) const -> std::shared_ptr<queue> {
		for (auto& q : queues) {
			if (get_physical_device()->getSurfaceSupportKHR(q.family_index, *win.get_vk_handle())) {
				return std::shared_ptr<queue>{shared_from_this(), &q};
			}
		}
		return nullptr;
	}

	[[nodiscard]]
	auto get_present_queues(const window& win) const -> std::vector<std::shared_ptr<queue>> {
		auto results = std::vector<std::shared_ptr<queue>>{};

		for (auto& q : queues) {
			if (get_physical_device()->getSurfaceSupportKHR(q.family_index, *win.get_vk_handle())) {
				results.emplace_back(shared_from_this(), &q);
			}
		}

		return results;
	}

	[[nodiscard]]
	auto create_device_memory(const vk::MemoryRequirements& memory_requirements, vk::MemoryPropertyFlags property_flags) const -> vk::raii::DeviceMemory {
		const auto memory_properties    = get_physical_device()->getMemoryProperties();
		const auto memory_type_index    = util::find_memory_type(memory_properties, memory_requirements.memoryTypeBits, property_flags);
		const auto memory_allocate_info = vk::MemoryAllocateInfo{memory_requirements.size, memory_type_index};

		return vk::raii::DeviceMemory{get_vk_handle(), memory_allocate_info};
	}

private:

	logical_device_info device_info;
	vk::raii::Device device;

	mutable std::vector<queue> queues;

	// Mappings to every queue from each combination of their flags. E.g. a Graphics/Compute/Transfer queue will be
	// mapped to each combination of those 3 types. std::vector doesn't invalidate pointers on move, so storing
	// pointers to the queues will be fine if an instance of this class is moved.
	mutable std::unordered_map<vk::QueueFlags, std::vector<std::reference_wrapper<queue>>> queue_map;
};

} //namespace vkw

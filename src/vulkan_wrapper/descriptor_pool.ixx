module;

#include <numeric>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module vkw.descriptor_pool;

import vkw.descriptor_set;
import vkw.logical_device;
import vkw.util;


export namespace vkw {

class descriptor_set_layout {
public:
	descriptor_set_layout(
		const logical_device& device,
		std::span<const vk::DescriptorSetLayoutBinding> layout_bindings,
		vk::DescriptorSetLayoutCreateFlags flags = {}
	) :
		layout(create_layout(device, layout_bindings, flags)),
		bindings(layout_bindings.begin(), layout_bindings.end()) {
	}

	[[nodiscard]]
	auto get_vk_layout() const noexcept -> const vk::raii::DescriptorSetLayout& {
		return layout;
	}

	[[nodiscard]]
	auto get_bindings() const noexcept -> const std::vector<vk::DescriptorSetLayoutBinding>& {
		return bindings;
	}

private:

	[[nodiscard]]
	static auto create_layout(
		const logical_device& device,
		std::span<const vk::DescriptorSetLayoutBinding> bindings,
		vk::DescriptorSetLayoutCreateFlags flags = {}
	) -> vk::raii::DescriptorSetLayout {

		const auto create_info = vk::DescriptorSetLayoutCreateInfo{flags, bindings};
		return vk::raii::DescriptorSetLayout{device.get_vk_device(), create_info};
	}

	vk::raii::DescriptorSetLayout layout;
	std::vector<vk::DescriptorSetLayoutBinding> bindings;
};


class descriptor_pool {
public:
	descriptor_pool(
		const logical_device& device,
		std::span<const vk::DescriptorPoolSize> pool_sizes,
		uint32_t max_sets,
		vk::DescriptorPoolCreateFlags flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet
	) :
		device(device),
		sizes(pool_sizes.begin(), pool_sizes.end()),
		max(max_sets),
		pool(make_descriptor_pool(device, pool_sizes, max_sets, flags)) {
	}
	
	descriptor_pool(
		const logical_device& device,
		std::span<const vk::DescriptorPoolSize> pool_sizes,
		vk::DescriptorPoolCreateFlags flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet
	) :
		descriptor_pool(
			device,
			pool_sizes,
			std::reduce(pool_sizes.begin(), pool_sizes.end(), uint32_t{0}, [](uint32_t sum, const auto& size) { return sum + size.descriptorCount; }),
			flags
		) {
	}

	[[nodiscard]]
	auto get_pool() const noexcept -> const vk::raii::DescriptorPool& {
		return pool;
	}

	[[nodiscard]]
	auto get_sizes() const noexcept -> const std::vector<vk::DescriptorPoolSize>& {
		return sizes;
	}

	[[nodiscard]]
	auto get_max() const noexcept -> uint32_t {
		return max;
	}

	[[nodiscard]]
	auto allocate(const descriptor_set_layout& layout) -> descriptor_set {
		const auto allocate_info = vk::DescriptorSetAllocateInfo{*pool, *layout.get_vk_layout()};
		auto descriptor = std::move(vk::raii::DescriptorSets{device.get().get_vk_device(), allocate_info}.front());
		return descriptor_set{std::move(descriptor), layout.get_bindings()};
	}

	[[nodiscard]]
	auto allocate(std::span<const descriptor_set_layout> layouts) -> std::vector<descriptor_set> {
		const auto vk_layouts = vkw::util::to_vector(vkw::util::as_handles(std::views::transform(layouts, &descriptor_set_layout::get_vk_layout)));

		const auto allocate_info = vk::DescriptorSetAllocateInfo{*pool, vk_layouts};
		auto sets = vk::raii::DescriptorSets{device.get().get_vk_device(), allocate_info};
		
		auto result = std::vector<descriptor_set>{};
		result.reserve(sets.size());
		for (auto i : std::views::iota(size_t{0}, layouts.size())) {
			result.emplace_back(std::move(sets[i]), layouts[i].get_bindings());
		}

		return result;
	}

private:

	[[nodiscard]]
	static auto make_descriptor_pool(
		const logical_device& device,
		std::span<const vk::DescriptorPoolSize> pool_sizes,
		uint32_t max_sets,
		vk::DescriptorPoolCreateFlags flags
	) -> vk::raii::DescriptorPool {

		const auto pool_create_info = vk::DescriptorPoolCreateInfo{flags, max_sets, pool_sizes};
		return vk::raii::DescriptorPool(device.get_vk_device(), pool_create_info);
	}


	std::reference_wrapper<const logical_device> device;
	
	std::vector<vk::DescriptorPoolSize> sizes;
	uint32_t max = 0;

	vk::raii::DescriptorPool pool;
};

} //namespace vkw

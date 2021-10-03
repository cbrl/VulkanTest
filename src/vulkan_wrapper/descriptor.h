#pragma once

#include <utility>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

#include "logical_device.h"


namespace vkw {
	
class descriptor_set_layout {
public:
	descriptor_set_layout(
		const logical_device& device,
		const std::vector<vk::DescriptorSetLayoutBinding>& layout_bindings,
		vk::DescriptorSetLayoutCreateFlags flags = {}
	) :
		layout(create_layout(device, layout_bindings, flags)),
		bindings(layout_bindings) {
	}

	[[nodiscard]]
	auto get_vk_layout() noexcept -> vk::raii::DescriptorSetLayout& {
		return layout;
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
	static auto create_layout(
		const logical_device& device,
		const std::vector<vk::DescriptorSetLayoutBinding>& bindings,
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
		const std::vector<vk::DescriptorPoolSize>& pool_sizes,
		uint32_t max_sets,
		vk::DescriptorPoolCreateFlags flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet
	) :
		device(device),
		pool(make_descriptor_pool(device, pool_sizes, max_sets, flags)),
		sizes(pool_sizes),
		max(max_sets) {
	}

	[[nodiscard]]
	auto get_pool() noexcept -> vk::raii::DescriptorPool& {
		return pool;
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
	auto allocate(const descriptor_set_layout& layout) -> vk::raii::DescriptorSet {
		const auto allocate_info = vk::DescriptorSetAllocateInfo{*pool, *layout.get_vk_layout()};
		return std::move(vk::raii::DescriptorSets{device.get().get_vk_device(), allocate_info}.front());
	}

	[[nodiscard]]
	auto allocate(const std::vector<descriptor_set_layout>& layouts) -> std::vector<vk::raii::DescriptorSet> {
		auto vk_layouts = std::vector<vk::DescriptorSetLayout>{};
		vk_layouts.reserve(layouts.size());

		for (const auto& layout : layouts) {
			vk_layouts.push_back(*layout.get_vk_layout());
		}

		const auto allocate_info = vk::DescriptorSetAllocateInfo{*pool, vk_layouts};
		return vk::raii::DescriptorSets{device.get().get_vk_device(), allocate_info};
	}

private:

	static auto make_descriptor_pool(
		const logical_device& device,
		const std::vector<vk::DescriptorPoolSize>& pool_sizes,
		uint32_t max_sets,
		vk::DescriptorPoolCreateFlags flags
	) -> vk::raii::DescriptorPool {

		const auto pool_create_info = vk::DescriptorPoolCreateInfo{flags, max_sets, pool_sizes};
		return vk::raii::DescriptorPool(device.get_vk_device(), pool_create_info);
	}

	std::reference_wrapper<const logical_device> device;
	vk::raii::DescriptorPool pool;
	std::vector<vk::DescriptorPoolSize> sizes;
	uint32_t max = 0;
};

} //namespace vkw
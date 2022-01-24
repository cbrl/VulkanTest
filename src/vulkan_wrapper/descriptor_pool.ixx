module;

#include <memory>
#include <numeric>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

#include "descriptor_pool_fwd.h"

export module vkw.descriptor_pool;

import vkw.descriptor_set;
import vkw.logical_device;
import vkw.util;


export namespace vkw {

class descriptor_pool : public std::enable_shared_from_this<descriptor_pool> {
public:
	[[nodiscard]]
	static auto create(
		std::shared_ptr<logical_device> device,
		std::span<const vk::DescriptorPoolSize> pool_sizes,
		uint32_t max_sets,
		vk::DescriptorPoolCreateFlags flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet
	) -> std::shared_ptr<descriptor_pool> {
		return std::make_shared<descriptor_pool>(std::move(device), pool_sizes, max_sets, flags);
	}

	[[nodiscard]]
	static auto create(
		std::shared_ptr<logical_device> device,
		std::span<const vk::DescriptorPoolSize> pool_sizes,
		vk::DescriptorPoolCreateFlags flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet
	) -> std::shared_ptr<descriptor_pool> {
		return std::make_shared<descriptor_pool>(std::move(device), pool_sizes, flags);
	}

	descriptor_pool(
		std::shared_ptr<logical_device> logic_device,
		std::span<const vk::DescriptorPoolSize> pool_sizes,
		uint32_t max_sets,
		vk::DescriptorPoolCreateFlags flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet
	) :
		device(std::move(logic_device)),
		sizes(pool_sizes.begin(), pool_sizes.end()),
		max(max_sets),
		pool(nullptr) {

		const auto pool_create_info = vk::DescriptorPoolCreateInfo{flags, max_sets, pool_sizes};
		pool = vk::raii::DescriptorPool{device->get_vk_handle(), pool_create_info};
	}
	
	descriptor_pool(
		std::shared_ptr<logical_device> logic_device,
		std::span<const vk::DescriptorPoolSize> pool_sizes,
		vk::DescriptorPoolCreateFlags flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet
	) :
		descriptor_pool(
			std::move(logic_device),
			pool_sizes,
			std::reduce(pool_sizes.begin(), pool_sizes.end(), uint32_t{0}, [](uint32_t sum, const auto& size) { return sum + size.descriptorCount; }),
			flags
		) {
	}

	[[nodiscard]]
	auto get_vk_handle() const noexcept -> const vk::raii::DescriptorPool& {
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
	auto allocate(std::shared_ptr<descriptor_set_layout> layout) -> std::shared_ptr<descriptor_set> {
		const auto allocate_info = vk::DescriptorSetAllocateInfo{*pool, *layout->get_vk_handle()};
		auto descriptor = std::move(vk::raii::DescriptorSets{device->get_vk_handle(), allocate_info}.front());
		return descriptor_set::create(device, shared_from_this(), std::move(layout), std::move(descriptor));
	}

	[[nodiscard]]
	auto allocate(std::span<const std::shared_ptr<descriptor_set_layout>> layouts) -> std::vector<std::shared_ptr<descriptor_set>> {
		const auto vk_layouts = vkw::ranges::to<std::vector>(layouts | util::as_handles());

		const auto allocate_info = vk::DescriptorSetAllocateInfo{*pool, vk_layouts};
		auto sets = vk::raii::DescriptorSets{device->get_vk_handle(), allocate_info};
		
		auto result = std::vector<std::shared_ptr<descriptor_set>>{};
		result.reserve(sets.size());

		for (auto i : std::views::iota(size_t{0}, layouts.size())) {
			result.push_back(descriptor_set::create(device, shared_from_this(), layouts[i], std::move(sets[i])));
		}

		return result;
	}

private:

	std::shared_ptr<logical_device> device;
	
	std::vector<vk::DescriptorPoolSize> sizes;
	uint32_t max = 0;

	vk::raii::DescriptorPool pool;
};

} //namespace vkw

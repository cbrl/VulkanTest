#pragma once

#include <array>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

#include "logical_device.h"
#include "descriptor.h"
#include "vulkan/vulkan_enums.hpp"


namespace vkw {

class pipeline_layout {
public:
	pipeline_layout(
		const logical_device& device,
		std::span<const descriptor_set_layout> layouts,
		std::span<vk::PushConstantRange> ranges
	) :
		layout(make_layout(device, layouts, ranges)),
		descriptor_layouts(layouts.begin(), layouts.end()),
		push_constant_ranges(ranges.begin(), ranges.end()) {
	}

	[[nodiscard]]
	auto get_vk_layout() const -> const vk::raii::PipelineLayout& {
		return layout;
	}

	[[nodiscard]]
	auto get_descriptors() const noexcept -> std::span<std::reference_wrapper<const descriptor_set_layout>> {
		return descriptor_layouts;
	}

	[[nodiscard]]
	auto get_push_constant_ranges() const noexcept -> std::span<const vk::PushConstantRange> {
		return push_constant_ranges;
	}

	auto bind_descriptor_sets(
		vk::raii::CommandBuffer& cmd_buffer,
		vk::PipelineBindPoint bind_point,
		const vk::DescriptorSet& descriptor_set,
		uint32_t first_set,
		const std::vector<uint32_t>& offsets
	) -> void {
		const auto sets = std::array{descriptor_set};
		bind_descriptor_sets(cmd_buffer, bind_point, sets, first_set, offsets);
	}

	auto bind_descriptor_sets(
		vk::raii::CommandBuffer& cmd_buffer,
		vk::PipelineBindPoint bind_point,
		std::span<const vk::raii::DescriptorSet> descriptor_sets,
		uint32_t first_set,
		const std::vector<uint32_t>& offsets
	) -> void {
		const auto sets = vkw::util::to_vector(vkw::util::as_handles(descriptor_sets));
		cmd_buffer.bindDescriptorSets(bind_point, *layout, sets, first_set, descriptor_sets, offsets)
	}

	auto bind_descriptor_sets(
		vk::raii::CommandBuffer& cmd_buffer,
		vk::PipelineBindPoint bind_point,
		std::span<const vk::DescriptorSet> descriptor_sets,
		uint32_t first_set,
		const std::vector<uint32_t>& offsets
	) -> void {
		cmd_buffer.bindDescriptorSets(bind_point, *layout, descriptor_sets, first_set, descriptor_sets, offsets)
	}

private:

	[[nodiscard]]
	static auto make_layout(
		const logical_device& device,
		std::span<const descriptor_set_layout> descriptor_layouts,
		std::span<const vk::PushConstantRange> push_constant_ranges
	) -> vk::raii::PipelineLayout {

		auto vk_layouts = std::vector<vk::DescriptorSetLayout>{};
		vk_layouts.reserve(descriptor_layouts.size());

		for (const auto& layout : descriptor_layouts) {
			vk_layouts.push_back(*layout.get().get_vk_layout());
		}

		const auto layout_create_info = vk::PipelineLayoutCreateInfo{
			vk::PipelineLayoutCreateFlags{},
			vk_layouts,
			push_constant_ranges
		};

		return vk::raii::PipelineLayout{device.get_vk_device(), layout_create_info};
	}

	vk::raii::PipelineLayout layout;
	std::vector<std::reference_wrapper<const descriptor_set_layout>> descriptor_layouts;
	std::vector<vk::PushConstantRange> push_constant_ranges;
};

} //namespace vkw

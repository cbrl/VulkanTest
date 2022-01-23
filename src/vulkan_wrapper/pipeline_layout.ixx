module;

#include <memory>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module vkw.pipeline_layout;

import vkw.logical_device;
import vkw.descriptor_pool;
import vkw.descriptor_set;
import vkw.util;


namespace vkw {

export class pipeline_layout {
public:
	[[nodiscard]]
	static auto create(
		std::shared_ptr<logical_device> device,
		std::span<const std::shared_ptr<descriptor_set_layout>> layouts,
		std::span<const vk::PushConstantRange> ranges
	) -> std::shared_ptr<pipeline_layout> {
		return std::make_shared<pipeline_layout>(std::move(device), layouts, ranges);
	}

	pipeline_layout(
		std::shared_ptr<logical_device> logic_device,
		std::span<const std::shared_ptr<descriptor_set_layout>> layouts,
		std::span<const vk::PushConstantRange> ranges
	) :
		device(std::move(logic_device)),
		descriptor_layouts(layouts.begin(), layouts.end()),
		push_constant_ranges(ranges.begin(), ranges.end()),
		layout(nullptr) {

		const auto vk_layouts = ranges::to<std::vector>(descriptor_layouts | std::views::transform([](auto&& layout) { return *layout->get_vk_handle(); }));

		const auto layout_create_info = vk::PipelineLayoutCreateInfo{
			vk::PipelineLayoutCreateFlags{},
			vk_layouts,
			push_constant_ranges
		};

		layout = vk::raii::PipelineLayout{device->get_vk_handle(), layout_create_info};
	}

	[[nodiscard]]
	auto get_vk_handle() const -> const vk::raii::PipelineLayout& {
		return layout;
	}

	[[nodiscard]]
	auto get_descriptors() const noexcept -> const std::vector<std::shared_ptr<descriptor_set_layout>>& {
		return descriptor_layouts;
	}

	[[nodiscard]]
	auto get_push_constant_ranges() const noexcept -> const std::vector<vk::PushConstantRange>& {
		return push_constant_ranges;
	}

	auto bind_descriptor_sets(
		const vk::raii::CommandBuffer& cmd_buffer,
		vk::PipelineBindPoint bind_point,
		uint32_t first_set,
		const descriptor_set& descriptor_set,
		const std::vector<uint32_t>& offsets
	) const -> void {
		bind_descriptor_sets(cmd_buffer, bind_point, first_set, std::span{&descriptor_set, 1}, offsets);
	}

	auto bind_descriptor_sets(
		const vk::raii::CommandBuffer& cmd_buffer,
		vk::PipelineBindPoint bind_point,
		uint32_t first_set,
		std::span<const descriptor_set> descriptor_sets,
		const std::vector<uint32_t>& offsets
	) const -> void {
		const auto sets = ranges::to<std::vector>(descriptor_sets | std::views::transform([](auto&& set) { return *set.get_vk_handle();}));
		cmd_buffer.bindDescriptorSets(bind_point, *get_vk_handle(), first_set, sets, offsets);
	}

private:
	std::shared_ptr<logical_device> device;

	std::vector<std::shared_ptr<descriptor_set_layout>> descriptor_layouts;
	std::vector<vk::PushConstantRange> push_constant_ranges;

	vk::raii::PipelineLayout layout;
};

} //namespace vkw

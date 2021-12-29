module;

#include <span>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module vkw.pipeline_layout;

import vkw.logical_device;
import vkw.descriptor;
import vkw.util;


export namespace vkw {

class pipeline_layout {
public:
	pipeline_layout(
		const logical_device& device,
		std::span<const descriptor_set_layout> layouts,
		std::span<const vk::PushConstantRange> ranges
	) :
		descriptor_layouts(layouts.begin(), layouts.end()),
		push_constant_ranges(ranges.begin(), ranges.end()),
		layout(make_layout(device, layouts, ranges)) {
	}

	[[nodiscard]]
	auto get_vk_layout() const -> const vk::raii::PipelineLayout& {
		return layout;
	}

	[[nodiscard]]
	auto get_descriptors() const noexcept -> std::span<const descriptor_set_layout> {
		return descriptor_layouts;
	}

	[[nodiscard]]
	auto get_push_constant_ranges() const noexcept -> const std::vector<vk::PushConstantRange>& {
		return push_constant_ranges;
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
			vk_layouts.push_back(*layout.get_vk_layout());
		}

		const auto layout_create_info = vk::PipelineLayoutCreateInfo{
			vk::PipelineLayoutCreateFlags{},
			vk_layouts,
			push_constant_ranges
		};

		return vk::raii::PipelineLayout{device.get_vk_device(), layout_create_info};
	}

	std::span<const descriptor_set_layout> descriptor_layouts;
	std::vector<vk::PushConstantRange> push_constant_ranges;

	vk::raii::PipelineLayout layout;
};

} //namespace vkw

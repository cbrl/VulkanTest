module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module vkw.pipeline;

import vkw.descriptor;
import vkw.logical_device;
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

	auto bind_descriptor_sets(
		vk::raii::CommandBuffer& cmd_buffer,
		vk::PipelineBindPoint bind_point,
		vk::DescriptorSet descriptor_set,
		uint32_t first_set,
		const std::vector<uint32_t>& offsets
	) -> void {
		bind_descriptor_sets(cmd_buffer, bind_point, first_set, std::span{&descriptor_set, 1}, offsets);
	}

	auto bind_descriptor_sets(
		vk::raii::CommandBuffer& cmd_buffer,
		vk::PipelineBindPoint bind_point,
		uint32_t first_set,
		std::span<const vk::raii::DescriptorSet> descriptor_sets,
		const std::vector<uint32_t>& offsets
	) -> void {
		const auto sets = vkw::util::to_vector(vkw::util::as_handles(descriptor_sets));
		bind_descriptor_sets(cmd_buffer, bind_point, first_set, sets, offsets);
	}

	auto bind_descriptor_sets(
		vk::raii::CommandBuffer& cmd_buffer,
		vk::PipelineBindPoint bind_point,
		uint32_t first_set,
		std::span<const vk::DescriptorSet> descriptor_sets,
		const std::vector<uint32_t>& offsets
	) -> void {
		cmd_buffer.bindDescriptorSets(bind_point, *layout, first_set, descriptor_sets, offsets);
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


class shader_stage {
public:
	static constexpr const char* entry_point_name = "main";

	shader_stage(
		const logical_device& device,
		vk::ShaderStageFlagBits stage,
		std::span<const vk::SpecializationMapEntry> specializations,
		std::span<const std::byte> shader_data,
		std::span<const std::byte> specialization_data
	) :
		specialization_entries(specializations.begin(), specializations.end()),
		specialization_data(specialization_data.begin(), specialization_data.end()),
		shader_module(create_shader_module(device, shader_data)) {

		create_info.flags               = vk::PipelineShaderStageCreateFlags{};
		create_info.stage               = stage;
		create_info.module              = *shader_module;
		create_info.pName               = entry_point_name;
		create_info.pSpecializationInfo = &specialization_info;

		specialization_info.mapEntryCount = static_cast<uint32_t>(specialization_entries.size());
		specialization_info.pMapEntries   = specialization_entries.data();
		specialization_info.dataSize      = specialization_data.size_bytes();
		specialization_info.pData         = this->specialization_data.data();
	}

	[[nodiscard]]
	auto get_create_info() const noexcept -> const vk::PipelineShaderStageCreateInfo& {
		return create_info;
	}

	[[nodiscard]]
	auto get_specialization_info() const noexcept -> const vk::SpecializationInfo& {
		return specialization_info;
	}

	[[nodiscard]]
	auto get_specialization_entries() const noexcept -> const std::vector<vk::SpecializationMapEntry>& {
		return specialization_entries;
	}

private:

	[[nodiscard]]
	static auto create_shader_module(const logical_device& device, std::span<const std::byte> data) -> vk::raii::ShaderModule {
		const auto create_info = vk::ShaderModuleCreateInfo{
			vk::ShaderModuleCreateFlags{},
			data.size_bytes(),
			reinterpret_cast<const uint32_t*>(data.data())
		};

		return vk::raii::ShaderModule{device.get_vk_device(), create_info};
	}

	vk::PipelineShaderStageCreateInfo create_info;
	vk::SpecializationInfo specialization_info;
	std::vector<vk::SpecializationMapEntry> specialization_entries;
	std::vector<std::byte> specialization_data;

	vk::raii::ShaderModule shader_module;
};


class pipeline {
public:
	explicit pipeline(const logical_device& device /*, const vk::raii::PipelineCache& cache*/);

};

} //namespace vkw

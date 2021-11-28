module;

#include <array>
#include <cstddef>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module vkw.pipeline;

import vkw.descriptor;
import vkw.logical_device;
import vkw.render_pass;
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
		std::span<const uint32_t> shader_data,
		std::span<const vk::SpecializationMapEntry> specializations = {},
		std::span<const uint32_t> specialization_data = {}
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
	static auto create_shader_module(const logical_device& device, std::span<const uint32_t> data) -> vk::raii::ShaderModule {
		const auto create_info = vk::ShaderModuleCreateInfo{
			vk::ShaderModuleCreateFlags{},
			data.size_bytes(),
			data.data()
		};

		return vk::raii::ShaderModule{device.get_vk_device(), create_info};
	}

	vk::PipelineShaderStageCreateInfo create_info;
	vk::SpecializationInfo specialization_info;
	std::vector<vk::SpecializationMapEntry> specialization_entries;
	std::vector<uint32_t> specialization_data;

	vk::raii::ShaderModule shader_module;
};


class graphics_pipeline {
public:
	struct pipeline_info {
		pipeline_info() {
			input_assembly_state.topology = vk::PrimitiveTopology::eTriangleList;

			viewport_state.viewportCount = 1;
			viewport_state.scissorCount  = 1;

			raster_state.lineWidth = 1.0f;

			add_dynamic_state(vk::DynamicState::eViewport);
			add_dynamic_state(vk::DynamicState::eScissor);
		}

		// This custom copy constructor maintains the correct data pointers in the state create info structs
		pipeline_info(const pipeline_info& other) {
			vertex_input_state   = other.vertex_input_state;
			input_assembly_state = other.input_assembly_state;
			tessellation_state   = other.tessellation_state;
			viewport_state       = other.viewport_state;
			raster_state         = other.raster_state;
			multistample_state   = other.multistample_state;
			depth_stencil_state  = other.depth_stencil_state;
			color_blend_state    = other.color_blend_state;
			dynamic_state        = other.dynamic_state;

			shader_stages = other.shader_stages;

			layout = other.layout;
			
			pass    = other.pass;
			subpass = other.subpass;

			vertex_input_bindings   = other.vertex_input_bindings;
			vertex_input_attributes = other.vertex_input_attributes;
			color_blend_attachments = other.color_blend_attachments;
			dynamic_states          = other.dynamic_states;

			vertex_input_state.setVertexBindingDescriptions(vertex_input_bindings);
			vertex_input_state.setVertexAttributeDescriptions(vertex_input_attributes);
			color_blend_state.setAttachments(color_blend_attachments);
			dynamic_state.setDynamicStates(dynamic_states);
		}

		pipeline_info(pipeline_info&&) noexcept = default;

		~pipeline_info() = default;

		pipeline_info& operator=(const pipeline_info& other) {
			// No extra logic needs to happen for this operation in comparison to the copy constructor,
			// so it can be done in terms of the copy constructor.
			new(this) pipeline_info(other);
			return *this;
		}

		pipeline_info& operator=(pipeline_info&&) noexcept = default;

		[[nodiscard]]
		auto get_vertex_input_bindings() const noexcept -> const std::vector<vk::VertexInputBindingDescription>& {
			return vertex_input_bindings;
		}

		[[nodiscard]]
		auto get_vertex_input_attributes() const noexcept -> const std::vector<vk::VertexInputAttributeDescription>& {
			return vertex_input_attributes;
		}

		[[nodiscard]]
		auto get_color_blend_attachments() const noexcept -> const std::vector<vk::PipelineColorBlendAttachmentState>& {
			return color_blend_attachments;
		}

		[[nodiscard]]
		auto get_dynamic_states() const noexcept -> const std::vector<vk::DynamicState>& {
			return dynamic_states;
		}

		auto add_vertex_input_binding(const vk::VertexInputBindingDescription& binding) -> void {
			vertex_input_bindings.push_back(binding);
			vertex_input_state.setVertexBindingDescriptions(vertex_input_bindings);
		}

		auto add_vertex_input_attribute(const vk::VertexInputAttributeDescription& attribute) -> void {
			vertex_input_attributes.push_back(attribute);
			vertex_input_state.setVertexAttributeDescriptions(vertex_input_attributes);
		}

		auto add_color_blend_attachment(const vk::PipelineColorBlendAttachmentState& attachment) -> void {
			color_blend_attachments.push_back(attachment);
			color_blend_state.setAttachments(color_blend_attachments);
		}

		auto add_dynamic_state(vk::DynamicState state) -> void {
			dynamic_states.push_back(state);
			dynamic_state.setDynamicStates(dynamic_states);
		}


		vk::PipelineVertexInputStateCreateInfo   vertex_input_state;
		vk::PipelineInputAssemblyStateCreateInfo input_assembly_state;
		vk::PipelineTessellationStateCreateInfo  tessellation_state;
		vk::PipelineViewportStateCreateInfo      viewport_state;
		vk::PipelineRasterizationStateCreateInfo raster_state;
		vk::PipelineMultisampleStateCreateInfo   multistample_state;
		vk::PipelineDepthStencilStateCreateInfo  depth_stencil_state;
		vk::PipelineColorBlendStateCreateInfo    color_blend_state;
		vk::PipelineDynamicStateCreateInfo       dynamic_state;

		std::vector<std::reference_wrapper<const shader_stage>> shader_stages;

		const pipeline_layout* layout;

		const render_pass* pass;
		uint32_t subpass = 0;

	private:

		std::vector<vk::VertexInputBindingDescription>     vertex_input_bindings;
		std::vector<vk::VertexInputAttributeDescription>   vertex_input_attributes;
		std::vector<vk::PipelineColorBlendAttachmentState> color_blend_attachments;
		std::vector<vk::DynamicState>                      dynamic_states;
	};

	graphics_pipeline(
		const logical_device& device,
		const pipeline_info& info,
		vk::raii::PipelineCache* cache = nullptr
	) :
		info(info),
		vk_pipeline(create_pipeline(device, this->info, cache)),
		pipeline_cache(cache) {
	}

private:

	static auto create_pipeline(
		const logical_device& device,
		const pipeline_info& info,
		vk::raii::PipelineCache* cache = nullptr
	) -> vk::raii::Pipeline {

		assert(info.layout);
		assert(info.pass);

		const auto stages = vkw::util::to_vector(std::views::transform(info.shader_stages, &shader_stage::get_create_info));

		const auto pipeline_create_info = vk::GraphicsPipelineCreateInfo{
			vk::PipelineCreateFlags{},
			stages,
			&info.vertex_input_state,
			&info.input_assembly_state,
			&info.tessellation_state,
			&info.viewport_state,
			&info.raster_state,
			&info.multistample_state,
			&info.depth_stencil_state,
			&info.color_blend_state,
			&info.dynamic_state,
			*info.layout->get_vk_layout(),
			*info.pass->get_vk_render_pass(),
			info.subpass,
			vk::Pipeline{},
			-1
		};

	
		return vk::raii::Pipeline{device.get_vk_device(), cache, pipeline_create_info};
	}

	pipeline_info info;

	vk::raii::Pipeline vk_pipeline;
	vk::raii::PipelineCache* pipeline_cache;
	
	vk::Viewport viewport;
	vk::Rect2D   scissor_rect;
};

} //namespace vkw

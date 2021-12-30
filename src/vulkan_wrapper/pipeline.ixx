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
import vkw.pipeline_layout;
import vkw.render_pass;
import vkw.shader;
import vkw.util;


namespace vkw {

// This base class exists so that the copy operators don't need to be completely re-implemented.
// Only the pointers to the bindings/attributes/attachments need to be updated on a copy, so the
// final derived class defines a custom copy operator that takes care of that.
class graphics_pipeline_info_base {
	friend class graphics_pipeline_info;

public:
	graphics_pipeline_info_base() {
		input_assembly_state.topology = vk::PrimitiveTopology::eTriangleList;

		viewport_state.viewportCount = 1;
		viewport_state.scissorCount  = 1;

		raster_state.lineWidth = 1.0f;

		add_dynamic_state(vk::DynamicState::eViewport);
		add_dynamic_state(vk::DynamicState::eScissor);
	}

	graphics_pipeline_info_base(const graphics_pipeline_info_base&) = default;
	graphics_pipeline_info_base(graphics_pipeline_info_base&&) noexcept = default;

	~graphics_pipeline_info_base() = default;

	graphics_pipeline_info_base& operator=(const graphics_pipeline_info_base&) = default;
	graphics_pipeline_info_base& operator=(graphics_pipeline_info_base&&) noexcept = default;

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


	// Pipeline stage creation info
	vk::PipelineVertexInputStateCreateInfo   vertex_input_state;
	vk::PipelineInputAssemblyStateCreateInfo input_assembly_state;
	vk::PipelineTessellationStateCreateInfo  tessellation_state;
	vk::PipelineViewportStateCreateInfo      viewport_state;
	vk::PipelineRasterizationStateCreateInfo raster_state;
	vk::PipelineMultisampleStateCreateInfo   multistample_state;
	vk::PipelineDepthStencilStateCreateInfo  depth_stencil_state;
	vk::PipelineColorBlendStateCreateInfo    color_blend_state;
	vk::PipelineDynamicStateCreateInfo       dynamic_state;

	// The shader stages this pipeline is composed of
	std::vector<std::reference_wrapper<const vkw::shader_stage>> shader_stages;

	// The pipeline layout
	const vkw::pipeline_layout* layout = nullptr;

	// The render pass and subpass
	const vkw::render_pass* pass = nullptr;
	uint32_t subpass = 0;

private:

	std::vector<vk::VertexInputBindingDescription>     vertex_input_bindings;
	std::vector<vk::VertexInputAttributeDescription>   vertex_input_attributes;
	std::vector<vk::PipelineColorBlendAttachmentState> color_blend_attachments;
	std::vector<vk::DynamicState>                      dynamic_states;
};


export class graphics_pipeline_info : public graphics_pipeline_info_base {
public:
	graphics_pipeline_info() = default;

	// This custom copy constructor maintains the correct data pointers in the state create info structs
	graphics_pipeline_info(const graphics_pipeline_info& other) : graphics_pipeline_info_base(other) {
		update_state_pointers();
	}

	// The default move constrctor is fine, because the destination vectors will take ownership of the
	// moved-from vectors' data pointers, instead of copying the data to another array.
	graphics_pipeline_info(graphics_pipeline_info&& other) noexcept = default;

	~graphics_pipeline_info() = default;

	graphics_pipeline_info& operator=(const graphics_pipeline_info& other) {
		graphics_pipeline_info_base::operator=(other);
		update_state_pointers();
		return *this;
	}

	graphics_pipeline_info& operator=(graphics_pipeline_info&& other) noexcept = default;

private:

	auto update_state_pointers() -> void {
		vertex_input_state.setVertexBindingDescriptions(vertex_input_bindings);
		vertex_input_state.setVertexAttributeDescriptions(vertex_input_attributes);
		color_blend_state.setAttachments(color_blend_attachments);
		dynamic_state.setDynamicStates(dynamic_states);
	}
};


export class graphics_pipeline {
public:
	graphics_pipeline(
		const logical_device& device,
		const graphics_pipeline_info& info,
		vk::raii::PipelineCache* cache = nullptr
	) :
		info(info),
		vk_pipeline(create_pipeline(device, this->info, cache)),
		pipeline_cache(cache) {
	}

	[[nodiscard]]
	auto get_pipeline_info() const noexcept -> const graphics_pipeline_info& {
		return info;
	}

	[[nodiscard]]
	auto get_vk_pipeline() const noexcept -> const vk::raii::Pipeline& {
		return vk_pipeline;
	}

	auto bind(const vk::raii::CommandBuffer& cmd_buffer) -> void {
		cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *get_vk_pipeline());
	}

	auto bind_descriptor_sets(
		const vk::raii::CommandBuffer& cmd_buffer,
		uint32_t first_set,
		const descriptor_set& descriptor_set,
		const std::vector<uint32_t>& offsets
	) const -> void {
		bind_descriptor_sets(cmd_buffer, first_set, std::span{&descriptor_set, 1}, offsets);
	}

	auto bind_descriptor_sets(
		const vk::raii::CommandBuffer& cmd_buffer,
		uint32_t first_set,
		std::span<const descriptor_set> descriptor_sets,
		const std::vector<uint32_t>& offsets
	) const -> void {
		info.layout->bind_descriptor_sets(cmd_buffer, vk::PipelineBindPoint::eGraphics, first_set, descriptor_sets, offsets);
	}

private:

	static auto create_pipeline(
		const logical_device& device,
		const graphics_pipeline_info& info,
		vk::raii::PipelineCache* cache = nullptr
	) -> vk::raii::Pipeline {

		assert(info.layout && "graphics_pipeline_info::layout must not be null");
		assert(info.pass && "graphics_pipeline_info::pass must not be null");

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

	graphics_pipeline_info info;

	vk::raii::Pipeline vk_pipeline;
	vk::raii::PipelineCache* pipeline_cache;
	
	//vk::Viewport viewport;
	//vk::Rect2D   scissor_rect;
};

} //namespace vkw

#include <vulkan/vulkan.hpp>
#include "vulkan/vulkan_raii.hpp"
#include <glslang/SPIRV/GlslangToSpv.h>

#include "utils/math.hpp"
#include "utils/raii/shaders.hpp"
#include "utils/raii/utils.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include "geometry.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <memory>
#include <ranges>
#include <string>
#include <thread>
#include <vector>

import vkw;

// TODO:
//   - Use vk::XXX instead of reference_wrapper<vk::raii::XXX> where the raii type is not needed
//   - Use descriptor indexing (core in Vulkan 1.2)
//   - Integrate VMA

auto main(int argc, char** argv) -> int {
	// Instance
	//--------------------------------------------------------------------------------
	const auto app_info = vkw::app_info{};

	const auto instance_info = vkw::instance_info{
		.layers = {},
		.extensions = vkw::util::get_surface_extensions()
	};

	const auto debug_info = vkw::debug_info{
		.utils = true,
		.validation = true
	};

	auto instance = vkw::instance{app_info, instance_info, debug_info};
	auto& physical_device = instance.get_physical_device(0);


	// Window
	//--------------------------------------------------------------------------------
	auto window = vkw::glfw_window{
		instance,
		"Vulkan Window",
		{1280, 1024},
		{{GLFW_RESIZABLE, GLFW_FALSE}}
	};

	window.add_event_handler([](vkw::window& win, vkw::window::event e, uint64_t param, void*) {
		if ((e == vkw::window::event::key_down) && (param == GLFW_KEY_ESCAPE)) {
			win.set_should_close(true);
		}
	});


	// Logical Device
	//--------------------------------------------------------------------------------
	auto device_info = vkw::logical_device_info{
		.physical_device = physical_device,
		.features = {},
		.extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME}
	};

	// Add a graphics queue
	const auto graphics_queue = device_info.add_queues(vk::QueueFlagBits::eGraphics, 1.0f);
	if (not graphics_queue.has_value()) {
		throw std::runtime_error("No queues with graphics support");
	}

	// Check if any existing queues support present	
	const auto present_queue_it = std::ranges::find_if(device_info.queue_family_info_list, [&](const auto& qfi) {
		return physical_device.getSurfaceSupportKHR(qfi.family_idx, *window.get_surface()) && !qfi.queues.empty();
	});

	// Add a queue which supports present if none exist
	auto present_queue = std::optional<uint32_t>{};
	if (present_queue_it == device_info.queue_family_info_list.end()) {
		present_queue = vkw::util::find_present_queue_index(*physical_device, *window.get_surface());
		if (present_queue.has_value()) {
			device_info.add_queues(*present_queue, 1.0f);
		}
		else {
			throw std::runtime_error("No queues with present support");
		}
	}
	else {
		present_queue = present_queue_it->family_idx;
	}

	// Create logical device
	auto logical_device = vkw::logical_device{device_info};


	// Swap Chain
	//--------------------------------------------------------------------------------

	// Find an SRGB surface format
	const auto srgb_format = vkw::util::select_srgb_surface_format(physical_device.getSurfaceFormatsKHR(*window.get_surface()));
	if (not srgb_format.has_value()) {
		throw std::runtime_error("No SRGB surface format");
	}

	// Check if the graphics and present queues are the same
	auto swap_queues = std::vector<uint32_t>{};
	if (*graphics_queue != *present_queue) {
		swap_queues = {*graphics_queue, *present_queue};
	}

	// Create the swapchain
	auto swapchain = vkw::swapchain{logical_device, window};
	swapchain.create(
		*srgb_format,
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst,
		window.get_window_size(),
		false,
		swap_queues
	);


	// Depth Buffer
	//--------------------------------------------------------------------------------
	auto depth_buffer = vkw::util::create_depth_buffer(
		logical_device,
		vkw::util::select_depth_format(logical_device.get_vk_physical_device()).value(),
		window.get_window_size()
	);


	// Render Pass
	//--------------------------------------------------------------------------------
/*
	// Setup the render pass info
	auto pass_info = vkw::render_pass_info{};

	pass_info.area_rect = vk::Rect2D{{0, 0}, window.get_window_size()};

	for (const auto& view : swapchain.get_image_views()) {
		pass_info.target_attachments.push_back({*view, *depth_buffer.get_vk_image_view()});
	}

	pass_info.attachment_descriptions.push_back( //color attachment
		vk::AttachmentDescription{
			vk::AttachmentDescriptionFlags{},
			swapchain.get_format().format,
			vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eClear,
			vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::ePresentSrcKHR
		}
	);
	
	pass_info.attachment_descriptions.push_back( //depth attachment
		vk::AttachmentDescription{
			vk::AttachmentDescriptionFlags{},
			depth_buffer.get_info().format,
			vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eClear,
			vk::AttachmentStoreOp::eDontCare,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eDepthStencilAttachmentOptimal
		}
	);

	auto graphics_subpass = vkw::subpass{};
	graphics_subpass.set_bind_point(vk::PipelineBindPoint::eGraphics);
	graphics_subpass.set_color_attachment(vk::AttachmentReference{0, vk::ImageLayout::eColorAttachmentOptimal});
	graphics_subpass.set_depth_stencil_attachment(vk::AttachmentReference{1, vk::ImageLayout::eDepthStencilAttachmentOptimal});
	pass_info.subpasses.push_back(graphics_subpass);


	// Create a render pass
	auto render_pass = vkw::render_pass{logical_device, pass_info};

	render_pass.set_clear_values({
		vk::ClearValue{vk::ClearColorValue{std::array{0.2f, 0.2f, 0.2f, 1.0f}}},
		vk::ClearValue{vk::ClearDepthStencilValue{1.0f, 0}}
	});
*/

	auto render_pass = vkw::render_pass_single{};

	render_pass.set_area(vk::Rect2D{{0, 0}, window.get_window_size()});

	for (auto i : std::views::iota(size_t{0}, swapchain.get_images().size())) {
		render_pass.add_frame_color_attachments(
			vk::RenderingAttachmentInfoKHR{
				*swapchain.get_image_views()[i],
				vk::ImageLayout::eColorAttachmentOptimal,
				vk::ResolveModeFlagBits::eNone,
				vk::ImageView{},
				vk::ImageLayout::eUndefined,
				vk::AttachmentLoadOp::eClear,
				vk::AttachmentStoreOp::eStore,
				vk::ClearValue{vk::ClearColorValue{std::array{0.2f, 0.2f, 0.2f, 1.0f}}}
			},
			swapchain.get_images()[i],
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::ePresentSrcKHR
		);
	}

	render_pass.set_depth_stencil_attachment(
		vk::RenderingAttachmentInfoKHR{
			*depth_buffer.get_vk_image_view(),
			vk::ImageLayout::eDepthStencilAttachmentOptimal,
			vk::ResolveModeFlagBits::eNone,
			vk::ImageView{},
			vk::ImageLayout::eUndefined,
			vk::AttachmentLoadOp::eClear,
			vk::AttachmentStoreOp::eDontCare,
			vk::ClearValue{vk::ClearDepthStencilValue{1.0f, 0}}		
		},
		depth_buffer,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eDepthStencilAttachmentOptimal
	);


	// Vertex Buffer
	//--------------------------------------------------------------------------------
	auto vertex_buffer = vkw::buffer<VertexPC>{logical_device, std::size(coloredCubeData), vk::BufferUsageFlagBits::eVertexBuffer};
	vertex_buffer.upload(coloredCubeData);


	// Model Uniform Buffer
	//--------------------------------------------------------------------------------
	auto uniform_buffer = vkw::buffer<glm::mat4>{logical_device, 1, vk::BufferUsageFlagBits::eUniformBuffer};
	const auto mvpc_matrix = vk::su::createModelViewProjectionClipMatrix(window.get_window_size());
	uniform_buffer.upload(mvpc_matrix);


	// Descriptor Pool
	//--------------------------------------------------------------------------------
	const auto pool_sizes = std::array{
		vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, 1}
	};

	auto descriptor_pool = vkw::descriptor_pool{logical_device, pool_sizes};


	// Descriptor Set
	//--------------------------------------------------------------------------------
	const auto bindings = std::array{
		vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr}
	};

	const auto descriptor_layout = vkw::descriptor_set_layout{logical_device, bindings};

	auto descriptor_set = descriptor_pool.allocate(descriptor_layout);

	descriptor_set.update(logical_device, vkw::write_buffer_set{0, {std::cref(uniform_buffer)}});


	// Shaders
	//--------------------------------------------------------------------------------
	const auto glsl_to_spirv = [](vk::ShaderStageFlagBits stage, const std::string& shader) {
		auto spirv = std::vector<uint32_t>{};
		if (!vk::su::GLSLtoSPV(stage, shader, spirv)) {
			throw std::runtime_error{"Error translating GLSL to SPIR-V"};
		}
		return spirv;
	};

	glslang::InitializeProcess();
	const auto vertex_shader_data   = glsl_to_spirv(vk::ShaderStageFlagBits::eVertex, vertexShaderText_PC_C);
	const auto fragment_shader_data = glsl_to_spirv(vk::ShaderStageFlagBits::eFragment, fragmentShaderText_C_C);
	glslang::FinalizeProcess();

	auto vertex_stage   = vkw::shader_stage{logical_device, vk::ShaderStageFlagBits::eVertex, vertex_shader_data, {}, {}};
	auto fragment_stage = vkw::shader_stage{logical_device, vk::ShaderStageFlagBits::eFragment, fragment_shader_data, {}, {}};


	// Pipeline
	//--------------------------------------------------------------------------------

	// Create a pipeline cache
	auto cache = vk::raii::PipelineCache{logical_device.get_vk_device(), vk::PipelineCacheCreateInfo{}};

	// Setup the pipeline info
	const auto pipeline_layout = vkw::pipeline_layout{
		logical_device,
		std::span{&descriptor_layout, 1},
		std::span<const vk::PushConstantRange>{}
	};

	auto pipeline_info = vkw::graphics_pipeline_info{};

	pipeline_info.shader_stages = {std::cref(vertex_stage), std::cref(fragment_stage)};
	pipeline_info.layout        = &pipeline_layout;
	pipeline_info.pass_details  = vkw::graphics_pipeline_info::render_pass_single_details{
		.color_formats = {swapchain.get_format().format, swapchain.get_format().format},
		.depth_stencil_format = depth_buffer.get_info().format
	};

	pipeline_info.raster_state.frontFace = vk::FrontFace::eClockwise;

	pipeline_info.depth_stencil_state = vk::PipelineDepthStencilStateCreateInfo{
		vk::PipelineDepthStencilStateCreateFlags{},
		VK_TRUE,
		VK_TRUE,
		vk::CompareOp::eLessOrEqual,
		VK_FALSE,
		VK_FALSE,
		vk::StencilOpState{vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::CompareOp::eAlways},
		vk::StencilOpState{vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::CompareOp::eAlways}
	};

	pipeline_info.add_vertex_input_binding(vk::VertexInputBindingDescription{0, static_cast<uint32_t>(sizeof(coloredCubeData[0]))});
	pipeline_info.add_vertex_input_attribute(vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32B32A32Sfloat, 0});
	pipeline_info.add_vertex_input_attribute(vk::VertexInputAttributeDescription{1, 0, vk::Format::eR32G32B32A32Sfloat, 16});
	pipeline_info.add_color_blend_attachment(
		vk::PipelineColorBlendAttachmentState{
			false,
			vk::BlendFactor::eZero,
			vk::BlendFactor::eZero,
			vk::BlendOp::eAdd,
			vk::BlendFactor::eZero,
			vk::BlendFactor::eZero,
			vk::BlendOp::eAdd,
			vk::ColorComponentFlagBits::eR
			| vk::ColorComponentFlagBits::eG
			| vk::ColorComponentFlagBits::eB
			| vk::ColorComponentFlagBits::eA
		}
	);
	

	// Create the pipeline
	auto pipeline = vkw::graphics_pipeline{logical_device, pipeline_info, &cache};


	// Setup Commands
	//--------------------------------------------------------------------------------

	// Create the command batch
	auto batch = vkw::command_batch{logical_device, 1, logical_device.get_queue(vk::QueueFlagBits::eGraphics, 0).family_index};

	auto image_num = uint32_t{0};

	batch.add_command([&](const vk::raii::CommandBuffer& buffer) mutable {
		//buffer.beginRenderPass(render_pass.get_render_pass_begin_info(image_num), vk::SubpassContents::eInline);
		render_pass.begin(image_num, buffer);

		buffer.setViewport(
			0,
			//camera.get_viewport()
			vk::Viewport{
				0.0f,
				0.0f,
				static_cast<float>(window.get_window_size().width),
				static_cast<float>(window.get_window_size().height),
				0.0f,
				1.0f
			}
		);

		//buffer.setScissor(0, render_pass.get_pass_info().area_rect);
		buffer.setScissor(0, render_pass.get_area());

		pipeline.bind(buffer);

		pipeline.bind_descriptor_sets(
			buffer,
			0,
			descriptor_set,
			{}
		);

		buffer.bindVertexBuffers(0, *vertex_buffer.get_vk_buffer(), vk::DeviceSize{0});
		buffer.draw(vertex_buffer.get_size(), 1, 0, 0);

		//buffer.endRenderPass();
		render_pass.end(image_num, buffer);
	});


	// Run Commands
	//--------------------------------------------------------------------------------
	auto image_acquired_semaphore = vk::raii::Semaphore{logical_device.get_vk_device(), vk::SemaphoreCreateInfo{}};
	const auto [acq_result, image_index] = swapchain.get_vk_swapchain().acquireNextImage(std::numeric_limits<uint64_t>::max(), *image_acquired_semaphore);

	image_num = image_index;

	batch.run_commands(0);


	// Submit and wait
	//--------------------------------------------------------------------------------
	const auto draw_fence  = vk::raii::Fence{logical_device.get_vk_device(), vk::FenceCreateInfo{}};
	const auto buffers     = vkw::util::to_vector(vkw::util::as_handles(batch.get_command_buffers(0)));
	const auto stage_flags = vk::PipelineStageFlags{vk::PipelineStageFlagBits::eColorAttachmentOutput};
	const auto submit_info = vk::SubmitInfo{*image_acquired_semaphore, stage_flags, buffers};
	logical_device.get_queue(vk::QueueFlagBits::eGraphics, 0).submit(submit_info, *draw_fence);

	logical_device.get_vk_device().waitForFences(*draw_fence, VK_TRUE, std::numeric_limits<uint64_t>::max());


	// Present
	//--------------------------------------------------------------------------------
	const auto present_info = vk::PresentInfoKHR{nullptr, *swapchain.get_vk_swapchain(), image_index};
	const auto present_result = logical_device.get_present_queue(*window.get_surface())->presentKHR(present_info);

	switch (present_result) {
		case vk::Result::eSuccess: break;
		case vk::Result::eSuboptimalKHR: {
			std::cout << "vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR\n";
			break;
		}
		default: assert(false);
	}

	logical_device.get_vk_device().waitIdle();


	// Wait for exit event
	while (not window.should_close()) {
		window.update();
	}

	return 0;
}

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
#include "vk_utils.h"

#include "vulkan_rendering.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

import vkw;

/*
static const std::string AppName = "VulkanTest";
static const std::string EngineName = "VulkanEngine";

static constexpr uint32_t Width = 800;
static constexpr uint32_t Height = 600;
*/

auto main(int argc, char** argv) -> int {
	glfwInit();


	// Instance
	//--------------------------------------------------------------------------------
	const auto app_info = vkw::app_info{};

	auto instance_info = vkw::instance_info{
		.layers = {},
		.extensions = vkw::util::get_surface_extensions()
	};
	instance_info.extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

	const auto debug_info = vkw::debug_info{
		.utils = true,
		.validation = true
	};

	auto instance = vkw::instance{app_info, instance_info, debug_info};
	auto& physical_device = instance.get_physical_device(0);


	// Window
	//--------------------------------------------------------------------------------
	auto window = vkw::window{instance.get_vk_instance(), "Vulkan Window", {1280, 1024}};


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
	auto swapchain = vkw::swapchain{logical_device, window.get_surface()};
	swapchain.create(
		*srgb_format,
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst,
		window.get_size(),
		false,
		swap_queues
	);


	// Depth Buffer
	//--------------------------------------------------------------------------------
	auto depth_buffer = vkw::util::create_depth_buffer(logical_device, vk::Format::eD16Unorm, window.get_size());


	// Render Pass
	//--------------------------------------------------------------------------------

	// Setup the render pass info
	auto pass_info = vkw::render_pass_info{};

	pass_info.area_rect = vk::Rect2D{{0, 0}, window.get_size()};

	auto graphics_subpass = vkw::subpass{};
	graphics_subpass.add_color_attachment(vk::AttachmentReference{0, vk::ImageLayout::eColorAttachmentOptimal});
	graphics_subpass.set_depth_stencil_attachment(vk::AttachmentReference{1, vk::ImageLayout::eDepthStencilAttachmentOptimal});
	pass_info.subpasses.push_back(graphics_subpass);

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

	pass_info.target_attachments = {
		{*swapchain.get_image_views()[0], *depth_buffer.get_vk_image_view()},
		{*swapchain.get_image_views()[1], *depth_buffer.get_vk_image_view()},
	};

	// Create a render pass
	auto render_pass = vkw::render_pass{logical_device, pass_info};

	const auto clear_values = std::vector<vk::ClearValue>{
		vk::ClearValue{vk::ClearColorValue{}},
		vk::ClearValue{vk::ClearDepthStencilValue{}}
	};

	render_pass.set_clear_values(clear_values);


	// Vertex Buffer
	//--------------------------------------------------------------------------------
	auto vertex_buffer = vkw::buffer<VertexPC>{logical_device, std::size(coloredCubeData), vk::BufferUsageFlagBits::eVertexBuffer};
	vertex_buffer.upload(coloredCubeData);


	// Model Uniform Buffer
	//--------------------------------------------------------------------------------
	auto uniform_buffer = vkw::buffer<glm::mat4>{logical_device, 1, vk::BufferUsageFlagBits::eUniformBuffer};
    const auto mvpc_matrix = vk::su::createModelViewProjectionClipMatrix(window.get_size());
	uniform_buffer.upload(mvpc_matrix);


	// Descriptor Pool
	//--------------------------------------------------------------------------------
	const auto pool_sizes = std::vector{vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, 1}};

	auto descriptor_pool = vkw::descriptor_pool{logical_device, pool_sizes};


	// Descriptor Set
	//--------------------------------------------------------------------------------
	const auto bindings = std::vector{
		vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr}
	};

	const auto descriptor_layout = vkw::descriptor_set_layout{logical_device, bindings};

	auto descriptor_set = descriptor_pool.allocate(descriptor_layout);

	descriptor_set.update(logical_device, vk::DescriptorType::eUniformBuffer, std::span{&uniform_buffer.get_vk_buffer(), 1});


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

	pipeline_info.layout        = &pipeline_layout;
	pipeline_info.pass          = &render_pass;
	pipeline_info.shader_stages = {std::cref(vertex_stage), std::cref(fragment_stage)};

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

	batch.add_command([&, frame = uint32_t{0}](vk::raii::CommandBuffer& buffer) mutable {
		buffer.beginRenderPass(render_pass.get_render_pass_begin_info(frame), vk::SubpassContents::eInline);

		buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline.get_vk_pipeline());

		buffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			*pipeline.get_pipeline_info().layout->get_vk_layout(),
			0,
			{*descriptor_set.get_vk_descriptor_set()},
			nullptr
		);

		buffer.bindVertexBuffers(0, {*vertex_buffer.get_vk_buffer()}, {0});

		buffer.setViewport(
			0,
			vk::Viewport{
				0.0f,
				0.0f,
				static_cast<float>(window.get_size().width),
				static_cast<float>(window.get_size().height),
				0.0f,
				1.0f
			}
		);

		buffer.setScissor(0, render_pass.get_pass_info().area_rect);

		buffer.draw(std::size(coloredCubeData), 1, 0, 0);

		buffer.endRenderPass();

		frame = (frame + 1) % render_pass.get_pass_info().target_attachments.size();
	});


	// Run Commands
	//--------------------------------------------------------------------------------
	auto image_acquired_semaphore = vk::raii::Semaphore{logical_device.get_vk_device(), vk::SemaphoreCreateInfo{}};
	const auto [acq_result, image_index] = swapchain.get_vk_swapchain().acquireNextImage(100'000'000, *image_acquired_semaphore);

	batch.run_commands(0);


	// Submit and wait
	//--------------------------------------------------------------------------------
	const auto draw_fence  = vk::raii::Fence{logical_device.get_vk_device(), vk::FenceCreateInfo{}};
	const auto buffers     = vkw::util::to_vector(vkw::util::as_handles(batch.get_command_buffers(0)));
	const auto stage_flags = vk::PipelineStageFlags{vk::PipelineStageFlagBits::eColorAttachmentOutput};
	const auto submit_info = vk::SubmitInfo{*image_acquired_semaphore, stage_flags, buffers};
	logical_device.get_queue(vk::QueueFlagBits::eGraphics, 0).submit(submit_info, *draw_fence);

	while (logical_device.get_vk_device().waitForFences(*draw_fence, VK_TRUE, 100'000'000) == vk::Result::eTimeout);


	// Present
	//--------------------------------------------------------------------------------
    const auto present_info = vk::PresentInfoKHR{nullptr, *swapchain.get_vk_swapchain(), image_index};
    const auto present_result = logical_device.get_present_queue(*window.get_surface())->presentKHR(present_info);

    switch (present_result) {
		case vk::Result::eSuccess: break;
		case vk::Result::eSuboptimalKHR: {
			std::cout << "vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR !\n";
			break;
		}
		default: assert(false);
    }

    logical_device.get_vk_device().waitIdle();


	// Wait for exit event
	while (!glfwWindowShouldClose(window.get_handle())) {
		glfwPollEvents();
	}

/*
	// Window
	auto window = Window(AppName, vk::Extent2D{Width, Height});

	// Vulkan Context
	auto context = VulkanContext(AppName, EngineName, window);

	// Swap Chain
	auto swap_chain = SwapChain(context, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc);

	// Command Pool
	auto cmd_buffer_pool = CommandBufferPool(context);

	// Depth Buffer
	auto depth_buffer = DepthBuffer(*context.physical_device, *context.device, vk::Format::eD16Unorm, context.window.get().size);

	// Model Uniform Buffer
	auto uniform_buffer = Buffer<glm::mat4>(*context.physical_device, *context.device, 1, vk::BufferUsageFlagBits::eUniformBuffer);
    const glm::mat4 mvpc_matrix = vk::su::createModelViewProjectionClipMatrix(context.window.get().size);
	uniform_buffer.upload(mvpc_matrix);

	// Render Pass
	std::unique_ptr<vk::raii::RenderPass> renderPass = makeRenderPass(*context.device, swap_chain.color_format, depth_buffer.format);

	// Compile Shaders
    glslang::InitializeProcess();
    auto vertexShaderModule = vk::raii::su::makeUniqueShaderModule(*context.device, vk::ShaderStageFlagBits::eVertex, vertexShaderText_PC_C);
    auto fragmentShaderModule = vk::raii::su::makeUniqueShaderModule(*context.device, vk::ShaderStageFlagBits::eFragment, fragmentShaderText_C_C);
    glslang::FinalizeProcess();

	// Framebuffers
	std::vector<std::unique_ptr<vk::raii::Framebuffer>> framebuffers = makeFramebuffers(*context.device, *renderPass, swap_chain.image_views, depth_buffer.image_view, context.window.get().size);

	// Vertex Buffer
	auto vertexBuffer = Buffer<VertexPC>(*context.physical_device, *context.device, std::size(coloredCubeData), vk::BufferUsageFlagBits::eVertexBuffer);
	vertexBuffer.upload(coloredCubeData);

	// Pipeline Layout
	std::unique_ptr<vk::raii::DescriptorSetLayout> descriptor_set_layout = makeDescriptorSetLayout(*context.device, {{vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex}});
	std::unique_ptr<vk::raii::PipelineLayout> pipelineLayout = makePipelineLayout(*context.device, *descriptor_set_layout);

	// Descriptor Set
    std::unique_ptr<vk::raii::DescriptorPool> descriptorPool = makeDescriptorPool(*context.device, {{vk::DescriptorType::eUniformBuffer, 1}});
    std::unique_ptr<vk::raii::DescriptorSet> descriptorSet = makeDescriptorSet(*context.device, *descriptorPool, *descriptor_set_layout);
    updateDescriptorSets(*context.device, *descriptorSet, {{vk::DescriptorType::eUniformBuffer, *uniform_buffer.buffer, nullptr}}, {});

	// Pipeline
	auto pipelineCache = std::make_unique<vk::raii::PipelineCache>(*context.device, vk::PipelineCacheCreateInfo());
    std::unique_ptr<vk::raii::Pipeline> graphicsPipeline = makeGraphicsPipeline(
    	*context.device,
    	*pipelineCache,
    	*vertexShaderModule,
    	nullptr,
    	*fragmentShaderModule,
    	nullptr,
    	vk::su::checked_cast<uint32_t>(sizeof(coloredCubeData[0])),
    	{{vk::Format::eR32G32B32A32Sfloat, 0}, {vk::Format::eR32G32B32A32Sfloat, 16}},
    	vk::FrontFace::eClockwise,
    	true,
    	*pipelineLayout,
    	*renderPass
	);

	// Semaphore
	auto imageAcquiredSemaphore = std::make_unique<vk::raii::Semaphore>(*context.device, vk::SemaphoreCreateInfo());


    vk::Result result;
    uint32_t   imageIndex;
    std::tie(result, imageIndex) = swap_chain.swap_chain->acquireNextImage(vk::su::FenceTimeout, **imageAcquiredSemaphore);
    assert(result == vk::Result::eSuccess);
    assert(imageIndex < swap_chain.images.size());

    std::array<vk::ClearValue, 2> clearValues;
    clearValues[0].color        = vk::ClearColorValue(std::array<float, 4>{0.2f, 0.2f, 0.2f, 0.2f});
    clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

    cmd_buffer_pool.buffer->begin({});

    const auto renderPassBeginInfo = vk::RenderPassBeginInfo(**renderPass, **framebuffers[imageIndex], vk::Rect2D(vk::Offset2D(0, 0), context.window.get().size), clearValues);
    cmd_buffer_pool.buffer->beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
    cmd_buffer_pool.buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, **graphicsPipeline);
    cmd_buffer_pool.buffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, **pipelineLayout, 0, {**descriptorSet}, nullptr);

    cmd_buffer_pool.buffer->bindVertexBuffers(0, {**vertexBuffer.buffer}, {0});

    cmd_buffer_pool.buffer->setViewport(
		0,
		vk::Viewport(
			0.0f,
			0.0f,
			static_cast<float>(context.window.get().size.width),
			static_cast<float>(context.window.get().size.height),
			0.0f,
			1.0f
		)
	);
    cmd_buffer_pool.buffer->setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), context.window.get().size));

	cmd_buffer_pool.buffer->draw(12 * 3, 1, 0, 0);

    cmd_buffer_pool.buffer->endRenderPass();
    cmd_buffer_pool.buffer->end();

	// Submit and wait
    //vk::raii::su::submitAndWait(*context.device, *context.graphics_queue, *cmd_buffer_pool.buffer);
	std::unique_ptr<vk::raii::Fence> drawFence = std::make_unique<vk::raii::Fence>(*context.device, vk::FenceCreateInfo());
	vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    vk::SubmitInfo         submitInfo(**imageAcquiredSemaphore, waitDestinationStageMask, **cmd_buffer_pool.buffer);
    context.graphics_queue->submit(submitInfo, **drawFence);

	while (vk::Result::eTimeout == context.device->waitForFences({**drawFence}, VK_TRUE, vk::su::FenceTimeout))
		;

	// Present
    vk::PresentInfoKHR presentInfoKHR(nullptr, **swap_chain.swap_chain, imageIndex);
    result = context.present_queue->presentKHR(presentInfoKHR);
    switch (result) {
      case vk::Result::eSuccess: break;
      case vk::Result::eSuboptimalKHR:
        std::cout << "vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR !\n";
        break;
      default: assert(false);  // an unexpected result is returned !
    }
    //std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    context.device->waitIdle();

	// Wait for exit event
	while (!glfwWindowShouldClose(context.window.get().handle)) {
		glfwPollEvents();
	}
*/

	glfwTerminate();

	return 0;
}

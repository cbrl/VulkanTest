#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <glslang/SPIRV/GlslangToSpv.h>

#include "utils/math.hpp"
#include "utils/shaders.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include "geometry.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <limits>
#include <memory>
#include <ranges>
#include <string>
#include <thread>
#include <vector>

import vkw;
import utils.handle;
import utils.handle_table;


// TODO:
//   - Add buffer_view class to encapsulate vk::raii::BufferView
//   - Use descriptor indexing (core in Vulkan 1.2)
//     - Single descriptor_set allocated from a single descriptor_pool
//     - Configurable descriptor counts with large defaults
//     - Track free indices and assign them at resource creation
//   - Integrate VMA
//   - Upgrade to Vulkan 1.3 minimum
//     - https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/chap50.html#roadmap-2022


template<typename ResourceT>
class indexed_shader_resource : public ResourceT {
	friend class bindless_descriptor_manager;

public:
	using ResourceT::ResourceT;

	virtual ~indexed_shader_resource() {
		if (descriptor_manager) {
			descriptor_manager->remove(this);
		}
	}

	[[nodiscard]]
	auto get_handle() const noexcept -> handle64 {
		return id;
	}

	[[nodiscard]]
	auto get_descriptor_manager() const noexcept -> const std::shared_ptr<bindless_descriptor_manager>& {
		return descriptor_manager;
	}

private:
	auto set_handle(handle64 h) noexcept {
		id = h;
	}

	auto set_descriptor_manager(std::shared_ptr<bindless_descriptor_manager> manager) {
		descriptor_manager = std::move(manager);
	}

	std::shared_ptr<bindless_descriptor_manager> descriptor_manager;
	handle64 id = handle64::invalid_handle();
};


template<typename T = void>
using indexed_buffer = indexed_shader_resource<vkw::buffer<T>>;
using indexed_image_view = indexed_shader_resource<vkw::image_view>;
using indexed_sampler = indexed_shader_resource<vkw::sampler>;

class bindless_descriptor_manager : public std::enable_shared_from_this<bindless_descriptor_manager> {
private:
	enum descriptor_index : uint32_t {
		storage_buffer,
		sampled_image,
		storage_image,
		sampler,
		count
	};

public:
	struct descriptor_sizes {
		uint32_t storage_buffers = 128 * 1024;
		uint32_t sampled_images = 128 * 1024;
		uint32_t storage_images = 32 * 1024;
		uint32_t samplers = 1024;
	};

	template<typename... ArgsT>
	[[nodiscard]]
	static auto create(ArgsT&&... args) -> std::shared_ptr<bindless_descriptor_manager> {
		return std::make_shared<bindless_descriptor_manager>(std::forward<ArgsT>(args)...);
	}

	bindless_descriptor_manager(const std::shared_ptr<vkw::logical_device>& logical_device, const descriptor_sizes& sizes = {}) {
		const auto pool_sizes = std::array{
			vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, sizes.storage_buffers},
			vk::DescriptorPoolSize{vk::DescriptorType::eSampledImage,  sizes.sampled_images},
			vk::DescriptorPoolSize{vk::DescriptorType::eStorageImage,  sizes.storage_images},
			vk::DescriptorPoolSize{vk::DescriptorType::eSampler,       sizes.samplers}
		};

		const auto bindings = std::array{
			vk::DescriptorSetLayoutBinding{descriptor_index::storage_buffer, vk::DescriptorType::eStorageBuffer, sizes.storage_buffers, vk::ShaderStageFlagBits::eAll, nullptr},
			vk::DescriptorSetLayoutBinding{descriptor_index::sampled_image,  vk::DescriptorType::eSampledImage,  sizes.sampled_images,  vk::ShaderStageFlagBits::eAll, nullptr},
			vk::DescriptorSetLayoutBinding{descriptor_index::storage_image,  vk::DescriptorType::eStorageImage,  sizes.storage_images,  vk::ShaderStageFlagBits::eAll, nullptr},
			vk::DescriptorSetLayoutBinding{descriptor_index::sampler,        vk::DescriptorType::eSampler,       sizes.samplers,        vk::ShaderStageFlagBits::eAll, nullptr}
		};

		descriptor_pool   = vkw::descriptor_pool::create(logical_device, pool_sizes, vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind | vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
		descriptor_layout = vkw::descriptor_set_layout::create(logical_device, bindings, vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool);
		descriptor_set    = descriptor_pool->allocate(descriptor_layout);
	}

	[[nodiscard]]
	auto get_descriptor_set_layout() const noexcept -> const std::shared_ptr<vkw::descriptor_set_layout>& {
		return descriptor_layout;
	}

	[[nodiscard]]
	auto get_descriptor_set() const noexcept -> const std::shared_ptr<vkw::descriptor_set>& {
		return descriptor_set;
	}

	template<typename T, typename... ArgsT>
	[[nodiscard]]
	auto create_storage_buffer(ArgsT&&... args) -> std::shared_ptr<indexed_buffer<T>> {
		auto result = std::make_shared<indexed_buffer<T>>(std::forward<ArgsT>(args)...);
		assert(result->get_usage() & vk::BufferUsageFlagBits::eStorageBuffer);

		result->set_handle(buffer_table.create_handle());
		result->set_descriptor_manager(shared_from_this());
		buffers[result->get_handle()] = result;

		descriptor_set->update(vkw::write_buffer_set{
			.binding = descriptor_index::storage_buffer,
			.array_offset = static_cast<uint32_t>(result->get_handle().index),
			.buffers = {std::cref(*result)}
		});

		return result;
	}

	template<typename T>
	auto remove(indexed_buffer<T>* buffer) -> void {
		buffer_table.release_handle(buffer->get_handle());
	}

private:

	std::shared_ptr<vkw::descriptor_pool> descriptor_pool;
	std::shared_ptr<vkw::descriptor_set_layout> descriptor_layout;
	std::shared_ptr<vkw::descriptor_set> descriptor_set;

	handle_table<handle64> buffer_table;
	handle_table<handle64> sampled_image_table;
	handle_table<handle64> storage_image_table;
	handle_table<handle64> sampler_table;

	std::unordered_map<handle64, std::weak_ptr<vkw::buffer<void>>> buffers;
	std::unordered_map<handle64, std::weak_ptr<vkw::image_view>> sampled_images;
	std::unordered_map<handle64, std::weak_ptr<vkw::image_view>> storage_images;
	std::unordered_map<handle64, std::weak_ptr<vkw::sampler>> samplers;
};


/*
class BufferAllocator {
public:
	BufferAllocator(vk::DeviceSize max_size) {
		ranges[0] = max_size;
		reverse_ranges[max_size] = 0;
	}

	auto allocate(vk::DeviceSize size) -> vk::DeviceSize {
		for (const auto [start, end] : ranges) {
			if ((end - start) <= size) {
				ranges[start] = start + size;
				reverse_ranges[end] = start + size + 1;
				return start;
			}
		}
		assert(false && "No memory available");
	}

	auto deallocate(vk::DeviceSize start, vk::DeviceSize size) -> void {
		const auto range_end = start + size;

		// If there's a free block ending just before this block, then merge them. Otherwise, just insert the entry.
		if (const auto end_it = reverse_ranges.find(start - 1); end_it != reverse_ranges.end()) {
			// Update the end of the prior block to be the end of the current block
			ranges[end_it->second] = range_end;

			// Insert an entry in the reverse map pointing from the end of the current block to the beginning of the prior block
			reverse_ranges[range_end] = ranges[end_it->second];

			// Erase the outdated entry in the reverse map
			reverse_ranges.erase(end_it);
		}
		else {
			ranges[start] = range_end;
			reverse_ranges[range_end] = start;
		}

		// If there's a free block starting just after this block, then merge them.
		if (const auto begin_it = ranges.find(range_end + 1); begin_it != ranges.end()) {
			// The beginning of the current block will not be equal to "start" if there was an unallocated block before the current one
			const auto block_begin = reverse_ranges[range_end];

			// Update the end of the next block's reverse map to point to the beginning of the current block
			reverse_ranges[begin_it->second] = block_begin;

			// Update the end of the current block to be the end of the next block
			ranges[block_begin] = begin_it->second;

			// Erase the outdated entry
			ranges.erase(begin_it);
		}
	}

private:
	std::unordered_map<vk::DeviceSize, vk::DeviceSize> ranges;
	std::unordered_map<vk::DeviceSize, vk::DeviceSize> reverse_ranges;
};

class BufferAllocator {
public:
	BufferAllocator(vk::DeviceSize max_size) {
		start_to_size[0] = max_size;
		size_to_start.emplace(max_size, 0);
	}

	auto allocate(vk::DeviceSize size) -> vk::DeviceSize {
		if (const auto it = size_to_start.find(size); it != size_to_start.end()) {
			const auto start = it->second;
			start_to_size.erase(it->second);
			size_to_start.erase(it);
			return start;
		}

		if (const auto it = size_to_start.upper_bound(size); it != size_to_start.end()) {
			const auto start = it->second;

			start_to_size[start] = size;
			size_to_start.emplace(size, start);

			start_to_size[start + size + 1] = it->first - size;
			size_to_start.emplace(it->first - size, start + size + 1);

			size_to_start.erase(it);

			return start;
		}
	}

	auto deallocate(vk::DeviceSize start, vk::DeviceSize size) -> void {
		assert(false);
	}

private:
	std::map<vk::DeviceSize, vk::DeviceSize> start_to_size;
	std::multimap<vk::DeviceSize, vk::DeviceSize> size_to_start;
};
*/


auto main(int argc, char** argv) -> int {
	// Instance
	//--------------------------------------------------------------------------------
	const auto app_info = vkw::app_info{};

	const auto instance_info = vkw::instance_info{};

	auto debug_info = vkw::debug_info{};
	debug_info.utils.enabled = true;
	debug_info.validation.enabled = true;

	auto instance = vkw::instance::create(app_info, instance_info, debug_info);
	auto physical_device = instance->get_physical_device(0);


	// Window
	//--------------------------------------------------------------------------------
	auto window = vkw::glfw_window::create(
		instance,
		"Vulkan Window",
		{1280, 1024},
		{{GLFW_RESIZABLE, GLFW_FALSE}}
	);

	window->add_event_handler([](vkw::window& win, vkw::window::event e, uint64_t param, void*) {
		if ((e == vkw::window::event::key_down) && (param == GLFW_KEY_ESCAPE)) {
			win.set_should_close(true);
		}
	});


	// Logical Device
	//--------------------------------------------------------------------------------
	auto device_info = vkw::logical_device_info{physical_device};

	// Add a graphics queue
	const auto graphics_queue_family = device_info.add_queues(vk::QueueFlagBits::eGraphics, 1.0f);
	if (not graphics_queue_family.has_value()) {
		throw std::runtime_error("No queues with graphics support");
	}

	auto present_queue_family = std::optional<uint32_t>{};

	// Check if the graphics queue supports present	
	if (physical_device->getSurfaceSupportKHR(*graphics_queue_family, *window->get_vk_handle())) {
		present_queue_family = graphics_queue_family;
	}
	else {
		// Check if any existing queues support present. If so, pick the first one that does.
		const auto present_queues = device_info.get_present_queue_families(*window);

		if (present_queues.empty()) {
			present_queue_family = vkw::util::find_present_queue_index(**physical_device, *window->get_vk_handle());

			if (present_queue_family.has_value()) {
				device_info.add_queues(*present_queue_family, 1.0f);
			}
		}
		else {
			present_queue_family = present_queues.front();
		}
	}

	if (not present_queue_family.has_value()) {
		throw std::runtime_error{"No queues with present support"};
	}

	// Create logical device
	auto logical_device = vkw::logical_device::create(device_info);


	// Swap Chain
	//--------------------------------------------------------------------------------

	auto swapchain = vkw::swapchain::create(logical_device, window);

	// Find an SRGB surface format
	const auto srgb_format = vkw::util::select_srgb_surface_format(physical_device->getSurfaceFormatsKHR(*window->get_vk_handle()));
	if (not srgb_format.has_value()) {
		throw std::runtime_error{"No SRGB surface format"};
	}

	// Check if the graphics and present queues are the same
	auto swap_queues = std::vector<uint32_t>{};
	if (*graphics_queue_family != *present_queue_family) {
		swap_queues = {*graphics_queue_family, *present_queue_family};
	}

	// Create the swapchain
	swapchain->rebuild(
		*srgb_format,
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst,
		window->get_window_size(),
		false,
		swap_queues
	);


	// Depth Buffer
	//--------------------------------------------------------------------------------
	auto depth_buffers = std::vector<std::shared_ptr<vkw::image_view>>{};
	for (auto _ : std::views::iota(size_t{0}, swapchain->get_image_count())) {
		depth_buffers.push_back(vkw::util::create_depth_buffer(
			logical_device,
			vkw::util::select_depth_format(*logical_device->get_physical_device()).value(),
			window->get_window_size()
		));
	}


	// Render Pass
	//--------------------------------------------------------------------------------
/*
	// Setup the render pass info
	auto pass_info = vkw::render_pass_info{};

	pass_info.area_rect = vk::Rect2D{{0, 0}, window->get_window_size()};

	for (auto i : std::views::iota(size_t{0}, swapchain->get_image_count())) {
		pass_info.target_attachments.push_back({swapchain->get_image_view(i), depth_buffers[i]});
	}

	pass_info.attachment_descriptions.push_back( //color attachment
		vk::AttachmentDescription{
			vk::AttachmentDescriptionFlags{},
			swapchain->get_format().format,
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
	auto render_pass = vkw::render_pass::create(logical_device, pass_info);

	render_pass->set_clear_values({
		vk::ClearValue{vk::ClearColorValue{std::array{0.2f, 0.2f, 0.2f, 1.0f}}},
		vk::ClearValue{vk::ClearDepthStencilValue{1.0f, 0}}
	});
*/

	auto render_pass = vkw::render_pass_single::create();

	render_pass->set_area(vk::Rect2D{{0, 0}, window->get_window_size()});

	render_pass->add_frame_color_attachments(
		swapchain,
		vkw::color_attachment{.clear_value = vk::ClearColorValue{std::array{0.2f, 0.2f, 0.2f, 1.0f}}}
	);

	render_pass->add_depth_stencil_attachments(depth_buffers, vkw::depth_attachment{});


	// Vertex Buffer
	//--------------------------------------------------------------------------------
	auto vertex_buffer = vkw::buffer<VertexPC>::create(logical_device, std::size(coloredCubeData), vk::BufferUsageFlagBits::eVertexBuffer);
	vertex_buffer->upload(coloredCubeData);


	// Model Uniform Buffer
	//--------------------------------------------------------------------------------
	auto uniform_buffer = vkw::buffer<glm::mat4>::create(logical_device, 1, vk::BufferUsageFlagBits::eUniformBuffer);
	const auto mvpc_matrix = vk::su::createModelViewProjectionClipMatrix(window->get_window_size());
	uniform_buffer->upload(mvpc_matrix);


	// Descriptor Pool
	//--------------------------------------------------------------------------------
	const auto pool_sizes = std::array{
		vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, 1}
	};

	auto descriptor_pool = vkw::descriptor_pool::create(logical_device, pool_sizes);


	// Descriptor Set
	//--------------------------------------------------------------------------------
	const auto bindings = std::array{
		vk::DescriptorSetLayoutBinding{0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr}
	};

	const auto descriptor_layout = vkw::descriptor_set_layout::create(logical_device, bindings);

	auto descriptor_set = descriptor_pool->allocate(descriptor_layout);

	descriptor_set->update(vkw::write_buffer_set{
		.binding = 0,
		.buffers = {std::cref(*uniform_buffer)}
	});



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

	auto vertex_stage   = vkw::shader_stage::create(logical_device, vk::ShaderStageFlagBits::eVertex, vertex_shader_data, {}, {});
	auto fragment_stage = vkw::shader_stage::create(logical_device, vk::ShaderStageFlagBits::eFragment, fragment_shader_data, {}, {});


	// Pipeline
	//--------------------------------------------------------------------------------

	// Create a pipeline cache
	auto cache = vk::raii::PipelineCache{logical_device->get_vk_handle(), vk::PipelineCacheCreateInfo{}};

	// Setup the pipeline info
	const auto pipeline_layout = vkw::pipeline_layout::create(
		logical_device,
		std::span{&descriptor_layout, 1},
		std::span<const vk::PushConstantRange>{}
	);

	auto pipeline_info = vkw::graphics_pipeline_info{};

	pipeline_info.shader_stages = {vertex_stage, fragment_stage};
	pipeline_info.layout        = pipeline_layout;
	pipeline_info.pass_details  = render_pass;

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
	auto pipeline = vkw::graphics_pipeline::create(logical_device, pipeline_info, &cache);


	// Setup Commands
	//--------------------------------------------------------------------------------

	// Create the command batch
	auto batch = vkw::command_batch::create(logical_device, 1, logical_device->get_queue(vk::QueueFlagBits::eGraphics, 0)->family_index);

	auto image_num = uint32_t{0};

	batch->add_command([&](const vk::raii::CommandBuffer& buffer) mutable {
		//buffer.beginRenderPass(render_pass->get_render_pass_begin_info(image_num), vk::SubpassContents::eInline);
		render_pass->begin(image_num, buffer);

		buffer.setViewport(
			0,
			//camera.get_viewport()
			vk::Viewport{
				0.0f,
				0.0f,
				static_cast<float>(window->get_window_size().width),
				static_cast<float>(window->get_window_size().height),
				0.0f,
				1.0f
			}
		);

		//buffer.setScissor(0, render_pass->get_pass_info().area_rect);
		buffer.setScissor(0, render_pass->get_area());

		pipeline->bind(buffer);

		pipeline->bind_descriptor_sets(buffer, 0, *descriptor_set, {});

		buffer.bindVertexBuffers(0, *vertex_buffer->get_vk_handle(), vk::DeviceSize{0});
		buffer.draw(vertex_buffer->get_size(), 1, 0, 0);

		//buffer.endRenderPass();
		render_pass->end(image_num, buffer);
	});


	// Run Commands
	//--------------------------------------------------------------------------------
	auto image_acquired_semaphore = vk::raii::Semaphore{logical_device->get_vk_handle(), vk::SemaphoreCreateInfo{}};
	const auto [acq_result, image_index] = swapchain->get_vk_handle().acquireNextImage(std::numeric_limits<uint64_t>::max(), *image_acquired_semaphore);

	image_num = image_index;

	batch->run_commands(0);


	// Submit and wait
	//--------------------------------------------------------------------------------
	const auto draw_fence  = vk::raii::Fence{logical_device->get_vk_handle(), vk::FenceCreateInfo{}};
	const auto buffers     = vkw::ranges::to<std::vector>(batch->get_command_buffers(0) | vkw::util::as_handles());
	const auto stage_flags = vk::PipelineStageFlags{vk::PipelineStageFlagBits::eColorAttachmentOutput};
	const auto submit_info = vk::SubmitInfo{*image_acquired_semaphore, stage_flags, buffers};
	logical_device->get_queue(vk::QueueFlagBits::eGraphics, 0)->submit(submit_info, *draw_fence);

	logical_device->get_vk_handle().waitForFences(*draw_fence, VK_TRUE, std::numeric_limits<uint64_t>::max());


	// Present
	//--------------------------------------------------------------------------------
	const auto present_info = vk::PresentInfoKHR{nullptr, *swapchain->get_vk_handle(), image_index};
	const auto present_result = logical_device->get_present_queue(*window)->presentKHR(present_info);

	switch (present_result) {
		case vk::Result::eSuccess: break;
		case vk::Result::eSuboptimalKHR: {
			std::cout << "vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR\n";
			break;
		}
		default: assert(false);
	}

	logical_device->get_vk_handle().waitIdle();


	// Wait for exit event
	while (not window->should_close()) {
		window->update();
	}

	return 0;
}

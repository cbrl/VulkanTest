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

#include "vulkan_wrapper/instance.h"
#include "vulkan_wrapper/logical_device.h"
#include "vulkan_wrapper/render_pass.h"
#include "vulkan_wrapper/window.h"

static const std::string AppName = "VulkanTest";
static const std::string EngineName = "VulkanEngine";

static constexpr uint32_t Width = 800;
static constexpr uint32_t Height = 600;


int main(int argc, char** argv) {
	glfwInit();


	const auto app_info = vkw::Instance::AppInfo{};

	auto instance_info = vkw::Instance::InstanceInfo{
		.layers = {},
		.extensions = vkw::util::getSurfaceExtensions()
	};
	instance_info.extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

	const auto debug_info = vkw::Instance::DebugInfo{
		.utils = true,
		.validation = true
	};

	auto instance = vkw::Instance(app_info, instance_info, debug_info);
	auto& physical_device = instance.getPhysicalDevice(0);

	auto window = vkw::Window(instance.getVkInstance(), "My Window", {1280, 1024});

	auto device_info = vkw::LogicalDevice::LogicalDeviceInfo{
		.physical_device = physical_device,
		.features = {},
		.extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME}
	};

	// Add a graphics queue
	device_info.addQueues(vk::QueueFlagBits::eGraphics, 1.0f);

	// Add a queue which supports present
	for (auto family_idx : std::views::iota(uint32_t{0}, physical_device.getQueueFamilyProperties().size())) {
		if (physical_device.getSurfaceSupportKHR(family_idx, *window.getSurface())) {
			device_info.addQueues(family_idx, 1.0f);
			break;
		}
	}

	auto logical_device = vkw::LogicalDevice(device_info);

	if (logical_device.getPresentQueues(window.getSurface()).empty()) {
		throw std::runtime_error("No queues with present support");
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

	// Pipeline Layout
    std::unique_ptr<vk::raii::DescriptorSetLayout> descriptor_set_layout = makeDescriptorSetLayout(*context.device, {{vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex}});
    std::unique_ptr<vk::raii::PipelineLayout> pipelineLayout = makePipelineLayout(*context.device, *descriptor_set_layout);

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

	// Allocate memory for the vertex buffer
    const auto memoryRequirements = vertexBuffer.buffer->getMemoryRequirements();
    const uint32_t memoryTypeIndex = findMemoryType(
		context.physical_device->getMemoryProperties(),
		memoryRequirements.memoryTypeBits,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);

    const auto memoryAllocateInfo = vk::MemoryAllocateInfo(memoryRequirements.size, memoryTypeIndex);
    std::unique_ptr<vk::raii::DeviceMemory> deviceMemory = std::make_unique<vk::raii::DeviceMemory>(*context.device, memoryAllocateInfo);

	// Copy data into vertex buffer
    auto* pData = static_cast<uint8_t*>(deviceMemory->mapMemory(0, memoryRequirements.size));
    std::memcpy(pData, coloredCubeData, sizeof(coloredCubeData));
    deviceMemory->unmapMemory();

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

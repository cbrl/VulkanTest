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

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

static const std::string AppName = "VulkanTest";
static const std::string EngineName = "VulkanEngine";

static constexpr uint32_t Width = 800;
static constexpr uint32_t Height = 600;


class Window {
public:
	Window(const std::string& name, const vk::Extent2D& size) : handle(handle), name(name), size(size) {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		handle = glfwCreateWindow(size.width, size.height, name.c_str(), nullptr, nullptr);
		if (!handle) {
			throw std::runtime_error("Failed to create window");
		}
	}

	~Window() {
		glfwDestroyWindow(handle);
	}

//private:
	GLFWwindow* handle;
	std::string name;
	vk::Extent2D size;
};

class Surface {
public:
	Surface(const vk::raii::Instance& instance, const Window& window) : window(window) {
		VkSurfaceKHR surface_khr;

		const VkResult err = glfwCreateWindowSurface(static_cast<VkInstance>(*instance), window.handle, nullptr, &surface_khr);
		if (err != VK_SUCCESS) {
			throw std::runtime_error("Failed to create window surface");
		}

		surface = std::make_unique<vk::raii::SurfaceKHR>(instance, surface_khr);
	}

//private:
	const Window& window;
	std::unique_ptr<vk::raii::SurfaceKHR> surface;
};

class VulkanContext {
public:
	VulkanContext(const std::string& app_name, const std::string& engine_name, Window& window) {
		// Context
		context = std::make_unique<vk::raii::Context>();

		// Instance
		instance = makeInstance(*context, app_name, engine_name, {}, getInstanceExtensions());

		// Physical Device
		physical_device = makePhysicalDevice(*instance);

		// Surface
		surface = std::make_unique<Surface>(*instance, window);

		// Logical Device
		std::tie(graphics_queue_family_idx, present_queue_family_idx) = findGraphicsAndPresentQueueFamilyIndex(*physical_device, *surface->surface);
		device = makeDevice(*physical_device, graphics_queue_family_idx, getDeviceExtensions());

		// Graphics Queue 
		graphics_queue = std::make_unique<vk::raii::Queue>(*device, graphics_queue_family_idx, 0);

		// Present Queue
		present_queue = std::make_unique<vk::raii::Queue>(*device, present_queue_family_idx, 0);
	}

//private:
	std::unique_ptr<vk::raii::Context> context;
	std::unique_ptr<vk::raii::Instance> instance;
	std::unique_ptr<vk::raii::PhysicalDevice> physical_device;
	std::unique_ptr<Surface> surface;
	std::unique_ptr<vk::raii::Device> device;
	std::unique_ptr<vk::raii::Queue> graphics_queue;
	std::unique_ptr<vk::raii::Queue> present_queue;

	uint32_t graphics_queue_family_idx = 0;
	uint32_t present_queue_family_idx = 0;
};


class SwapChain {
public:
	SwapChain(
		const VulkanContext& context,
		vk::ImageUsageFlags usage,
		const std::unique_ptr<vk::raii::SwapchainKHR>& old_swap_chain
	) {
		const vk::SurfaceCapabilitiesKHR surface_capabilities = context.physical_device->getSurfaceCapabilitiesKHR(**context.surface->surface);
		const vk::SurfaceFormatKHR surface_format = selectSurfaceFormat(context.physical_device->getSurfaceFormatsKHR(**context.surface->surface));
		const vk::PresentModeKHR present_mode = selectPresentMode(context.physical_device->getSurfacePresentModesKHR(**context.surface->surface));

		color_format = surface_format.format;

		const vk::Extent2D swap_chain_extent = [&] {
			vk::Extent2D extent;

			if (surface_capabilities.currentExtent.width == std::numeric_limits<uint32_t>::max()) {
				// If the surface size is undefined, the size is set to the size of the images requested.
				extent.width  = std::clamp(context.surface->window.size.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
				extent.height = std::clamp(context.surface->window.size.height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
			}
			else {
				// If the surface size is defined, the swap chain size must match
				extent = surface_capabilities.currentExtent;
			}

			return extent;
		}();

		const vk::SurfaceTransformFlagBitsKHR pre_transform = [&] {
			if (surface_capabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity) {
				return vk::SurfaceTransformFlagBitsKHR::eIdentity;
			}
			else {
				return surface_capabilities.currentTransform;
			}
		}();

		const vk::CompositeAlphaFlagBitsKHR composite_alpha = [&] {
			if (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePreMultiplied) {
				return vk::CompositeAlphaFlagBitsKHR::ePreMultiplied;
			}
			else if (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePostMultiplied) {
				return vk::CompositeAlphaFlagBitsKHR::ePostMultiplied;
			}
			else if (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit) {
				return vk::CompositeAlphaFlagBitsKHR::eInherit;
			}
			else {
				return vk::CompositeAlphaFlagBitsKHR::eOpaque;
			}
		}();

		auto swap_chain_create_info = vk::SwapchainCreateInfoKHR(
			{},
			**context.surface->surface,
			surface_capabilities.minImageCount,
			color_format,
			surface_format.colorSpace,
			swap_chain_extent,
			1,
			usage,
			vk::SharingMode::eExclusive,
			{},
			pre_transform,
			composite_alpha,
			present_mode,
			true,
			old_swap_chain ? **old_swap_chain : nullptr
		);

		if (context.graphics_queue_family_idx != context.present_queue_family_idx) {
			const uint32_t queue_family_indices[2] = {context.graphics_queue_family_idx, context.present_queue_family_idx};

			// If the graphics and present queues are from different queue families, we either have to explicitly
			// transfer ownership of images between the queues, or we have to create the swapchain with imageSharingMode
			// as vk::SharingMode::eConcurrent
			swap_chain_create_info.imageSharingMode      = vk::SharingMode::eConcurrent;
			swap_chain_create_info.queueFamilyIndexCount = 2;
			swap_chain_create_info.pQueueFamilyIndices   = queue_family_indices;
		}

		swap_chain = std::make_unique<vk::raii::SwapchainKHR>(*context.device, swap_chain_create_info);
		images = swap_chain->getImages();
		image_views.reserve(images.size());

		const auto component_mapping = vk::ComponentMapping(vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA);
		const auto sub_resource_range = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

		for (const auto& image : images) {
			const auto image_view_create_info = vk::ImageViewCreateInfo(
				{},
				static_cast<vk::Image>(image),
				vk::ImageViewType::e2D,
				color_format,
				component_mapping,
				sub_resource_range
			);

			image_views.emplace_back(*context.device, image_view_create_info);
		}
	}

	static vk::SurfaceFormatKHR selectSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats) {
		assert(!formats.empty());

		// Priority list of formats to look for
		static const vk::Format desired_formats[] = {
			vk::Format::eB8G8R8A8Unorm,
			vk::Format::eR8G8B8A8Unorm,
			vk::Format::eB8G8R8Unorm,
			vk::Format::eR8G8B8Unorm
		};

		// Only look for SRGB color space
		const vk::ColorSpaceKHR desired_color_space = vk::ColorSpaceKHR::eSrgbNonlinear;

		for (const auto& format : desired_formats) {
			const auto it = std::ranges::find_if(formats, [format, desired_color_space](const vk::SurfaceFormatKHR& f) {
				return (f.format == format) && (f.colorSpace == desired_color_space);
			});

			if (it != formats.end()) {
				return *it;
			}
		}

		throw std::runtime_error("No desired surface format found");
	}

	static vk::PresentModeKHR selectPresentMode(const std::vector<vk::PresentModeKHR>& modes) {
		static const vk::PresentModeKHR desired_modes[] = {
			vk::PresentModeKHR::eMailbox,
			vk::PresentModeKHR::eImmediate
		};

		for (const auto& mode : desired_modes) {
			if (const auto it = std::ranges::find(modes, mode); it != modes.end()) {
				return *it;
			}
		}

		// FIFO is guaranteed to be available
		return vk::PresentModeKHR::eFifo;
    }

//private:
	vk::Format                              color_format;
	std::unique_ptr<vk::raii::SwapchainKHR> swap_chain;
	std::vector<VkImage>                    images;
	std::vector<vk::raii::ImageView>        image_views;
};


class CommandBufferPool {
public:
	CommandBufferPool(VulkanContext& context) {
		// Create command pool
		const auto command_pool_create_info = vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, context.graphics_queue_family_idx);
        pool = std::make_unique<vk::raii::CommandPool>(*context.device, command_pool_create_info);

		// Create command buffers
		const auto command_buffer_allocate_info = vk::CommandBufferAllocateInfo(**pool, vk::CommandBufferLevel::ePrimary, 1);
		auto buffers = vk::raii::CommandBuffers(*context.device, command_buffer_allocate_info);
        buffer = std::make_unique<vk::raii::CommandBuffer>(std::move(buffers.front()));
	}

//private:
	std::unique_ptr<vk::raii::CommandPool> pool;
	std::unique_ptr<vk::raii::CommandBuffer> buffer;
};


template<typename T>
class Buffer {
public:
	Buffer(
		const vk::raii::PhysicalDevice& physical_device,
		const vk::raii::Device&         device,
		size_t                          count,
		vk::BufferUsageFlags            usage,
		vk::MemoryPropertyFlags         property_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	) : count(count),
	    usage(usage),
	    property_flags(property_flags) {

		assert(count);
		buffer        = std::make_unique<vk::raii::Buffer>(device, vk::BufferCreateInfo({}, sizeof(T) * count, usage));
		device_memory = std::make_unique<vk::raii::DeviceMemory>(
			allocateDeviceMemory(
				device,
				physical_device.getMemoryProperties(),
				buffer->getMemoryRequirements(),
				property_flags
			)
		);

		buffer->bindMemory(**device_memory, 0);
	}

	void upload(const T& data) const {
		assert(property_flags & vk::MemoryPropertyFlagBits::eHostCoherent);
		assert(property_flags & vk::MemoryPropertyFlagBits::eHostVisible);

		void* mapped = device_memory->mapMemory(0, sizeof(T));
		std::memcpy(mapped, &data, sizeof(T));
		device_memory->unmapMemory();
	}

	void upload(const std::vector<T>& data) const {
		assert(property_flags & vk::MemoryPropertyFlagBits::eHostCoherent);
		assert(property_flags & vk::MemoryPropertyFlagBits::eHostVisible);
		assert(data.size() <= count);

		const size_t data_size = data.size() * sizeof(T);
		
		void* mapped = device_memory->mapMemory(0, data_size);
		std::memcpy(mapped, data.data(), data_size);
        device_memory->unmapMemory();
	}

	void upload(
		const vk::raii::PhysicalDevice& physical_device,
		const vk::raii::Device&         device,
		const vk::raii::CommandPool&    command_pool,
		const vk::raii::Queue&          queue,
		const std::vector<T>&           data
	) const {
		assert(usage & vk::BufferUsageFlagBits::eTransferDst);
		assert(property_flags & vk::MemoryPropertyFlagBits::eDeviceLocal);
		assert(data.size() <= count);

		const size_t data_size = data.size() * sizeof(T);

		auto staging_buffer = Buffer<T>(physical_device, device, data.size(), vk::BufferUsageFlagBits::eTransferSrc);
		buffer.upload(data);

		auto cmd_buffers = vk::raii::CommandBuffers(device, {*command_pool, vk::CommandBufferLevel::ePrimary, 1});
		auto& command_buffer = cmd_buffers.front();

		command_buffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
        command_buffer.copyBuffer(**staging_buffer.buffer, **this->buffer, vk::BufferCopy(0, 0, data_size));
        command_buffer.end();

        const auto submit_info = vk::SubmitInfo(nullptr, nullptr, *command_buffer);
        queue.submit(submit_info, nullptr);
        queue.waitIdle();
	}


//private:
	std::unique_ptr<vk::raii::Buffer>       buffer;
	std::unique_ptr<vk::raii::DeviceMemory> device_memory;

	size_t count;
	vk::BufferUsageFlags usage;
	vk::MemoryPropertyFlags property_flags;
};


class Image {
public:
	Image(
		const vk::raii::PhysicalDevice& physical_device,
		const vk::raii::Device&         device,
		vk::Format                      format,
		const vk::Extent2D&             extent,
		vk::ImageTiling                 tiling,
		vk::ImageUsageFlags             usage,
		vk::ImageLayout                 initial_layout,
		vk::MemoryPropertyFlags         memory_properties,
		vk::ImageAspectFlags            aspect_mask
	) : format(format) {
		const auto image_create_info = vk::ImageCreateInfo(
			vk::ImageCreateFlags(),
			vk::ImageType::e2D,
			format,
			vk::Extent3D(extent, 1),
			1,
			1,
			vk::SampleCountFlagBits::e1,
			tiling,
			usage | vk::ImageUsageFlagBits::eSampled,
			vk::SharingMode::eExclusive,
			{},
			initial_layout
		);

		image = std::make_unique<vk::raii::Image>(device, image_create_info);

		device_memory = std::make_unique<vk::raii::DeviceMemory>(
			allocateDeviceMemory(
				device,
				physical_device.getMemoryProperties(),
				image->getMemoryRequirements(),
				memory_properties
			)
		);

		image->bindMemory(**device_memory, 0);

		const auto component_mapping = vk::ComponentMapping(
			vk::ComponentSwizzle::eR,
			vk::ComponentSwizzle::eG,
			vk::ComponentSwizzle::eB,
			vk::ComponentSwizzle::eA
		);
	
		const auto image_subresource_range = vk::ImageSubresourceRange(aspect_mask, 0, 1, 0, 1);
		const auto image_view_create_info = vk::ImageViewCreateInfo(
			{},
			**image,
			vk::ImageViewType::e2D,
			format,
			component_mapping,
			image_subresource_range
		);

		image_view = std::make_unique<vk::raii::ImageView>(device, image_view_create_info);
	}

//private:
	vk::Format                              format;
	std::unique_ptr<vk::raii::Image>        image;
	std::unique_ptr<vk::raii::DeviceMemory> device_memory;
	std::unique_ptr<vk::raii::ImageView>    image_view;
};


class DepthBuffer : public Image {
public:
	DepthBuffer(
		const vk::raii::PhysicalDevice& physical_device,
		const vk::raii::Device&         device,
		vk::Format                      format,
		const vk::Extent2D&             extent
	) : Image(
			physical_device,
			device,
			format,
			extent,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eDepthStencilAttachment,
			vk::ImageLayout::eUndefined,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			vk::ImageAspectFlagBits::eDepth
		) {
	}
};


int main(int argc, char** argv) {
	glfwInit();

	// Window
	auto window = Window(AppName, vk::Extent2D{Width, Height});

	// Vulkan Context
	auto context = VulkanContext(AppName, EngineName, window);

	// Swap Chain
	auto swap_chain = SwapChain(context, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc, {});

	// Command Pool
	auto cmd_buffer_pool = CommandBufferPool(context);

	// Depth Buffer
	auto depth_buffer = DepthBuffer(*context.physical_device, *context.device, vk::Format::eD16Unorm, context.surface->window.size);

	// Model Uniform Buffer
	auto uniform_buffer = Buffer<glm::mat4>(*context.physical_device, *context.device, 1, vk::BufferUsageFlagBits::eUniformBuffer);
    const glm::mat4 mvpc_matrix = vk::su::createModelViewProjectionClipMatrix(context.surface->window.size);
	uniform_buffer.upload(mvpc_matrix);

	// Pipeline Layout
    std::unique_ptr<vk::raii::DescriptorSetLayout> descriptor_set_layout = makeDescriptorSetLayout(*context.device, {{vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex}});
    std::unique_ptr<vk::raii::PipelineLayout> pipelineLayout = makePipelineLayout(*context.device, *descriptor_set_layout);

	// Render Pass
	std::unique_ptr<vk::raii::RenderPass> renderPass = makeRenderPass(*context.device, swap_chain.color_format, depth_buffer.format);

	// Compile Shaders
    glslang::InitializeProcess();
    std::unique_ptr<vk::raii::ShaderModule> vertexShaderModule = vk::raii::su::makeUniqueShaderModule(*context.device, vk::ShaderStageFlagBits::eVertex, vertexShaderText_PC_C);
    std::unique_ptr<vk::raii::ShaderModule> fragmentShaderModule = vk::raii::su::makeUniqueShaderModule(*context.device, vk::ShaderStageFlagBits::eFragment, fragmentShaderText_C_C);
    glslang::FinalizeProcess();

	// Framebuffers
	std::vector<std::unique_ptr<vk::raii::Framebuffer>> framebuffers = makeFramebuffers(*context.device, *renderPass, swap_chain.image_views, depth_buffer.image_view, context.surface->window.size);

	// Vertex Buffer
	//vk::raii::su::BufferData vertexBufferData(*context.physical_device, *device, sizeof(coloredCubeData), vk::BufferUsageFlagBits::eVertexBuffer);
    //vk::raii::su::copyToDevice(*vertexBufferData.deviceMemory, coloredCubeData, sizeof(coloredCubeData) / sizeof(coloredCubeData[0]));
	const auto bufferCreateInfo = vk::BufferCreateInfo({}, sizeof(coloredCubeData), vk::BufferUsageFlagBits::eVertexBuffer);
	std::unique_ptr<vk::raii::Buffer> vertexBuffer = std::make_unique<vk::raii::Buffer>(*context.device, bufferCreateInfo);

	// Allocate memory for the vertex buffer
    const auto memoryRequirements = vertexBuffer->getMemoryRequirements();
    const uint32_t memoryTypeIndex = findMemoryType(
		context.physical_device->getMemoryProperties(),
		memoryRequirements.memoryTypeBits,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);

    const auto memoryAllocateInfo = vk::MemoryAllocateInfo(memoryRequirements.size, memoryTypeIndex);
    std::unique_ptr<vk::raii::DeviceMemory> deviceMemory = std::make_unique<vk::raii::DeviceMemory>(*context.device, memoryAllocateInfo);

	// Copy data into vertex buffer
    uint8_t* pData = static_cast<uint8_t*>(deviceMemory->mapMemory(0, memoryRequirements.size));
    std::memcpy(pData, coloredCubeData, sizeof(coloredCubeData));
    deviceMemory->unmapMemory();

	// Bind device memory to the vertex buffer
	vertexBuffer->bindMemory(**deviceMemory, 0);

	// Descriptor Set
    std::unique_ptr<vk::raii::DescriptorPool> descriptorPool = makeDescriptorPool(*context.device, {{vk::DescriptorType::eUniformBuffer, 1}});
    std::unique_ptr<vk::raii::DescriptorSet> descriptorSet = makeDescriptorSet(*context.device, *descriptorPool, *descriptor_set_layout);
    updateDescriptorSets(*context.device, *descriptorSet, {{vk::DescriptorType::eUniformBuffer, *uniform_buffer.buffer, nullptr}}, {});

	// Pipeline
	std::unique_ptr<vk::raii::PipelineCache> pipelineCache = std::make_unique<vk::raii::PipelineCache>(*context.device, vk::PipelineCacheCreateInfo());
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
	std::unique_ptr<vk::raii::Semaphore> imageAcquiredSemaphore = std::make_unique<vk::raii::Semaphore>(*context.device, vk::SemaphoreCreateInfo());


    vk::Result result;
    uint32_t   imageIndex;
    std::tie(result, imageIndex) = swap_chain.swap_chain->acquireNextImage(vk::su::FenceTimeout, **imageAcquiredSemaphore);
    assert(result == vk::Result::eSuccess);
    assert(imageIndex < swap_chain.images.size());

    std::array<vk::ClearValue, 2> clearValues;
    clearValues[0].color        = vk::ClearColorValue(std::array<float, 4>{0.2f, 0.2f, 0.2f, 0.2f});
    clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

    cmd_buffer_pool.buffer->begin({});

    vk::RenderPassBeginInfo renderPassBeginInfo(**renderPass, **framebuffers[imageIndex], vk::Rect2D(vk::Offset2D(0, 0), context.surface->window.size), clearValues);
    cmd_buffer_pool.buffer->beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
    cmd_buffer_pool.buffer->bindPipeline(vk::PipelineBindPoint::eGraphics, **graphicsPipeline);
    cmd_buffer_pool.buffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, **pipelineLayout, 0, {**descriptorSet}, nullptr);

    cmd_buffer_pool.buffer->bindVertexBuffers(0, {**vertexBuffer}, {0});

    cmd_buffer_pool.buffer->setViewport(
		0,
		vk::Viewport(
			0.0f,
			0.0f,
			static_cast<float>(context.surface->window.size.width),
			static_cast<float>(context.surface->window.size.height),
			0.0f,
			1.0f
		)
	);
    cmd_buffer_pool.buffer->setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), context.surface->window.size));

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
	while (!glfwWindowShouldClose(context.surface->window.handle)) {
		glfwPollEvents();
	}

	glfwTerminate();

	return 0;
}

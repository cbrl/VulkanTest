#pragma once

#include <vulkan/vulkan.hpp>
#include "vulkan/vulkan_raii.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vk_utils.h"

#include <algorithm>
#include <memory>
#include <ranges>
#include <string>
#include <vector>


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


class VulkanContext {
public:
	VulkanContext(const std::string& app_name, const std::string& engine_name, const Window& window) : 
		app_name(app_name),
		engine_name(engine_name),
		window(window) {

		// Context
		context = std::make_unique<vk::raii::Context>();

		// Instance
		makeInstance();

		// Physical Device
		physical_device = std::make_unique<vk::raii::PhysicalDevice>(std::move(vk::raii::PhysicalDevices(*instance).front()));

		// Surface
		makeSurface();

		// Logical Device
		makeDevice();

		// Graphics Queue 
		graphics_queue = std::make_unique<vk::raii::Queue>(*device, graphics_queue_family_idx, 0);

		// Present Queue
		present_queue = std::make_unique<vk::raii::Queue>(*device, present_queue_family_idx, 0);
	}

//private:
	
	auto makeInstance(uint32_t api_version = VK_API_VERSION_1_0) -> void {
		const auto application_info = vk::ApplicationInfo(app_name.c_str(), 1, engine_name.c_str(), 1, api_version);

		const auto enabled_layers     = gatherLayers({}, context->enumerateInstanceLayerProperties());
		const auto enabled_extensions = gatherExtensions(getInstanceExtensions(), context->enumerateInstanceExtensionProperties());

		const auto instance_create_info_chain = makeInstanceCreateInfoChain(application_info, enabled_layers, enabled_extensions);

		instance = std::make_unique<vk::raii::Instance>(*context, instance_create_info_chain.get<vk::InstanceCreateInfo>());
	}

	auto makeDevice(const vk::PhysicalDeviceFeatures* physical_device_features = nullptr) -> void {
		std::tie(graphics_queue_family_idx, present_queue_family_idx) = findGraphicsAndPresentQueueFamilyIndex(*physical_device, *surface);

		const auto enabled_extensions = [] {
			const auto device_extensions = getDeviceExtensions();

			auto extensions = std::vector<const char*>{};
			for (const auto& ext : device_extensions) {
				extensions.push_back(ext.data());
			}
			return extensions;
		}();

		const auto queue_priority = 0.0f;
		const auto device_queue_create_info = vk::DeviceQueueCreateInfo(vk::DeviceQueueCreateFlags(), graphics_queue_family_idx, 1, &queue_priority);
		const auto device_create_info       = vk::DeviceCreateInfo(vk::DeviceCreateFlags(), device_queue_create_info, {}, enabled_extensions, physical_device_features);

		device = std::make_unique<vk::raii::Device>(*physical_device, device_create_info);
	}

	auto makeSurface() -> void {
		auto surface_khr = VkSurfaceKHR{};

		const auto err = glfwCreateWindowSurface(static_cast<VkInstance>(**instance), window.get().handle, nullptr, &surface_khr);
		if (err != VK_SUCCESS) {
			throw std::runtime_error("Failed to create window surface");
		}

		surface = std::make_unique<vk::raii::SurfaceKHR>(*instance, surface_khr);
	}

	static auto getDeviceExtensions() -> std::vector<std::string> {
		return {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	}

	static auto getInstanceExtensions() -> std::vector<std::string> {
		std::vector<std::string> extensions;
		extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
	#if defined(VK_USE_PLATFORM_ANDROID_KHR)
		extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
	#elif defined(VK_USE_PLATFORM_IOS_MVK)
		extensions.push_back(VK_MVK_IOS_SURFACE_EXTENSION_NAME);
	#elif defined(VK_USE_PLATFORM_MACOS_MVK)
		extensions.push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
	#elif defined(VK_USE_PLATFORM_MIR_KHR)
		extensions.push_back(VK_KHR_MIR_SURFACE_EXTENSION_NAME);
	#elif defined(VK_USE_PLATFORM_VI_NN)
		extensions.push_back(VK_NN_VI_SURFACE_EXTENSION_NAME);
	#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
		extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
	#elif defined(VK_USE_PLATFORM_WIN32_KHR)
		extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
	#elif defined(VK_USE_PLATFORM_XCB_KHR)
		extensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
	#elif defined(VK_USE_PLATFORM_XLIB_KHR)
		extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
	#elif defined(VK_USE_PLATFORM_XLIB_XRANDR_EXT)
		extensions.push_back(VK_EXT_ACQUIRE_XLIB_DISPLAY_EXTENSION_NAME);
	#endif
		return extensions;
	}


	static auto gatherLayers(
		const std::vector<std::string>& layers,
		const std::vector<vk::LayerProperties>& layer_properties
	) -> std::vector<const char*> {

		auto enabled_layers = std::vector<const char*>{};
		enabled_layers.reserve(layers.size());

		for (const auto& layer : layers) {
			assert(
				std::ranges::any_of(layer_properties, [layer](const vk::LayerProperties& lp) {
					return layer == lp.layerName;
				})
			);

			enabled_layers.push_back(layer.data());
		}

		#if !defined(NDEBUG)
		// Enable standard validation layer to find as much errors as possible!
		const bool has_validation_layer = std::ranges::find(layers, "VK_LAYER_KHRONOS_validation") != layers.end();

		const bool validation_layer_exists = std::ranges::any_of(layer_properties, [](const vk::LayerProperties& lp) {
			return (strcmp("VK_LAYER_KHRONOS_validation", lp.layerName) == 0);
		});

		if (!has_validation_layer && validation_layer_exists) {
			enabled_layers.push_back("VK_LAYER_KHRONOS_validation");
		}
		#endif

		return enabled_layers;
	}

	static auto gatherExtensions(
		const std::vector<std::string>& extensions,
		const std::vector<vk::ExtensionProperties>& extension_properties
	) -> std::vector<const char*> {

		auto enabled_extensions = std::vector<const char*>{};
		enabled_extensions.reserve(extensions.size());

		for (const auto& ext : extensions) {
			assert(
				std::ranges::any_of(extension_properties, [ext](const vk::ExtensionProperties& ep) {
					return ext == ep.extensionName;
				})
			);

			enabled_extensions.push_back(ext.data());
		}

		#if !defined(NDEBUG)
		const bool has_debug_utils = std::ranges::find(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != extensions.end();

		const bool debug_utils_exist = std::ranges::any_of(extension_properties, [](const vk::ExtensionProperties& ep) {
			return (strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, ep.extensionName) == 0);
		});

		if (!has_debug_utils && debug_utils_exist) {
			enabled_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
		#endif

		return enabled_extensions;
	}

	static auto findGraphicsAndPresentQueueFamilyIndex(
		const vk::raii::PhysicalDevice& physical_device,
		const vk::raii::SurfaceKHR& surface
	) -> std::pair<uint32_t, uint32_t> {

		auto queue_family_properties = physical_device.getQueueFamilyProperties();
		assert(queue_family_properties.size() < std::numeric_limits<uint32_t>::max());

		const uint32_t graphics_queue_family_index = [&] {
			// Get the first index into queueFamiliyProperties which supports graphics
			const auto graphics_queue_family_property = std::ranges::find_if(queue_family_properties, [](const vk::QueueFamilyProperties& qfp) {
				return static_cast<bool>(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
			});

			assert(graphics_queue_family_property != queue_family_properties.end());
			return static_cast<uint32_t>(std::distance(queue_family_properties.begin(), graphics_queue_family_property));
		}();

		if (physical_device.getSurfaceSupportKHR(graphics_queue_family_index, *surface)) {
			// The first graphics_queue_family_index also supports present
			return std::make_pair(graphics_queue_family_index, graphics_queue_family_index);
		}

		// The graphics_queue_family_index doesn't support present. Look for another family index
		// that supports both graphics and present
		for (size_t i = 0; i < queue_family_properties.size(); ++i) {
			const auto graphics_queue  = queue_family_properties[i].queueFlags & vk::QueueFlagBits::eGraphics;
			const auto surface_support = physical_device.getSurfaceSupportKHR(static_cast<uint32_t>(i), *surface);

			if (graphics_queue && surface_support) {
				return std::make_pair(static_cast<uint32_t>(i), static_cast<uint32_t>(i));
			}
		}

		// There's no single family index that supports both graphics and present. Look for another
		// family index that supports present.
		for (size_t i = 0; i < queue_family_properties.size(); ++i) {
			if (physical_device.getSurfaceSupportKHR(static_cast<uint32_t>(i), *surface)) {
				return std::make_pair(graphics_queue_family_index, static_cast<uint32_t>(i));
			}
		}

		throw std::runtime_error("Could not find both graphics and present queues");
	}


	static auto makeInstanceCreateInfoChain(
		const vk::ApplicationInfo& applicationInfo,
		const std::vector<const char*>& layers,
		const std::vector<const char*>& extensions
	) -> 
	#if defined(NDEBUG)
		vk::StructureChain<vk::InstanceCreateInfo>
	#else
		vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT>
	#endif
	{
		// Return type depends on debug/release build
		#if defined(NDEBUG)
		using instance_create_chain = vk::StructureChain<vk::InstanceCreateInfo>;
		#else
		using instance_create_chain = vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT>;
		#endif

		#if defined(NDEBUG)
		// In release mode just use the InstanceCreateInfo for instance creation
		const auto instanceCreateInfo = instance_create_chain(
			{{}, &applicationInfo, layers, extensions}
		);
		#else
		// In debug mode, addionally use the debugUtilsMessengerCallback in instance creation
		const auto severityFlags = vk::DebugUtilsMessageSeverityFlagsEXT(
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
		);

		const auto messageTypeFlags = vk::DebugUtilsMessageTypeFlagsEXT(
			vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral     |
			vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
			vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
		);

		const auto instanceCreateInfo = instance_create_chain(
			{{}, &applicationInfo, layers, extensions},
			{{}, severityFlags, messageTypeFlags, &debugUtilsMessengerCallback}
		);
		#endif

		return instanceCreateInfo;
	}


	std::string app_name;
	std::string engine_name;

	std::reference_wrapper<const Window> window;

	std::unique_ptr<vk::raii::Context> context;
	std::unique_ptr<vk::raii::Instance> instance;
	std::unique_ptr<vk::raii::PhysicalDevice> physical_device;
	std::unique_ptr<vk::raii::SurfaceKHR> surface;
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
		vk::SwapchainKHR old_swap_chain = nullptr
	) {
		const auto surface_capabilities = context.physical_device->getSurfaceCapabilitiesKHR(**context.surface);
		const auto surface_format       = selectSurfaceFormat(context.physical_device->getSurfaceFormatsKHR(**context.surface));
		const auto present_mode         = selectPresentMode(context.physical_device->getSurfacePresentModesKHR(**context.surface));

		color_format = surface_format.format;

		const vk::Extent2D swap_chain_extent = [&] {
			vk::Extent2D extent;

			if (surface_capabilities.currentExtent.width == std::numeric_limits<uint32_t>::max()) {
				// If the surface size is undefined, the size is set to the size of the images requested.
				extent.width  = std::clamp(context.window.get().size.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
				extent.height = std::clamp(context.window.get().size.height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
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
			**context.surface,
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
			old_swap_chain
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

		const auto component_mapping  = vk::ComponentMapping(vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA);
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

	static auto selectSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats) -> vk::SurfaceFormatKHR {
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

	static auto selectPresentMode(const std::vector<vk::PresentModeKHR>& modes) -> vk::PresentModeKHR {
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

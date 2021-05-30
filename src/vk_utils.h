#pragma once

#include <vulkan/vulkan.hpp>
#include "vulkan/vulkan_raii.hpp"

#include <cassert>
#include <map>
#include <memory>
#include <numeric>
#include <ranges>
#include <string>
#include <utility>
#include <vector>


VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessengerCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT       messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT              messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT*  pCallbackData,
	void*                                        pUserData
) {
#if !defined(NDEBUG)
	if (pCallbackData->messageIdNumber == 648835635) {
		// UNASSIGNED-khronos-Validation-debug-build-warning-message
		return VK_FALSE;
	}
	if (pCallbackData->messageIdNumber == 767975156) {
		// UNASSIGNED-BestPractices-vkCreateInstance-specialuse-extension
		return VK_FALSE;
	}
#endif

	std::cerr << vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity)) << ": "
	          << vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageTypes)) << ":\n";
	std::cerr << "\t"
	          << "messageIDName   = <" << pCallbackData->pMessageIdName << ">\n";
	std::cerr << "\t"
	          << "messageIdNumber = " << pCallbackData->messageIdNumber << "\n";
	std::cerr << "\t"
	          << "message         = <" << pCallbackData->pMessage << ">\n";

	if (0 < pCallbackData->queueLabelCount) {
		std::cerr << "\t"
		          << "Queue Labels:\n";
		for (uint8_t i = 0; i < pCallbackData->queueLabelCount; i++) {
			std::cerr << "\t\t"
			          << "labelName = <" << pCallbackData->pQueueLabels[i].pLabelName << ">\n";
		}
	}
	if (0 < pCallbackData->cmdBufLabelCount) {
		std::cerr << "\t"
		          << "CommandBuffer Labels:\n";
		for (uint8_t i = 0; i < pCallbackData->cmdBufLabelCount; i++) {
			std::cerr << "\t\t"
			         << "labelName = <" << pCallbackData->pCmdBufLabels[i].pLabelName << ">\n";
		}
	}
	if (0 < pCallbackData->objectCount) {
		std::cerr << "\t"
		          << "Objects:\n";
		for (uint8_t i = 0; i < pCallbackData->objectCount; i++) {
			std::cerr << "\t\t"
			          << "Object " << i << "\n";
			std::cerr << "\t\t\t"
			          << "objectType   = "
			          << vk::to_string(static_cast<vk::ObjectType>(pCallbackData->pObjects[i].objectType)) << "\n";
			std::cerr << "\t\t\t"
			          << "objectHandle = " << pCallbackData->pObjects[i].objectHandle << "\n";
			if (pCallbackData->pObjects[i].pObjectName) {
				std::cerr << "\t\t\t"
				          << "objectName   = <" << pCallbackData->pObjects[i].pObjectName << ">\n";
			}
		}
	}

	return VK_TRUE;
}


std::vector<std::string> getDeviceExtensions() {
	return {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
}

std::vector<std::string> getInstanceExtensions() {
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


uint32_t findMemoryType(
	const vk::PhysicalDeviceMemoryProperties& memoryProperties,
	uint32_t typeBits,
	vk::MemoryPropertyFlags requirementsMask
) {
	auto typeIndex = static_cast<uint32_t>(~0);

	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
		if ((typeBits & 1) && ((memoryProperties.memoryTypes[i].propertyFlags & requirementsMask ) == requirementsMask)) {
			typeIndex = i;
			break;
		}

		typeBits >>= 1;
	}

	assert(typeIndex != static_cast<uint32_t>(~0));

	return typeIndex;
}

vk::raii::DeviceMemory allocateDeviceMemory(
	const vk::raii::Device&                   device,
	const vk::PhysicalDeviceMemoryProperties& memoryProperties,
	const vk::MemoryRequirements&             memoryRequirements,
	vk::MemoryPropertyFlags                   memoryPropertyFlags
) {
	const uint32_t memoryTypeIndex = findMemoryType(memoryProperties, memoryRequirements.memoryTypeBits, memoryPropertyFlags);
	const auto memoryAllocateInfo = vk::MemoryAllocateInfo(memoryRequirements.size, memoryTypeIndex);
	return vk::raii::DeviceMemory(device, memoryAllocateInfo);
}

std::vector<const char*> gatherLayers(
	const std::vector<std::string>& layers
#if !defined(NDEBUG)
	,
	const std::vector<vk::LayerProperties>& layerProperties
#endif
) {
	std::vector<const char*> enabledLayers;
	enabledLayers.reserve(layers.size());

	for (const auto& layer : layers) {
		assert(
			std::ranges::any_of(layerProperties, [layer](const vk::LayerProperties& lp) {
				return layer == lp.layerName;
			})
		);

		enabledLayers.push_back(layer.data());
	}

#if !defined(NDEBUG)
	// Enable standard validation layer to find as much errors as possible!
	const bool has_validation_layer = std::ranges::find(layers, "VK_LAYER_KHRONOS_validation") != layers.end();

	const bool validation_layer_exists = std::ranges::any_of(layerProperties, [](const vk::LayerProperties& lp) {
		return (strcmp("VK_LAYER_KHRONOS_validation", lp.layerName) == 0);
	});

	if (!has_validation_layer && validation_layer_exists) {
		enabledLayers.push_back("VK_LAYER_KHRONOS_validation");
	}
#endif

	return enabledLayers;
}

std::vector<const char*> gatherExtensions(
	const std::vector<std::string>& extensions
#if !defined(NDEBUG)
	,
	const std::vector<vk::ExtensionProperties>& extensionProperties
#endif
) {
	std::vector<const char*> enabledExtensions;
	enabledExtensions.reserve(extensions.size());

	for (const auto& ext : extensions) {
		assert(
			std::ranges::any_of(extensionProperties, [ext](const vk::ExtensionProperties& ep) {
				return ext == ep.extensionName;
			})
		);

		enabledExtensions.push_back(ext.data());
	}

#if !defined(NDEBUG)
	const bool has_debug_utils = std::ranges::find(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != extensions.end();

	const bool debug_utils_exist = std::ranges::any_of(extensionProperties, [](const vk::ExtensionProperties& ep) {
			return (strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, ep.extensionName) == 0);
	});

	if (!has_debug_utils && debug_utils_exist) {
		enabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
#endif

	return enabledExtensions;
}

auto makeInstanceCreateInfoChain(
	const vk::ApplicationInfo& applicationInfo,
	const std::vector<const char*>& layers,
	const std::vector<const char*>& extensions
) {
#if defined(NDEBUG)
	using instance_create_chain = vk::StructureChain<vk::InstanceCreateInfo>;
#else
	using instance_create_chain = vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT>;
#endif

#if defined(NDEBUG)
	// in non-debug mode just use the InstanceCreateInfo for instance creation
	const auto instanceCreateInfo = instance_create_chain(
		{{}, &applicationInfo, enabledLayers, enabledExtensions}
	);
#else
	// in debug mode, addionally use the debugUtilsMessengerCallback in instance creation!
	const auto severityFlags = vk::DebugUtilsMessageSeverityFlagsEXT(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
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

std::unique_ptr<vk::raii::Instance> makeInstance(
	const vk::raii::Context& context,
	const std::string& app_name,
	const std::string& engine_name,
	const std::vector<std::string>& layers = {},
	const std::vector<std::string>& extensions = {},
	uint32_t api_version = VK_API_VERSION_1_0
) {
	const auto applicationInfo = vk::ApplicationInfo(app_name.c_str(), 1, engine_name.c_str(), 1, api_version);
	std::vector<const char*> enabledLayers = gatherLayers(layers, context.enumerateInstanceLayerProperties());
	std::vector<const char*> enabledExtensions = gatherExtensions(extensions, context.enumerateInstanceExtensionProperties());

	const auto instanceCreateInfoChain = makeInstanceCreateInfoChain(applicationInfo, enabledLayers, enabledExtensions);

	return std::make_unique<vk::raii::Instance>(context, instanceCreateInfoChain.get<vk::InstanceCreateInfo>());
}

std::unique_ptr<vk::raii::PhysicalDevice> makePhysicalDevice(const vk::raii::Instance& instance) {
	return std::make_unique<vk::raii::PhysicalDevice>(
		std::move(vk::raii::PhysicalDevices(instance).front())
	);
}

std::unique_ptr<vk::raii::Device> makeDevice(
	const vk::raii::PhysicalDevice&   physicalDevice,
	uint32_t                          queueFamilyIndex,
	const std::vector<std::string>&   extensions             = {},
	const vk::PhysicalDeviceFeatures* physicalDeviceFeatures = nullptr,
	const void*                       pNext                  = nullptr
) {
	std::vector<char const *> enabledExtensions;
	enabledExtensions.reserve(extensions.size());

	for (const auto& ext : extensions) {
		enabledExtensions.push_back(ext.data());
	}

	float queuePriority = 0.0f;
	auto deviceQueueCreateInfo = vk::DeviceQueueCreateInfo(vk::DeviceQueueCreateFlags(), queueFamilyIndex, 1, &queuePriority);
	auto deviceCreateInfo = vk::DeviceCreateInfo(vk::DeviceCreateFlags(), deviceQueueCreateInfo, {}, enabledExtensions, physicalDeviceFeatures);
	deviceCreateInfo.pNext = pNext;

	return std::make_unique<vk::raii::Device>(physicalDevice, deviceCreateInfo);
}

uint32_t findGraphicsQueueFamilyIndex(const std::vector<vk::QueueFamilyProperties>& queueFamilyProperties) {
	// get the first index into queueFamiliyProperties which supports graphics
	const auto graphicsQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties, [](const vk::QueueFamilyProperties& qfp) {
		return static_cast<bool>(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
	});

	assert(graphicsQueueFamilyProperty != queueFamilyProperties.end());

	return static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
}

std::pair<uint32_t, uint32_t> findGraphicsAndPresentQueueFamilyIndex(const vk::raii::PhysicalDevice& physicalDevice, const vk::raii::SurfaceKHR& surface) {
	std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

	assert(queueFamilyProperties.size() < std::numeric_limits<uint32_t>::max());

	const uint32_t graphicsQueueFamilyIndex = findGraphicsQueueFamilyIndex(queueFamilyProperties);
	if (physicalDevice.getSurfaceSupportKHR(graphicsQueueFamilyIndex, *surface)) {
		return std::make_pair(
		graphicsQueueFamilyIndex,
		graphicsQueueFamilyIndex );  //the first graphicsQueueFamilyIndex does also support presents
	}

	// the graphicsQueueFamilyIndex doesn't support present -> look for an other family index that supports both
	// graphics and present
	for (size_t i = 0; i < queueFamilyProperties.size(); ++i) {
		if ((queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) && physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), *surface)) {
			return std::make_pair(static_cast<uint32_t>(i), static_cast<uint32_t>(i));
		}
	}

	// there's nothing like a single family index that supports both graphics and present -> look for an other
	// family index that supports present
	for (size_t i = 0; i < queueFamilyProperties.size(); ++i) {
		if (physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), *surface)) {
			return std::make_pair(graphicsQueueFamilyIndex, static_cast<uint32_t>(i));
		}
	}

	throw std::runtime_error("Could not find both graphics and present queues");
}
module;

#include <format>
#include <iostream>
#include <ranges>

#include <vulkan/vulkan.hpp>

export module vkw.debug;


export namespace vkw::debug {

VKAPI_ATTR auto VKAPI_CALL debug_utils_messenger_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT             messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void*                                       pUserData
) -> VkBool32 {
	(void)pUserData;

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

	std::cerr << std::format("{}: {}:\n",
		vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity)),
		vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageTypes))
	);
	std::cerr << std::format("\tmessageIDName   = <{}>\n", pCallbackData->pMessageIdName);
	std::cerr << std::format("\tmessageIdNumber = {}\n" , pCallbackData->messageIdNumber);
	std::cerr << std::format("\tmessage         = <{}>\n", pCallbackData->pMessage);

	if (pCallbackData->queueLabelCount > 0) {
		std::cerr << "\tQueue Labels:\n";
		for (uint8_t i = 0; i < pCallbackData->queueLabelCount; i++) {
			std::cerr << std::format("\t\tlabelName = <{}>\n", pCallbackData->pQueueLabels[i].pLabelName);
		}
	}
	if (pCallbackData->cmdBufLabelCount > 0) {
		std::cerr << "\tCommandBuffer Labels:\n";
		for (uint8_t i = 0; i < pCallbackData->cmdBufLabelCount; i++) {
			std::cerr << std::format("\t\tlabelName = <{}>\n", pCallbackData->pCmdBufLabels[i].pLabelName);
		}
	}
	if (pCallbackData->objectCount > 0) {
		std::cerr << "\tObjects:\n";
		for (uint8_t i = 0; i < pCallbackData->objectCount; i++) {
			std::cerr << std::format("\t\tObject {}\n", i);
			std::cerr << std::format("\t\t\tobjectType   = {}\n", vk::to_string(static_cast<vk::ObjectType>(pCallbackData->pObjects[i].objectType)));
			std::cerr << std::format("\t\t\tobjectHandle = {}\n", pCallbackData->pObjects[i].objectHandle);
			if (pCallbackData->pObjects[i].pObjectName) {
				std::cerr << std::format("\t\t\tobjectName   = <{}>\n", pCallbackData->pObjects[i].pObjectName);
			}
		}
	}

	return VK_TRUE;
}


auto validate_layers(const std::vector<const char*>& layers, const std::vector<vk::LayerProperties>& layer_properties) -> void {
	if (layer_properties.empty()) {
		std::cout << "No layer properties" << std::endl;
		throw std::runtime_error{"No layer properties"};
	}

	const auto is_invalid = [&](const char* layer) {
		return std::ranges::all_of(layer_properties, [&](const auto& lp) {
			return strcmp(layer, lp.layerName) != 0;
		});
	};

	bool invalid_layers = false;
	for (const auto& layer : layers | std::views::filter(is_invalid)) {
		if (not invalid_layers) {
			std::cout << "Invalid or unavailable layers: ";
			invalid_layers = true;
		}

		std::cout << layer << ' ';
	}

	if (invalid_layers) {
		std::cout << std::endl;
		throw std::runtime_error{"Invalid layers"};
	}
}


auto validate_extensions(const std::vector<const char*>& extensions, const std::vector<vk::ExtensionProperties>& ext_properties) -> void {
	if (ext_properties.empty()) {
		std::cout << "No extension properties" << std::endl;
		throw std::runtime_error{"No extension properties"};
	}

	const auto is_invalid = [&](const char* extension) {
		return std::ranges::all_of(ext_properties, [&](const auto& ep) {
			return strcmp(extension, ep.extensionName) != 0;
		});
	};

	bool invalid_extensions = false;
	for (const auto& extension : extensions | std::views::filter(is_invalid)) {
		if (not invalid_extensions) {
			std::cout << "Invalid or unavailable extensions: ";
			invalid_extensions = true;
		}

		std::cout << extension << ' ';
	}

	if (invalid_extensions) {
		std::cout << std::endl;
		throw std::runtime_error{"Invalid extensions"};
	}
}

} //namespace vkw::debug

#pragma once

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <string>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "debug.h"


namespace vkw {

namespace util {
auto get_surface_extensions() -> std::vector<const char*> {
	auto extensions = std::vector<const char*>{};
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
} //namespace util

class instance {
public:
	struct app_info {
		std::string app_name = "VulkanApp";
		uint32_t app_version_major = 0;
		uint32_t app_version_minor = 0;
		uint32_t app_vesrion_patch = 0;

		std::string engine_name = "VulkanEngine";
		uint32_t engine_version_major = 0;
		uint32_t engine_version_minor = 0;
		uint32_t engine_vesrion_patch = 0;

		uint32_t api_version = VK_API_VERSION_1_0;
	};

	struct instance_info {
		std::vector<const char*> layers = {};
		std::vector<const char*> extensions = {};
	};

	struct debug_info {
		bool utils = false;
		bool validation = false;
	};

	instance(const app_info& app_config, const instance_info& instance_config, const debug_info& debug_config)
		: context(std::make_unique<vk::raii::Context>())
		, app_config(app_config)
		, instance_config(instance_config)
		, debug_config(debug_config) {

		// Enumerate layer and extension properties
		layer_properties     = context->enumerateInstanceLayerProperties();
		extension_properties = context->enumerateInstanceExtensionProperties();

		// Validate the extensions/layers, and add requested debug extensions/layers.
		validate_instance_info();

		// Create the instance
		create_instance();

		// Enumerate physical devices
		physical_devices = vk::raii::PhysicalDevices(*vk_instance);
	}


	[[nodiscard]]
	auto get_app_info() const noexcept -> const app_info& {
		return app_config;
	}

	[[nodiscard]]
	auto get_instance_info() const noexcept -> const instance_info& {
		return instance_config;
	}

	[[nodiscard]]
	auto get_debug_info() const noexcept -> const debug_info& {
		return debug_config;
	}

	[[nodiscard]]
	auto get_vk_instance() -> vk::raii::Instance& {
		return *vk_instance;
	}

	[[nodiscard]]
	auto get_vk_instance() const -> const vk::raii::Instance& {
		return *vk_instance;
	}

	[[nodiscard]]
	auto get_layer_properties() const noexcept -> const std::vector<vk::LayerProperties>& {
		return layer_properties;
	}

	[[nodiscard]]
	auto get_extension_properties() const noexcept -> const std::vector<vk::ExtensionProperties>& {
		return extension_properties;
	}

	[[nodiscard]]
	auto get_physical_device(uint32_t idx) -> vk::raii::PhysicalDevice& {
		return physical_devices.at(idx);
	}

	[[nodiscard]]
	auto get_physical_device(uint32_t idx) const -> const vk::raii::PhysicalDevice& {
		return physical_devices.at(idx);
	}

	[[nodiscard]]
	auto get_physical_devices() noexcept -> std::vector<vk::raii::PhysicalDevice>& {
		return physical_devices;
	}

	[[nodiscard]]
	auto get_physical_devices() const noexcept -> const std::vector<vk::raii::PhysicalDevice>& {
		return physical_devices;
	}

private:

	auto validate_instance_info() -> void {
		// Add the debug utils layer if requested
		if (debug_config.utils) {
			const bool has_debug_utils = std::ranges::any_of(instance_config.extensions, [](const char* ext) {
				return strcmp(ext, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0;
			});

			const bool debug_utils_exist = std::ranges::any_of(extension_properties, [](const vk::ExtensionProperties& ep) {
				return strcmp(ep.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0;
			});

			if (!has_debug_utils && debug_utils_exist) {
				instance_config.extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			}
		}

		// Add the validation layer if requested
		if (debug_config.validation) {
			const auto has_validation_layer = std::ranges::any_of(instance_config.layers, [](const char* layer) {
				return strcmp(layer, "VK_LAYER_KHRONOS_validation") == 0;
			});

			const auto validation_layer_exists = std::ranges::any_of(layer_properties, [](const vk::LayerProperties& lp) {
				return strcmp(lp.layerName, "VK_LAYER_KHRONOS_validation");
			});

			if (!has_validation_layer && validation_layer_exists) {
				instance_config.layers.push_back("VK_LAYER_KHRONOS_validation");
			}
		}

		// Ensure all specified layers and extensions are available
		debug::validate_layers(instance_config.layers, layer_properties);
		debug::validate_extensions(instance_config.extensions, extension_properties);
	}

	auto create_instance() -> void {
		// Build app/engine versions
		const auto app_version    = VK_MAKE_VERSION(app_config.app_version_major, app_config.app_version_minor, app_config.app_vesrion_patch);
		const auto engine_version = VK_MAKE_VERSION(app_config.engine_version_major, app_config.engine_version_minor, app_config.engine_vesrion_patch);


		// Application info struct
		const auto application_info = vk::ApplicationInfo(
			app_config.app_name.c_str(),
			app_version,
			app_config.engine_name.c_str(),
			engine_version,
			app_config.api_version
		);

		if (debug_config.utils) {
			const auto severity_flags = vk::DebugUtilsMessageSeverityFlagsEXT(
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
			);

			const auto message_type_flags = vk::DebugUtilsMessageTypeFlagsEXT(
				vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral     |
				vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
				vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
			);

			// Instance create info structure chain
			const auto instance_create_info = vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT>(
				{{}, &application_info, instance_config.layers, instance_config.extensions},
				{{}, severity_flags, message_type_flags, &debug::debug_utils_messenger_callback}
			);

			// Create the instance
			vk_instance = std::make_unique<vk::raii::Instance>(*context, instance_create_info.get<vk::InstanceCreateInfo>());
		}
		else {
			const auto instance_create_info = vk::StructureChain<vk::InstanceCreateInfo>(
				{{}, &application_info, instance_config.layers, instance_config.extensions}
			);
			
			vk_instance = std::make_unique<vk::raii::Instance>(*context, instance_create_info.get<vk::InstanceCreateInfo>());
		}
	}


	app_info app_config;
	instance_info instance_config;
	debug_info debug_config;

	std::unique_ptr<vk::raii::Context>  context;
	std::unique_ptr<vk::raii::Instance> vk_instance;

	std::vector<vk::LayerProperties> layer_properties;
	std::vector<vk::ExtensionProperties> extension_properties;

	std::vector<vk::raii::PhysicalDevice> physical_devices;
};

} //namespace vkw

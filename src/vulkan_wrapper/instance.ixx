module;

#include <algorithm>
#include <cstdint>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module vkw.instance;

import vkw.debug;
import vkw.util;


namespace vkw {

namespace util {
export auto get_surface_extensions() -> std::vector<const char*> {
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


export struct app_info {
	std::string app_name = "VulkanApp";
	uint32_t app_version_major = 0;
	uint32_t app_version_minor = 0;
	uint32_t app_vesrion_patch = 0;

	std::string engine_name = "VulkanEngine";
	uint32_t engine_version_major = 0;
	uint32_t engine_version_minor = 0;
	uint32_t engine_vesrion_patch = 0;

	uint32_t api_version = VK_API_VERSION_1_2;
};

export struct instance_info {
	std::vector<const char*> layers = {};
	std::vector<const char*> extensions = {};
};

export struct debug_info {
	bool utils = false;
	bool validation = false;
};


// Add any required layers or extensions
[[nodiscard]]
auto process_config(const vk::raii::Context& context, const instance_info& instance_config, const debug_info& debug_config) -> instance_info {
	auto output = instance_config;

	const auto layer_properties = context.enumerateInstanceLayerProperties();
	const auto extension_properties = context.enumerateInstanceExtensionProperties();

	// Add the debug utils layer if requested
	if (debug_config.utils) {
		const bool has_debug_utils = std::ranges::any_of(instance_config.extensions, [](const char* ext) {
			return strcmp(ext, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0;
		});

		const bool debug_utils_exist = std::ranges::any_of(extension_properties, [](const vk::ExtensionProperties& ep) {
			return strcmp(ep.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0;
		});

		if (!has_debug_utils && debug_utils_exist) {
			output.extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
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
			output.layers.push_back("VK_LAYER_KHRONOS_validation");
		}
	}

	return output;
}

auto validate_instance_info(const vk::raii::Context& context, const instance_info& instance_config) -> void {
	const auto layer_properties = context.enumerateInstanceLayerProperties();
	const auto extension_properties = context.enumerateInstanceExtensionProperties();

	// Ensure all specified layers and extensions are available
	debug::validate_layers(instance_config.layers, layer_properties);
	debug::validate_extensions(instance_config.extensions, extension_properties);
}

[[nodiscard]]
auto create_instance(
	const vk::raii::Context& context,
	const app_info& app_config,
	const instance_info& instance_config,
	const debug_info& debug_config
) -> vk::raii::Instance {

	// Validate the extensions/layers, and add requested debug extensions/layers.
	validate_instance_info(context, instance_config);

	// Build app/engine versions
	const auto app_version = VK_MAKE_VERSION(app_config.app_version_major, app_config.app_version_minor, app_config.app_vesrion_patch);
	const auto engine_version = VK_MAKE_VERSION(app_config.engine_version_major, app_config.engine_version_minor, app_config.engine_vesrion_patch);

	// Application info struct
	const auto application_info = vk::ApplicationInfo{
		app_config.app_name.c_str(),
		app_version,
		app_config.engine_name.c_str(),
		engine_version,
		app_config.api_version
	};

	if (debug_config.utils) {
		const auto severity_flags = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
			| vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;

		const auto message_type_flags = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
			| vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
			| vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;

		// Instance create info structure chain
		const auto instance_create_info = vk::StructureChain{
			vk::InstanceCreateInfo{
				vk::InstanceCreateFlags{},
				&application_info,
				instance_config.layers,
				instance_config.extensions
			},
			vk::DebugUtilsMessengerCreateInfoEXT{
				vk::DebugUtilsMessengerCreateFlagsEXT{},
				severity_flags,
				message_type_flags,
				&debug::debug_utils_messenger_callback
			}
		};

		// Create the instance
		return vk::raii::Instance{context, instance_create_info.get<vk::InstanceCreateInfo>()};
	}
	else {
		const auto instance_create_info = vk::InstanceCreateInfo{
			vk::InstanceCreateFlags{},
			&application_info,
			instance_config.layers,
			instance_config.extensions
		};

		return vk::raii::Instance{context, instance_create_info};
	}
}


export class instance : public std::enable_shared_from_this<instance> {
public:
	[[nodiscard]]
	static auto create(const app_info& app_cfg, const instance_info& instance_cfg, const debug_info& debug_cfg) -> std::shared_ptr<instance> {
		return std::make_shared<instance>(app_cfg, instance_cfg, debug_cfg);
	}

	instance(const app_info& app_cfg, const instance_info& instance_cfg, const debug_info& debug_cfg) :
		app_config(app_cfg),
		instance_config(process_config(vk_context, instance_cfg, debug_cfg)),
		debug_config(debug_cfg),
		vk_instance(create_instance(vk_context, app_config, instance_config, debug_config)) {

		// Enumerate layer and extension properties
		layer_properties     = vk_context.enumerateInstanceLayerProperties();
		extension_properties = vk_context.enumerateInstanceExtensionProperties();

		// Enumerate physical devices
		physical_devices = vk::raii::PhysicalDevices{vk_instance};
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
		return vk_instance;
	}

	[[nodiscard]]
	auto get_vk_instance() const -> const vk::raii::Instance& {
		return vk_instance;
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
	auto get_physical_device(uint32_t idx) -> std::shared_ptr<vk::raii::PhysicalDevice> {
		return std::shared_ptr<vk::raii::PhysicalDevice>(shared_from_this(), &physical_devices.at(idx));
	}

	[[nodiscard]]
	auto get_physical_devices() const noexcept -> std::vector<std::shared_ptr<vk::raii::PhysicalDevice>> {
		return vkw::util::to_vector(std::views::transform(physical_devices, [this](const auto& device) {
			return std::shared_ptr<vk::raii::PhysicalDevice>(shared_from_this(), const_cast<vk::raii::PhysicalDevice*>(&device));
		}));
	}

private:

	// Vulkan function dispatcher
	vk::raii::Context  vk_context;

	// Configuration structs
	app_info      app_config;
	instance_info instance_config;
	debug_info    debug_config;

	// The Vulkan instance handle
	vk::raii::Instance vk_instance;

	// Layer & extension properties
	std::vector<vk::LayerProperties>     layer_properties;
	std::vector<vk::ExtensionProperties> extension_properties;

	std::vector<vk::raii::PhysicalDevice> physical_devices;
};

} //namespace vkw

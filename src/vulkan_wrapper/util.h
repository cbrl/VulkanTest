#pragma once

#include <bit>
#include <optional>
#include <ranges>
#include <vector>

#include <vulkan/vulkan.hpp>


namespace vkw::util {

[[nodiscard]]
inline auto contains_property(const std::vector<vk::ExtensionProperties>& extension_properties, const char* extension) -> bool {
	return std::ranges::any_of(extension_properties, [&](const auto& prop) {
		return strcmp(extension, prop.extensionName.data()) == 0;
	});
}

[[nodiscard]]
inline auto contains_property(const std::vector<vk::LayerProperties>& layer_properties, const char* layer) -> bool {
	return std::ranges::any_of(layer_properties, [&](const auto& prop) {
		return strcmp(layer, prop.layerName.data()) == 0;
	});
}

template<typename BitType>
[[nodiscard]]
auto separate_flags(vk::Flags<BitType> flags) -> std::vector<BitType> {
	auto mask = static_cast<vk::Flags<BitType>::MaskType>(flags);

	auto result = std::vector<BitType>{};
	result.reserve(std::popcount(mask));

	for (size_t bit = 0; bit < (sizeof(mask) * 8); ++bit) {
		if (mask & (1 << bit)) {
			result.push_back(static_cast<BitType>(1 << bit));
		}
	}

	return result;
}


[[nodiscard]]
inline auto select_srgb_surface_format(const std::vector<vk::SurfaceFormatKHR>& formats) -> std::optional<vk::SurfaceFormatKHR> {
	// Priority list of formats to look for
	static const vk::Format desired_formats[] = {
		vk::Format::eB8G8R8A8Srgb,
		vk::Format::eR8G8B8A8Srgb,
		vk::Format::eB8G8R8Srgb,
		vk::Format::eR8G8B8Srgb,
	};

	// Look for a desired format that's in the SRGB color space
	for (const auto& format : desired_formats) {
		const auto it = std::ranges::find_if(formats, [format](const vk::SurfaceFormatKHR& f) {
			return (f.format == format) && (f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear);
			});

		if (it != formats.end()) {
			return *it;
		}
	}

	return {};
}

[[nodiscard]]
inline auto select_unorm_surface_format(const std::vector<vk::SurfaceFormatKHR>& formats) -> std::optional<vk::SurfaceFormatKHR> {
	// Priority list of formats to look for
	static const vk::Format desired_formats[] = {
		vk::Format::eB8G8R8A8Unorm,
		vk::Format::eR8G8B8A8Unorm,
		vk::Format::eB8G8R8Unorm,
		vk::Format::eR8G8B8Unorm,
	};

	// Look for a desired format that's in the SRGB color space
	for (const auto& format : desired_formats) {
		const auto it = std::ranges::find_if(formats, [format](const vk::SurfaceFormatKHR& f) {
			return (f.format == format) && (f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear);
		});

		if (it != formats.end()) {
			return *it;
		}
	}

	return {};
}

} //namespace vkw::util

#pragma once

#include <bit>
#include <ranges>
#include <vector>

#include <vulkan/vulkan.hpp>


namespace vkw::util {

auto containsProperty(const std::vector<vk::ExtensionProperties>& extension_properties, const char* extension) -> bool {
	return std::ranges::any_of(extension_properties, [&](const auto& prop) {
		return strcmp(extension, prop.extensionName.data()) == 0;
	});
}

auto containsProperty(const std::vector<vk::LayerProperties>& layer_properties, const char* layer) -> bool {
	return std::ranges::any_of(layer_properties, [&](const auto& prop) {
		return strcmp(layer, prop.layerName.data()) == 0;
	});
}

template<typename BitType>
[[nodiscard]]
auto separateFlags(vk::Flags<BitType> flags) -> std::vector<BitType> {
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

} //namespace vkw::util

module;

#include <bit>
#include <concepts>
#include <memory>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

#include <vulkan/vulkan.hpp>

export module vkw.util;

template<typename T>
concept smart_pointer = requires {
	requires
		requires { typename T::element_type; }
		&& requires(T v) { v.get(); std::is_pointer_v<decltype(v.get())>; }
		&& requires(T v) { *v; std::same_as<std::remove_cvref_t<decltype(*v)>, std::remove_cvref_t<typename T::element_type>>; };
};

template<typename T>
concept raw_pointer = requires(T v) {
	requires std::is_pointer_v<T>;
	std::same_as<std::remove_cvref_t<decltype(*v)>, std::remove_cvref_t<typename std::pointer_traits<T>::element_type>>;
};

template<typename T>
concept pointer = raw_pointer<T> || smart_pointer<T>;

export namespace vkw::util {

/// Converts a range to a vector
template <std::ranges::range R>
[[nodiscard]]
constexpr auto to_vector(R&& r) -> std::vector<std::ranges::range_value_t<R>> {
	auto result = std::vector<std::ranges::range_value_t<R>>{};

	// Reserve space if possible
	if constexpr (std::ranges::sized_range<R>) {
		result.reserve(std::ranges::size(r));
	}

	std::ranges::copy(r, std::back_inserter(result));

	return result;
}

/// Constructs a transform_view that views a range of vk::raii::X objects as their vk::X handle. Works on ranges of value, pointer-like, or reference_wrapper-like types.
template<std::ranges::input_range V>
[[nodiscard]]
auto as_handles(V&& v) {
	using value_type = std::ranges::range_value_t<V>;

	constexpr auto pointer_like = pointer<value_type>;

	constexpr auto ref_wrapper_like = requires(value_type v) {
		typename value_type::type;
		v.get();
		std::same_as<std::remove_cvref_t<decltype(v.get())>, std::remove_cvref_t<typename value_type::type>>;
	};

	if constexpr (pointer_like) {
		using element_type = std::pointer_traits<value_type>::element_type;
		return std::views::transform(std::views::transform(v, [](auto&& n) { return *n; }), &element_type::operator*);
	}
	else if constexpr (ref_wrapper_like) {
		using element_type = typename value_type::type;
		return std::views::transform(std::views::transform(v, &value_type::get), &element_type::operator*);
	}
	else {
		return std::views::transform(v, &value_type::operator*);
	}
};


[[nodiscard]]
auto contains_property(const std::vector<vk::ExtensionProperties>& extension_properties, const char* extension) -> bool {
	return std::ranges::any_of(extension_properties, [&](const auto& prop) {
		return strcmp(extension, prop.extensionName.data()) == 0;
	});
}

[[nodiscard]]
auto contains_property(const std::vector<vk::LayerProperties>& layer_properties, const char* layer) -> bool {
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
auto select_srgb_surface_format(const std::vector<vk::SurfaceFormatKHR>& formats) -> std::optional<vk::SurfaceFormatKHR> {
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
auto select_unorm_surface_format(const std::vector<vk::SurfaceFormatKHR>& formats) -> std::optional<vk::SurfaceFormatKHR> {
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


[[nodiscard]]
auto find_memory_type(const vk::PhysicalDeviceMemoryProperties& memory_properties, uint32_t memory_type_bits, vk::MemoryPropertyFlags property_flags) -> uint32_t {
	auto type_index = std::numeric_limits<uint32_t>::max();

	for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
		if ((memory_type_bits & 1) && ((memory_properties.memoryTypes[i].propertyFlags & property_flags) == property_flags)) {
			type_index = i;
			break;
		}

		memory_type_bits >>= 1;
	}

	assert(type_index != std::numeric_limits<uint32_t>::max());
	return type_index;
}

} //namespace vkw::util

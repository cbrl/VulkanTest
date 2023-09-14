module;

#include <array>
#include <bit>
#include <concepts>
#include <memory>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module vkw.util;

export import vkw.ranges;


template<typename T>
concept smart_pointer = requires { typename T::element_type; }
	&& requires(T v) { v.get(); requires std::is_pointer_v<decltype(v.get())>; }
	&& requires(T v) { *v; requires std::same_as<std::remove_cvref_t<decltype(*v)>, std::remove_cvref_t<typename T::element_type>>; };

template<typename T>
concept raw_pointer = requires(T v) {
	requires std::is_pointer_v<T>;
	requires std::same_as<std::remove_cvref_t<decltype(*v)>, std::remove_cvref_t<typename std::pointer_traits<T>::element_type>>;
};

template<typename T>
concept pointer = raw_pointer<T> || smart_pointer<T>;

template<typename T>
concept ref_wrapper = requires(T v) {
	typename decltype(v)::type;
	v.get();
	requires std::is_reference_v<decltype(v.get())>;
	requires std::same_as<std::remove_cvref_t<decltype(v.get())>, std::remove_cvref_t<typename decltype(v)::type>>;
};

template<typename T>
concept vkw_handle_type = requires(T v) { v.get_vk_handle(); };

export namespace vkw::util {
template<typename T>
[[nodiscard]]
inline auto as_raii_handle(const T& v) -> decltype(auto) {
	if constexpr (pointer<T>) {
		static_assert(requires(T v) { v->get_vk_handle(); }, "Not a vkw type which supports T::get_vk_handle()");
		return v->get_vk_handle();
	}
	else if constexpr (ref_wrapper<T>) {
		static_assert(requires(T v) { v.get().get_vk_handle(); }, "Not a vkw type which supports T::get_vk_handle()");
		return v.get().get_vk_handle();
	}
	else {
		static_assert(requires(T v) { v.get_vk_handle(); }, "Not a vkw type which supports T::get_vk_handle()");
		return v.get_vk_handle();
	}
}

/// Convert a vk::raii::X object to its vk::X handle. Works on ranges of value, pointer-like, or reference_wrapper-like types.
template<typename T>
[[nodiscard]]
inline auto as_handle(const T& v) {
	if constexpr (pointer<T>) {
		if constexpr (vkw_handle_type<std::remove_cvref_t<decltype(*v)>>) {
			return *as_raii_handle(v);
		}
		else {
			return **v;
		}
	}
	else if constexpr (ref_wrapper<T>) {
		if constexpr (vkw_handle_type<std::remove_cvref_t<decltype(v.get())>>) {
			return *as_raii_handle(v);
		}
		else {
			return *v.get();
		}
	}
	else {
		if constexpr (vkw_handle_type<T>) {
			return *as_raii_handle(v);
		}
		else {
			return *v;
		}
	}
}


[[nodiscard]]
inline auto as_handles() {
	return std::views::transform([](auto&& n) { return as_handle(n); });
}

[[nodiscard]]
inline auto as_raii_handles() {
	return std::views::transform([](auto&& n) { return as_raii_handle(n); });
}

template<std::ranges::viewable_range R>
[[nodiscard]]
inline auto as_handles(R&& r) {
	return std::views::transform(std::forward<R>(r), as_handle<std::ranges::range_value_t<R>>);
}

template<std::ranges::viewable_range R>
[[nodiscard]]
inline auto as_raii_handles(R&& r) {
	return std::views::transform(std::forward<R>(r), as_raii_handle<std::ranges::range_value_t<R>>);
}


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
auto select_depth_format(const vk::raii::PhysicalDevice& device) -> std::optional<vk::Format> {
	static constexpr auto desired_formats = std::array{
		vk::Format::eD32Sfloat,
		vk::Format::eD16Unorm
	};

	for (const auto& format : desired_formats) {
		const auto properties = device.getFormatProperties(format);

		if (properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
			return format;
		}
	}

	return std::nullopt;
}

[[nodiscard]]
auto select_depth_stencil_format(const vk::raii::PhysicalDevice& device) -> std::optional<vk::Format> {
	static constexpr auto desired_formats = std::array{
		vk::Format::eD32SfloatS8Uint,
		vk::Format::eD24UnormS8Uint
	};

	for (const auto& format : desired_formats) {
		const auto properties = device.getFormatProperties(format);

		if (properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
			return format;
		}
	}

	return std::nullopt;
}

[[nodiscard]]
auto select_srgb_surface_format(const std::vector<vk::SurfaceFormatKHR>& formats) -> std::optional<vk::SurfaceFormatKHR> {
	// Priority list of formats to look for
	static constexpr auto desired_formats = std::array{
		vk::Format::eB8G8R8A8Srgb,
		vk::Format::eR8G8B8A8Srgb,
		vk::Format::eB8G8R8Srgb,
		vk::Format::eR8G8B8Srgb
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

	return std::nullopt;
}

[[nodiscard]]
auto select_unorm_surface_format(const std::vector<vk::SurfaceFormatKHR>& formats) -> std::optional<vk::SurfaceFormatKHR> {
	// Priority list of formats to look for
	static constexpr auto desired_formats = std::array{
		vk::Format::eB8G8R8A8Unorm,
		vk::Format::eR8G8B8A8Unorm,
		vk::Format::eB8G8R8Unorm,
		vk::Format::eR8G8B8Unorm
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

	return std::nullopt;
}


[[nodiscard]]
auto find_memory_type(const vk::PhysicalDeviceMemoryProperties& memory_properties, uint32_t memory_type_bits, vk::MemoryPropertyFlags property_flags) -> uint32_t {
	auto type_index = std::numeric_limits<uint32_t>::max();

	for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
		if ((memory_type_bits & (1 << i)) && ((memory_properties.memoryTypes[i].propertyFlags & property_flags) == property_flags)) {
			type_index = i;
			break;
		}
	}

	assert(type_index != std::numeric_limits<uint32_t>::max());
	return type_index;
}

} //namespace vkw::util

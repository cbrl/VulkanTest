#pragma once

#include <limits>
#include <span>

#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "logical_device.h"
#include "util.h"

namespace vkw {

class image {
public:
	image(
		const logical_device&   device,
		vk::ImageType           type,
		vk::ImageViewType       view_type,
		vk::Format              format,
		vk::Extent3D            extent,
		vk::ImageTiling         tiling,
		vk::ImageUsageFlags     usage,
		vk::ImageLayout         initial_layout,
		vk::MemoryPropertyFlags memory_properties,
		vk::ImageAspectFlags    aspect_mask
	) :
		type(type),
		view_type(view_type),
		format(format),
		extent(extent),
		vk_image(create_image(device, type, format, extent, tiling, usage, initial_layout)),
		device_memory(create_memory(device, vk_image, memory_properties)),
		image_view(create_view(device, vk_image, view_type, format, aspect_mask)) {

	}

	[[nodiscard]]
	auto get_type() const noexcept -> vk::ImageType {
		return type;
	}

	[[nodiscard]]
	auto get_view_type() const noexcept -> vk::ImageViewType {
		return view_type;
	}

	[[nodiscard]]
	auto get_format() const noexcept -> vk::Format {
		return format;
	}

	[[nodiscard]]
	auto get_extent() const noexcept -> vk::Extent3D {
		return extent;
	}

	[[nodiscard]]
	auto get_vk_image() const noexcept -> const vk::raii::Image& {
		return vk_image;
	}

	[[nodiscard]]
	auto get_vk_image_view() const noexcept -> const vk::raii::ImageView& {
		return image_view;
	}

private:

	static auto create_image(
		const logical_device& device,
		vk::ImageType         type,
		vk::Format            format,
		vk::Extent3D          extent,
		vk::ImageTiling       tiling,
		vk::ImageUsageFlags   usage,
		vk::ImageLayout       initial_layout
	) -> vk::raii::Image {
		const auto image_create_info = vk::ImageCreateInfo{
			vk::ImageCreateFlags{},
			type,
			format,
			extent,
			1,
			1,
			vk::SampleCountFlagBits::e1,
			tiling,
			usage | vk::ImageUsageFlagBits::eSampled,
			vk::SharingMode::eExclusive,
			{},
			initial_layout
		};

		return vk::raii::Image{device.get_vk_device(), image_create_info};
	}

	static auto create_memory(
		const logical_device&   device,
		const vk::raii::Image&  vk_image,
		vk::MemoryPropertyFlags memory_properties
	) -> vk::raii::DeviceMemory {
		auto memory = device.create_device_memory(vk_image.getMemoryRequirements(), memory_properties);
		vk_image.bindMemory(*memory, 0);
		return memory;
	}

	static auto create_view(
		const logical_device&  device,
		const vk::raii::Image& vk_image,
		vk::ImageViewType      type,
		vk::Format             format,
		vk::ImageAspectFlags   aspect_mask
	) -> vk::raii::ImageView {
		const auto component_mapping = vk::ComponentMapping{
			vk::ComponentSwizzle::eR,
			vk::ComponentSwizzle::eG,
			vk::ComponentSwizzle::eB,
			vk::ComponentSwizzle::eA
		};
	
		const auto image_subresource_range = vk::ImageSubresourceRange{aspect_mask, 0, 1, 0, 1};
		const auto image_view_create_info  = vk::ImageViewCreateInfo{
			vk::ImageViewCreateFlags{},
			*vk_image,
			type,
			format,
			component_mapping,
			image_subresource_range
		};

		return vk::raii::ImageView{device.get_vk_device(), image_view_create_info};
	}


	vk::ImageType     type;
	vk::ImageViewType view_type;
	vk::Format        format;
	vk::Extent3D      extent;

	vk::raii::Image        vk_image;
	vk::raii::DeviceMemory device_memory;
	vk::raii::ImageView    image_view;
};


[[nodiscard]]
inline auto create_depth_buffer(const vkw::logical_device& device, vk::Format format, const vk::Extent2D& extent) -> image {
	return image{
		device,
		vk::ImageType::e2D,
		vk::ImageViewType::e2D,
		format,
		vk::Extent3D{extent, 1},
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eDepthStencilAttachment,
		vk::ImageLayout::eUndefined,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		vk::ImageAspectFlagBits::eDepth
	};
}

} //namespace vkw

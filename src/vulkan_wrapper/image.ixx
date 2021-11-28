module;

#include <span>
#include <vulkan/vulkan_raii.hpp>

export module vkw.image;

import vkw.logical_device;
import vkw.util;


export namespace vkw {

class image {
public:
	struct image_info {
		vk::ImageType           type              = vk::ImageType::e2D;
		vk::Format              format            = vk::Format::eR8G8B8A8Srgb;
		vk::Extent3D            extent;
		vk::ImageTiling         tiling            = vk::ImageTiling::eOptimal;
		vk::ImageUsageFlags     usage             = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
		vk::ImageLayout         initial_layout    = vk::ImageLayout::eUndefined;
		vk::MemoryPropertyFlags memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
		vk::ImageViewType       view_type         = vk::ImageViewType::e2D;
		vk::ComponentMapping    component_mapping = vk::ComponentMapping{vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA};
		vk::ImageAspectFlags    aspect_flags      = vk::ImageAspectFlagBits::eColor;
	};

	image(const logical_device& device, const image_info& info) :
		info(info),
		vk_image(create_image(device, info)),
		device_memory(create_memory(device, vk_image, info.memory_properties)),
		image_view(create_view(device, vk_image, info)) {

	}

	[[nodiscard]]
	auto get_info() const noexcept -> const image_info& {
		return info;
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

	static auto create_image(const logical_device& device, const image_info& info) -> vk::raii::Image {
		const auto image_create_info = vk::ImageCreateInfo{
			vk::ImageCreateFlags{},
			info.type,
			info.format,
			info.extent,
			1,
			1,
			vk::SampleCountFlagBits::e1,
			info.tiling,
			info.usage,
			vk::SharingMode::eExclusive,
			{},
			info.initial_layout
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
		const image_info&      info
	) -> vk::raii::ImageView {	
		const auto image_subresource_range = vk::ImageSubresourceRange{info.aspect_flags, 0, 1, 0, 1};
		const auto image_view_create_info  = vk::ImageViewCreateInfo{
			vk::ImageViewCreateFlags{},
			*vk_image,
			info.view_type,
			info.format,
			info.component_mapping,
			image_subresource_range
		};

		return vk::raii::ImageView{device.get_vk_device(), image_view_create_info};
	}


	image_info info;

	vk::raii::Image        vk_image;
	vk::raii::DeviceMemory device_memory;
	vk::raii::ImageView    image_view;
};


[[nodiscard]]
inline auto create_depth_buffer(const vkw::logical_device& device, vk::Format format, const vk::Extent2D& extent) -> image {
	return image{
		device,
		image::image_info{
			.format = format,
			.extent = vk::Extent3D{extent, 1},
			.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
			.memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
			.aspect_flags = vk::ImageAspectFlagBits::eDepth
		}
	};
}

} //namespace vkw

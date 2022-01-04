module;

#include <span>
#include <tuple>

#include <vulkan/vulkan_raii.hpp>

export module vkw.image;

import vkw.logical_device;
import vkw.util;


export namespace vkw {

struct image_info {
	vk::ImageCreateFlags      flags             = vk::ImageCreateFlags{};
	vk::ImageType             type              = vk::ImageType::e2D;
	vk::Format                format            = vk::Format::eR8G8B8A8Srgb;
	vk::Extent3D              extent;
	vk::ImageTiling           tiling            = vk::ImageTiling::eOptimal;
	vk::ImageUsageFlags       usage             = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
	vk::ImageLayout           initial_layout    = vk::ImageLayout::eUndefined;
	vk::MemoryPropertyFlags   memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
	vk::ImageViewType         view_type         = vk::ImageViewType::e2D;
	vk::ComponentMapping      component_mapping = {vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA};
	vk::ImageSubresourceRange subresource_range = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
};

class image {
public:
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

	[[nodiscard]]
	auto get_device_memory() const noexcept -> const vk::raii::DeviceMemory& {
		return device_memory;
	}

private:

	static auto create_image(const logical_device& device, const image_info& info) -> vk::raii::Image {
		const auto image_create_info = vk::ImageCreateInfo{
			info.flags,
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
		const auto image_view_create_info  = vk::ImageViewCreateInfo{
			vk::ImageViewCreateFlags{},
			*vk_image,
			info.view_type,
			info.format,
			info.component_mapping,
			info.subresource_range
		};

		return vk::raii::ImageView{device.get_vk_device(), image_view_create_info};
	}


	image_info info;

	vk::raii::Image        vk_image;
	vk::raii::DeviceMemory device_memory;
	vk::raii::ImageView    image_view;
};


namespace util {

[[nodiscard]]
auto create_depth_buffer(const vkw::logical_device& device, vk::Format format, const vk::Extent2D& extent) -> image {
	return image{
		device,
		image_info{
			.format = format,
			.extent = vk::Extent3D{extent, 1},
			.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
			.memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
			.subresource_range = vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1}
		}
	};
}

[[nodiscard]]
auto create_depth_stencil_buffer(const vkw::logical_device& device, vk::Format format, const vk::Extent2D& extent) -> image {
	return image{
		device,
		image_info{
			.format = format,
			.extent = vk::Extent3D{extent, 1},
			.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
			.memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
			.subresource_range = vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil, 0, 1, 0, 1}
		}
	};
}


[[nodiscard]]
auto create_layout_barrier(
	vk::Image            img,
	vk::ImageAspectFlags aspect_flags,
	vk::ImageLayout      old_layout,
	vk::ImageLayout      new_layout
) -> std::tuple<vk::PipelineStageFlags, vk::PipelineStageFlags, vk::ImageMemoryBarrier> {
	const auto source_access_mask = [&]() -> vk::AccessFlags {
		switch (old_layout) {
			case vk::ImageLayout::eTransferDstOptimal: return vk::AccessFlagBits::eTransferWrite;
			case vk::ImageLayout::ePreinitialized: return vk::AccessFlagBits::eHostWrite;
			case vk::ImageLayout::eGeneral: return vk::AccessFlags{}; //source_access_mask is empty
			case vk::ImageLayout::eUndefined: return vk::AccessFlags{};
			default: return vk::AccessFlags{};
		}
	}();

	const auto destination_access_mask = [&]() -> vk::AccessFlags {
		switch (new_layout) {
			case vk::ImageLayout::eColorAttachmentOptimal: return vk::AccessFlagBits::eColorAttachmentWrite;
			case vk::ImageLayout::eDepthStencilAttachmentOptimal: return vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
			case vk::ImageLayout::eGeneral: return vk::AccessFlagBits{}; //empty destination_access_mask
			case vk::ImageLayout::ePresentSrcKHR: return vk::AccessFlagBits{};
			case vk::ImageLayout::eShaderReadOnlyOptimal: return vk::AccessFlagBits::eShaderRead;
			case vk::ImageLayout::eTransferSrcOptimal: return vk::AccessFlagBits::eTransferRead;
			case vk::ImageLayout::eTransferDstOptimal: return vk::AccessFlagBits::eTransferWrite;
			default: return vk::AccessFlagBits{};
		}
	}();

	const auto source_stage = [&]() -> vk::PipelineStageFlags {
		switch (old_layout) {
			case vk::ImageLayout::eGeneral:
			case vk::ImageLayout::ePreinitialized: return vk::PipelineStageFlagBits::eHost;
			case vk::ImageLayout::eTransferDstOptimal: return vk::PipelineStageFlagBits::eTransfer;
			case vk::ImageLayout::eUndefined: return vk::PipelineStageFlagBits::eTopOfPipe;
			default: return vk::PipelineStageFlags{};
		}
	}();

	const auto destination_stage = [&]() -> vk::PipelineStageFlags {
		switch (new_layout) {
			case vk::ImageLayout::eColorAttachmentOptimal: return vk::PipelineStageFlagBits::eColorAttachmentOutput;
			case vk::ImageLayout::eDepthStencilAttachmentOptimal: return vk::PipelineStageFlagBits::eEarlyFragmentTests;
			case vk::ImageLayout::eGeneral: return vk::PipelineStageFlagBits::eHost;
			case vk::ImageLayout::ePresentSrcKHR: return vk::PipelineStageFlagBits::eBottomOfPipe;
			case vk::ImageLayout::eShaderReadOnlyOptimal: return vk::PipelineStageFlagBits::eFragmentShader;
			case vk::ImageLayout::eTransferDstOptimal: return vk::PipelineStageFlags{};
			case vk::ImageLayout::eTransferSrcOptimal: return vk::PipelineStageFlagBits::eTransfer;
			default: return vk::PipelineStageFlags{};
		}
	}();

	const auto barrier = vk::ImageMemoryBarrier{
		source_access_mask,
		destination_access_mask,
		old_layout,
		new_layout,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		img,
		vk::ImageSubresourceRange{aspect_flags, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_MIP_LEVELS}
	};

	return {source_stage, destination_stage, barrier};
}

auto create_layout_barrier(const image& img, vk::ImageLayout old_layout, vk::ImageLayout new_layout) {
	return create_layout_barrier(*img.get_vk_image(), img.get_info().subresource_range.aspectMask, old_layout, new_layout);
}

} //namespace util

} //namespace vkw

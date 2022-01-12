module;

#include <functional>
#include <span>
#include <tuple>

#include <vulkan/vulkan_raii.hpp>

export module vkw.image;

import vkw.logical_device;
import vkw.util;



export namespace vkw {
export struct image_info {
	image_info() {
		create_info.imageType     = vk::ImageType::e2D;
		create_info.format        = vk::Format::eR8G8B8A8Srgb;
		create_info.mipLevels     = 1;
		create_info.arrayLayers   = 1;
		create_info.samples       = vk::SampleCountFlagBits::e1;
		create_info.tiling        = vk::ImageTiling::eOptimal;
		create_info.usage         = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
		create_info.sharingMode   = vk::SharingMode::eExclusive;
		create_info.initialLayout = vk::ImageLayout::eUndefined;
	}

	vk::ImageCreateInfo create_info;
	vk::MemoryPropertyFlags memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
};


[[nodiscard]]
auto create_memory(const vkw::logical_device& device, const vk::raii::Image& vk_image, vk::MemoryPropertyFlags memory_properties) -> vk::raii::DeviceMemory {
	auto memory = device.create_device_memory(vk_image.getMemoryRequirements(), memory_properties);
	vk_image.bindMemory(*memory, 0);
	return memory;
}


export class image {
public:
	image(const logical_device& device, const image_info& info) :
		info(info),
		vk_image(device.get_vk_device(), info.create_info),
		device_memory(create_memory(device, vk_image, info.memory_properties)) {
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
	auto get_device_memory() const noexcept -> const vk::raii::DeviceMemory& {
		return device_memory;
	}

private:

	image_info info;

	vk::raii::Image        vk_image;
	vk::raii::DeviceMemory device_memory;
};


export class image_view {
public:
	image_view(const logical_device& device, const vk::ImageViewCreateInfo& info) :
		info(info),
		view(device.get_vk_device(), info) {
	}

	[[nodiscard]]
	auto get_info() const noexcept -> const vk::ImageViewCreateInfo& {
		return info;
	}

	[[nodiscard]]
	auto get_vk_image_view() const noexcept -> const vk::raii::ImageView& {
		return view;
	}

private:
	vk::ImageViewCreateInfo info;
	vk::raii::ImageView view;
};


export namespace util {

// This does not handle multi-planar formats
[[nodiscard]]
constexpr auto format_to_aspect(vk::Format format) noexcept -> vk::ImageAspectFlags {
	switch (format) {
		case vk::Format::eUndefined: {
			return {};
		}

		case vk::Format::eD16Unorm:
		case vk::Format::eD32Sfloat:
		case vk::Format::eX8D24UnormPack32: {
			return vk::ImageAspectFlagBits::eDepth;
		}

		case vk::Format::eS8Uint: {
			return vk::ImageAspectFlagBits::eStencil;
		}

		case vk::Format::eD16UnormS8Uint:
		case vk::Format::eD24UnormS8Uint:
		case vk::Format::eD32SfloatS8Uint: {
			return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
		}

		default: {
			return vk::ImageAspectFlagBits::eColor;
		}
	}
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
	return create_layout_barrier(*img.get_vk_image(), format_to_aspect(img.get_info().create_info.format), old_layout, new_layout);
}

} //namespace util
} //namespace vkw

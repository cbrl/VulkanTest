module;

#include <functional>
#include <memory>
#include <span>
#include <tuple>

#include <vulkan/vulkan_raii.hpp>

export module vkw.image;

import vkw.logical_device;
import vkw.util;


namespace vkw {

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

export struct image_view_info {
	vk::ImageViewCreateFlags  flags = {};
	vk::ImageViewType         view_type = vk::ImageViewType::e2D;
	vk::Format                format = vk::Format::eR8G8B8A8Srgb;
	vk::ComponentMapping      components = {vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA};
	vk::ImageSubresourceRange subresource_range = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
};


export class image {
	friend class swapchain;

	[[nodiscard]]
	static auto create(std::shared_ptr<logical_device> device, vk::Image img, const image_info& info) -> std::shared_ptr<image> {
		struct enable_construction : public image {
			enable_construction(std::shared_ptr<logical_device> device, vk::Image img, const image_info& info) :
				image(std::move(device), img, info) {
			}
		};
		return std::make_shared<enable_construction>(std::move(device), img, info);
	}

	image(std::shared_ptr<logical_device> logic_device, vk::Image img, const image_info& info) :
		device(std::move(logic_device)),
		info(info),
		vk_image(device->get_vk_handle(), img),
		device_memory(nullptr) {
	}

public:
	[[nodiscard]]
	static auto create(std::shared_ptr<logical_device> device, const image_info& info) -> std::shared_ptr<image> {
		return std::make_shared<image>(std::move(device), info);
	}

	image(std::shared_ptr<logical_device> logic_device, const image_info& info) :
		device(std::move(logic_device)),
		info(info),
		vk_image(device->get_vk_handle(), info.create_info),
		device_memory(device->create_device_memory(vk_image.getMemoryRequirements(), info.memory_properties)) {

		vk_image.bindMemory(*device_memory, 0);
	}

	[[nodiscard]]
	auto get_info() const noexcept -> const image_info& {
		return info;
	}

	[[nodiscard]]
	auto get_device() const noexcept -> const std::shared_ptr<logical_device>& {
		return device;
	}

	[[nodiscard]]
	auto get_vk_handle() const noexcept -> const vk::raii::Image& {
		return vk_image;
	}

	[[nodiscard]]
	auto get_device_memory() const noexcept -> const vk::raii::DeviceMemory& {
		return device_memory;
	}

private:
	std::shared_ptr<logical_device> device;

	image_info info;

	vk::raii::Image        vk_image;
	vk::raii::DeviceMemory device_memory;
};


export class image_view {
public:
	[[nodiscard]]
	static auto create(std::shared_ptr<image> img, const image_view_info& info) -> std::shared_ptr<image_view> {
		return std::make_shared<image_view>(std::move(img), info);
	}

	image_view(std::shared_ptr<image> src_img, const image_view_info& info) :
		img(std::move(src_img)),
		info(info),
		view(nullptr) {

		view = vk::raii::ImageView{
			img->get_device()->get_vk_handle(),
			vk::ImageViewCreateInfo{
				info.flags,
				*img->get_vk_handle(),
				info.view_type,
				info.format,
				info.components,
				info.subresource_range
			}
		};
	}

	[[nodiscard]]
	auto get_info() const noexcept -> const image_view_info& {
		return info;
	}

	[[nodiscard]]
	auto get_vk_handle() const noexcept -> const vk::raii::ImageView& {
		return view;
	}

	[[nodiscard]]
	auto get_image() const noexcept -> const std::shared_ptr<image>& {
		return img;
	}

private:
	std::shared_ptr<image> img;

	image_view_info info;
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
	return create_layout_barrier(*img.get_vk_handle(), format_to_aspect(img.get_info().create_info.format), old_layout, new_layout);
}


[[nodiscard]]
auto create_depth_buffer(const std::shared_ptr<logical_device>& device, vk::Format format, const vk::Extent2D& extent) -> std::shared_ptr<image_view> {
	auto img_info = image_info{};
	img_info.create_info.format = format;
	img_info.create_info.extent = vk::Extent3D{extent, 1};
	img_info.create_info.usage  = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
	img_info.memory_properties  = vk::MemoryPropertyFlagBits::eDeviceLocal;

	const auto view_info = image_view_info{
		.view_type         = vk::ImageViewType::e2D,
		.format            = format,
		.components        = vk::ComponentMapping{vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA},
		.subresource_range = vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1},
	};

	return image_view::create(image::create(device, img_info), view_info);
}

[[nodiscard]]
auto create_depth_stencil_buffer(const std::shared_ptr<logical_device>& device, vk::Format format, const vk::Extent2D& extent) -> std::shared_ptr<image_view> {
	auto img_info = image_info{};
	img_info.create_info.format = format;
	img_info.create_info.extent = vk::Extent3D{extent, 1};
	img_info.create_info.usage  = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
	img_info.memory_properties  = vk::MemoryPropertyFlagBits::eDeviceLocal;

	const auto view_info = image_view_info{
		.view_type         = vk::ImageViewType::e2D,
		.format            = format,
		.components        = vk::ComponentMapping{vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA},
		.subresource_range = vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil, 0, 1, 0, 1},
	};

	return image_view::create(image::create(device, img_info), view_info);
}

} //namespace util
} //namespace vkw

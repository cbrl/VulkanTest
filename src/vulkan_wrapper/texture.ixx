module;

#include <algorithm>
#include <memory>
#include <ranges>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module vkw.texture;

import vkw.buffer;
import vkw.image;
import vkw.logical_device;


export namespace vkw {
struct mip_level {
	vk::Extent3D extent;
	size_t size;
};

struct texture_info {
	image_info image;
	vk::ImageViewCreateInfo view;
	std::vector<std::vector<mip_level>> image_layers;
	bool force_staging = false;
};
} //namespace vkw


[[nodiscard]]
auto is_staging_required(const vkw::logical_device& device, vk::Format format) -> bool {
	const auto format_properties = device.get_vk_physical_device().getFormatProperties(format);
	return (format_properties.linearTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage) != vk::FormatFeatureFlagBits::eSampledImage;
}

[[nodiscard]]
auto create_image(const vkw::logical_device& device, const vkw::texture_info& tex_info) -> vkw::image {
	const auto needs_staging = tex_info.force_staging || is_staging_required(device, tex_info.image.create_info.format);

	auto image_nfo = tex_info.image;
	image_nfo.create_info.usage |= vk::ImageUsageFlagBits::eSampled;
	
	if (needs_staging) {
		image_nfo.create_info.tiling        = vk::ImageTiling::eOptimal;
		image_nfo.create_info.usage         |= vk::ImageUsageFlagBits::eTransferDst;
		image_nfo.create_info.initialLayout = vk::ImageLayout::eUndefined;
		image_nfo.memory_properties         = vk::MemoryPropertyFlagBits::eDeviceLocal;
	}
	else {
		image_nfo.create_info.tiling        = vk::ImageTiling::eLinear;
		image_nfo.create_info.initialLayout = vk::ImageLayout::ePreinitialized;
		image_nfo.memory_properties         = vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible;
	}

	if (tex_info.view.viewType == vk::ImageViewType::eCube) {
		image_nfo.create_info.flags |= vk::ImageCreateFlagBits::eCubeCompatible;
	}

	return vkw::image{device, image_nfo};
}

[[nodiscard]]
auto create_image_view(const vkw::logical_device& device, const vkw::image& img, const vkw::texture_info& tex_info) -> vkw::image_view {
	auto view_nfo = tex_info.view;
	
	view_nfo.image = *img.get_vk_image();

	view_nfo.subresourceRange.baseMipLevel   = 0;
	view_nfo.subresourceRange.levelCount     = tex_info.image_layers.empty() ? 1 : static_cast<uint32_t>(tex_info.image_layers[0].size());

	view_nfo.subresourceRange.baseArrayLayer = 0;
	view_nfo.subresourceRange.layerCount     = std::max<uint32_t>(1, static_cast<uint32_t>(tex_info.image_layers.size()));

	return vkw::image_view{device, view_nfo};
}


export namespace vkw {

class texture {
public:
	texture(const logical_device& device, const texture_info& tex_info) :
		image_data(create_image(device, tex_info)),
		view(create_image_view(device, image_data, tex_info)),
		image_layers(tex_info.image_layers) {

		needs_staging = tex_info.force_staging || is_staging_required(device, tex_info.image.create_info.format);
	}

	[[nodiscard]]
	auto get_image() const noexcept -> const image& {
		return image_data;
	}

	[[nodiscard]]
	auto get_image_view() const noexcept -> const image_view& {
		return view;
	}

	auto upload(const logical_device& device, std::span<const std::byte> data) -> void {
		if (needs_staging) {
			if (not staging_buffer || (staging_buffer->get_size_bytes() < data.size_bytes())) {
				staging_buffer = std::make_unique<buffer<void>>(device, data.size_bytes(), vk::BufferUsageFlagBits::eTransferSrc);
			}

			staging_buffer->upload(data);
		}
		else {
			auto* memory = image_data.get_device_memory().mapMemory(0, data.size_bytes());
			std::memcpy(memory, data.data(), data.size_bytes());
			image_data.get_device_memory().unmapMemory();
		}
	}

	auto stage(const vk::raii::CommandBuffer& command_buffer) -> void {
		if (not needs_staging) {
			// Use the linear tiled image as a texture if possible
			const auto [src_stage, dest_stage, barrier] = vkw::util::create_layout_barrier(
				image_data,
				vk::ImageLayout::ePreinitialized,
				vk::ImageLayout::eShaderReadOnlyOptimal
			);

			command_buffer.pipelineBarrier(src_stage, dest_stage, vk::DependencyFlagBits{}, nullptr, nullptr, barrier);
		}
		else {
			auto copy_regions = std::vector<vk::BufferImageCopy>{};

			if (not image_layers.empty()) {
				auto offset = vk::DeviceSize{0};

				for (uint32_t layer = 0; layer < image_layers.size(); ++layer) {
					for (uint32_t level = 0; level < image_layers[layer].size(); ++level) {
						copy_regions.push_back(vk::BufferImageCopy{
							offset,
							0,
							0,
							vk::ImageSubresourceLayers{view.get_info().subresourceRange.aspectMask, level, layer, 1},
							vk::Offset3D{0, 0, 0},
							image_layers[layer][level].extent
						});

						offset += image_layers[layer][level].size;
					}
				}
			}
			else {
				copy_regions.push_back(vk::BufferImageCopy{
					0,
					image_data.get_info().create_info.extent.width,
					image_data.get_info().create_info.extent.height,
					vk::ImageSubresourceLayers{view.get_info().subresourceRange.aspectMask, 0, 0, 1},
					vk::Offset3D{0, 0, 0},
					image_data.get_info().create_info.extent
				});
			}

			// Since we're going to blit to the texture image, set its layout to eTransferDstOptimal
			{
				const auto [src_stage, dest_stage, barrier] = vkw::util::create_layout_barrier(
					image_data,
					vk::ImageLayout::eUndefined,
					vk::ImageLayout::eTransferDstOptimal
				);

				command_buffer.pipelineBarrier(src_stage, dest_stage, vk::DependencyFlagBits{}, nullptr, nullptr, barrier);
			}

			command_buffer.copyBufferToImage(
				*staging_buffer->get_vk_buffer(),
				*image_data.get_vk_image(),
				vk::ImageLayout::eTransferDstOptimal,
				copy_regions
			);

			// Set the layout for the texture image from eTransferDstOptimal to eShaderReadOnlyOptimal
			{
				const auto [src_stage, dest_stage, barrier] = vkw::util::create_layout_barrier(
					image_data,
					vk::ImageLayout::eTransferDstOptimal,
					vk::ImageLayout::eShaderReadOnlyOptimal
				);

				command_buffer.pipelineBarrier(src_stage, dest_stage, vk::DependencyFlagBits{}, nullptr, nullptr, barrier);
			}
		}
	}

private:

	image      image_data;
	image_view view;

	std::vector<std::vector<mip_level>> image_layers;

	std::unique_ptr<buffer<void>> staging_buffer;
	bool needs_staging;
};


namespace util {
	
[[nodiscard]]
auto create_depth_buffer(const vkw::logical_device& device, vk::Format format, const vk::Extent2D& extent) -> texture {
	auto info = texture_info{};

	info.force_staging = true;

	info.image.create_info.format = format;
	info.image.create_info.extent = vk::Extent3D{extent, 1};
	info.image.create_info.usage  = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
	info.image.memory_properties  = vk::MemoryPropertyFlagBits::eDeviceLocal;

	info.view.viewType         = vk::ImageViewType::e2D;
	info.view.format           = format;
	info.view.components       = vk::ComponentMapping{vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA};
	info.view.subresourceRange = vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1};

	return texture{device, info};
}

[[nodiscard]]
auto create_depth_stencil_buffer(const vkw::logical_device& device, vk::Format format, const vk::Extent2D& extent) -> texture {
	auto info = texture_info{};

	info.force_staging = true;

	info.image.create_info.format = format;
	info.image.create_info.extent = vk::Extent3D{extent, 1};
	info.image.create_info.usage  = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
	info.image.memory_properties  = vk::MemoryPropertyFlagBits::eDeviceLocal;

	info.view.viewType         = vk::ImageViewType::e2D;
	info.view.format           = format;
	info.view.components       = vk::ComponentMapping{vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA};
	info.view.subresourceRange = vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil, 0, 1, 0, 1};

	return texture{device, info};
}

} //namespace util
} //namespace vkw

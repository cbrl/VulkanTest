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
struct texture_info {
	vk::ImageType           type              = vk::ImageType::e2D;
	vk::Format              format            = vk::Format::eR8G8B8A8Srgb;
	vk::Extent3D            extent;
	vk::ImageUsageFlags     usage;
	vk::ImageViewType       view_type         = vk::ImageViewType::e2D;
	vk::ComponentMapping    component_mapping = {vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA};
	vk::ImageAspectFlags    aspect_flags      = vk::ImageAspectFlagBits::eColor;
	vk::SamplerAddressMode  sampler_mode      = vk::SamplerAddressMode::eRepeat;
	bool                    anisotropy        = true;
};

struct mip_level {
	vk::Extent3D extent;
	size_t size;
};
}


[[nodiscard]]
auto is_staging_required(const vkw::logical_device& device, vk::Format format) -> bool {
	const auto format_properties = device.get_vk_physical_device().getFormatProperties(format);
	return (format_properties.linearTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage) != vk::FormatFeatureFlagBits::eSampledImage;
}

[[nodiscard]]
static auto create_image(
	const vkw::logical_device& device,
	const vkw::texture_info& tex_info,
	const std::vector<std::vector<vkw::mip_level>>& layers
) -> vkw::image {
	const auto needs_staging = is_staging_required(device, tex_info.format);

	auto image_nfo = vkw::image_info{
		.type              = tex_info.type,
		.format            = tex_info.format,
		.extent            = tex_info.extent,
		.usage             = tex_info.usage | vk::ImageUsageFlagBits::eSampled,
		.view_type         = tex_info.view_type,
		.component_mapping = tex_info.component_mapping,
		.subresource_range = vk::ImageSubresourceRange{
			tex_info.aspect_flags,
			0,
			(layers.empty() ? 1 : static_cast<uint32_t>(layers[0].size())),
			0,
			std::max<uint32_t>(1, static_cast<uint32_t>(layers.size()))
		}
	};
	
	if (needs_staging) {
		image_nfo.tiling            = vk::ImageTiling::eOptimal;
		image_nfo.usage             |= vk::ImageUsageFlagBits::eTransferDst;
		image_nfo.initial_layout    = vk::ImageLayout::eUndefined;
		image_nfo.memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
	}
	else {
		image_nfo.tiling            = vk::ImageTiling::eLinear;
		image_nfo.initial_layout    = vk::ImageLayout::ePreinitialized;
		image_nfo.memory_properties = vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible;
	}

	if (tex_info.view_type == vk::ImageViewType::eCube) {
		image_nfo.flags |= vk::ImageCreateFlagBits::eCubeCompatible;
	}

	return vkw::image{device, image_nfo};
}

[[nodiscard]]
static auto create_sampler(const vkw::logical_device& device, vk::SamplerAddressMode sampler_mode, bool anisotropy, float max_lod) -> vk::raii::Sampler {
	const auto features   = device.get_vk_physical_device().getFeatures();
	const auto properties = device.get_vk_physical_device().getProperties();

	return vk::raii::Sampler{
		device.get_vk_device(),
		vk::SamplerCreateInfo{
			vk::SamplerCreateFlags{},
			vk::Filter::eLinear,
			vk::Filter::eLinear,
			vk::SamplerMipmapMode::eLinear,
			sampler_mode,
			sampler_mode,
			sampler_mode,
			0.0f,
			anisotropy && features.samplerAnisotropy,
			properties.limits.maxSamplerAnisotropy,
			false,
			vk::CompareOp::eNever,
			0.0f,
			max_lod,
			vk::BorderColor::eFloatOpaqueBlack,
			false
		}
	};
}


export namespace vkw {

class texture {
public:
	texture(const logical_device& device, const texture_info& tex_info, const std::vector<std::vector<mip_level>>& img_layers = {}) :
		image_data(create_image(device, tex_info, img_layers)),
		sampler(create_sampler(device, tex_info.sampler_mode, tex_info.anisotropy, static_cast<float>(img_layers.size()))),
		layers(img_layers) {

		needs_staging = is_staging_required(device, tex_info.format);
	}

	[[nodiscard]]
	auto get_image() const -> const image& {
		return image_data;
	}

	[[nodiscard]]
	auto get_sampler() const -> const vk::raii::Sampler& {
		return sampler;
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
		auto copy_regions = std::vector<vk::BufferImageCopy>{};

		if (not layers.empty()) {
			auto offset = size_t{0};

			for (uint32_t layer = 0; layer < layers.size(); ++layer) {
				for (uint32_t level = 0; level < layers[layer].size(); ++level) {
					copy_regions.push_back(vk::BufferImageCopy{
						offset,
						0,
						0,
						vk::ImageSubresourceLayers{image_data.get_info().subresource_range.aspectMask, level, layer, 1},
						vk::Offset3D{0, 0, 0},
						layers[layer][level].extent
					});

					offset += layers[layer][level].size;
				}
			}
		}
		else {
			copy_regions.push_back(vk::BufferImageCopy{
				0,
				image_data.get_info().extent.width,
				image_data.get_info().extent.height,
				vk::ImageSubresourceLayers{image_data.get_info().subresource_range.aspectMask, 0, 0, 1},
				vk::Offset3D{0, 0, 0},
				image_data.get_info().extent
			});
		}

		if (needs_staging) {
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
		else
		{
			// Use the linear tiled image as a texture if possible
			const auto [src_stage, dest_stage, barrier] = vkw::util::create_layout_barrier(
				image_data,
				vk::ImageLayout::ePreinitialized,
				vk::ImageLayout::eShaderReadOnlyOptimal
			);

			command_buffer.pipelineBarrier(src_stage, dest_stage, vk::DependencyFlagBits{}, nullptr, nullptr, barrier);
		}
	}

private:

	image image_data;
	vk::raii::Sampler sampler;

	std::vector<std::vector<mip_level>> layers;

	std::unique_ptr<buffer<void>> staging_buffer;
	bool needs_staging;
};

} //namespace vkw

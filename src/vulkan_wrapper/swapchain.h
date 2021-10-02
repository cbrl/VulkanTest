#pragma once

#include <memory>
#include <utility>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

#include "logical_device.h"


namespace vkw {

class swapchain {
public:
	auto create(
		const vkw::logical_device& device,
		const vk::raii::SurfaceKHR& surface,
		vk::SurfaceFormatKHR format,
		vk::ImageUsageFlags usage,
		vk::Extent2D size,
		bool vsync,
		std::vector<uint32_t> shared_queues = {}
	) -> void {
		this->device        = &device;
		this->surface       = &surface;
		this->format        = format;
		this->usage         = usage;
		this->size          = size;
		this->vsync         = vsync;
		this->shared_queues = shared_queues;
	}

	auto create_impl() -> void {
		assert(device && surface);

		const auto& vk_device          = device->get_vk_device();
		const auto& vk_physical_device = device->get_vk_physical_device();

		const auto surface_capabilities  = vk_physical_device.getSurfaceCapabilitiesKHR(**surface);
		const auto surface_present_mdoes = vk_physical_device.getSurfacePresentModesKHR(**surface);

		const auto present_mode     = vsync ? vk::PresentModeKHR::eFifo : select_present_mode(surface_present_mdoes);
		const auto swapchain_extent = select_swapchain_extent(surface_capabilities, size);
		const auto pre_transform    = select_transform(surface_capabilities);
		const auto composite_alpha  = select_composite_alpha(surface_capabilities);

		auto swap_chain_create_info = vk::SwapchainCreateInfoKHR{
			vk::SwapchainCreateFlagsKHR{},
			**surface,
			surface_capabilities.minImageCount,
			format.format,
			format.colorSpace,
			swapchain_extent,
			1,
			usage,
			vk::SharingMode::eExclusive,
			{},
			pre_transform,
			composite_alpha,
			present_mode,
			true,
			vk_swapchain ? **vk_swapchain : vk::SwapchainKHR{}
		};

		if (not shared_queues.empty()) {
			// If the associated queues are from different queue families, we either have to explicitly
			// transfer ownership of images between the queues, or we have to create the swapchain with
			// imageSharingMode as vk::SharingMode::eConcurrent
			swap_chain_create_info.imageSharingMode      = vk::SharingMode::eConcurrent;
			swap_chain_create_info.queueFamilyIndexCount = static_cast<uint32_t>(shared_queues.size());
			swap_chain_create_info.pQueueFamilyIndices   = shared_queues.data();
		}

		vk_swapchain = std::make_unique<vk::raii::SwapchainKHR>(vk_device, swap_chain_create_info);

		const auto swap_images = vk_swapchain->getImages();
		images.reserve(swap_images.size());
		std::ranges::copy(swap_images, std::back_inserter(images));

		const auto component_mapping  = vk::ComponentMapping(vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA);
		const auto subresource_range = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

		image_views.reserve(images.size());
		for (const auto& image : images) {
			const auto image_view_create_info = vk::ImageViewCreateInfo{
				vk::ImageViewCreateFlags{},
				static_cast<vk::Image>(image),
				vk::ImageViewType::e2D,
				format.format,
				component_mapping,
				subresource_range
			};

			image_views.emplace_back(vk_device, image_view_create_info);
		}
	}

	auto resize(const vk::raii::PhysicalDevice& physical_device, vk::Extent2D new_size) -> void {
		assert(device);
		device->get_vk_device().waitIdle();

		size = new_size;
		create_impl();
	}

	[[nodiscard]]
	auto get_size() -> vk::Extent2D {
		return size;
	}

	[[nodiscard]]
	auto get_vk_swapchain() -> vk::raii::SwapchainKHR& {
		return *vk_swapchain;
	}

	[[nodiscard]]
	auto get_vk_swapchain() const -> const vk::raii::SwapchainKHR& {
		return *vk_swapchain;
	}

	[[nodiscard]]
	auto get_images() const -> const std::vector<vk::Image>& {
		return images;
	}

	[[nodiscard]]
	auto get_image_views() const -> const std::vector<vk::raii::ImageView>& {
		return image_views;
	}

private:

	[[nodiscard]]
	static auto select_present_mode(const std::vector<vk::PresentModeKHR>& modes) -> vk::PresentModeKHR {
		static const vk::PresentModeKHR desired_modes[] = {
			vk::PresentModeKHR::eMailbox,
			vk::PresentModeKHR::eImmediate
		};

		for (const auto& mode : desired_modes) {
			if (const auto it = std::ranges::find(modes, mode); it != modes.end()) {
				return *it;
			}
		}

		// FIFO is guaranteed to be available
		return vk::PresentModeKHR::eFifo;
	}

	[[nodiscard]]
	static auto select_swapchain_extent(const vk::SurfaceCapabilitiesKHR& surface_capabilities, vk::Extent2D requested_size) -> vk::Extent2D {
		if (surface_capabilities.currentExtent.width == std::numeric_limits<uint32_t>::max()) {
			// If the surface size is undefined, the size is set to the size of the images requested.
			requested_size.width  = std::clamp(requested_size.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
			requested_size.height = std::clamp(requested_size.height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
			return requested_size;
		}
		else {
			// If the surface size is defined, the swap chain size must match.
			return surface_capabilities.currentExtent;
		}
	}

	[[nodiscard]]
	static auto select_transform(const vk::SurfaceCapabilitiesKHR& surface_capabilities) -> vk::SurfaceTransformFlagBitsKHR {
		if (surface_capabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity) {
			return vk::SurfaceTransformFlagBitsKHR::eIdentity;
		}
		else {
			return surface_capabilities.currentTransform;
		}
	}

	[[nodiscard]]
	static auto select_composite_alpha(const vk::SurfaceCapabilitiesKHR& surface_capabilities) -> vk::CompositeAlphaFlagBitsKHR {
		if (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePreMultiplied) {
			return vk::CompositeAlphaFlagBitsKHR::ePreMultiplied;
		}
		else if (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePostMultiplied) {
			return vk::CompositeAlphaFlagBitsKHR::ePostMultiplied;
		}
		else if (surface_capabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit) {
			return vk::CompositeAlphaFlagBitsKHR::eInherit;
		}
		else {
			return vk::CompositeAlphaFlagBitsKHR::eOpaque;
		}
	}

	const vkw::logical_device* device;
	const vk::raii::SurfaceKHR* surface;

	vk::SurfaceFormatKHR format = {};
	vk::ImageUsageFlags usage = {};
	vk::Extent2D size = {};
	bool vsync = false;
	std::vector<uint32_t> shared_queues = {};

	std::unique_ptr<vk::raii::SwapchainKHR> vk_swapchain;
	std::vector<vk::Image> images;
	std::vector<vk::raii::ImageView> image_views;
};

} //namespace vkw

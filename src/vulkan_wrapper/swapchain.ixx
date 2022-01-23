module;

#include <algorithm>
#include <memory>
#include <ranges>
#include <utility>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module vkw.swapchain;

import vkw.image;
import vkw.logical_device;
import vkw.util;
import vkw.window;


[[nodiscard]]
auto select_present_mode(const std::vector<vk::PresentModeKHR>& modes) -> vk::PresentModeKHR {
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
auto select_swapchain_extent(const vk::SurfaceCapabilitiesKHR& surface_capabilities, vk::Extent2D requested_size) -> vk::Extent2D {
	if (surface_capabilities.currentExtent.width == std::numeric_limits<uint32_t>::max()) {
		// If the surface size is undefined, the size is set to the size of the images requested.
		requested_size.width = std::clamp(requested_size.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
		requested_size.height = std::clamp(requested_size.height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
		return requested_size;
	}
	else {
		// If the surface size is defined, the swap chain size must match.
		return surface_capabilities.currentExtent;
	}
}

[[nodiscard]]
auto select_transform(const vk::SurfaceCapabilitiesKHR& surface_capabilities) -> vk::SurfaceTransformFlagBitsKHR {
	if (surface_capabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity) {
		return vk::SurfaceTransformFlagBitsKHR::eIdentity;
	}
	else {
		return surface_capabilities.currentTransform;
	}
}

[[nodiscard]]
auto select_composite_alpha(const vk::SurfaceCapabilitiesKHR& surface_capabilities) -> vk::CompositeAlphaFlagBitsKHR {
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


export namespace vkw {

class swapchain : public std::enable_shared_from_this<swapchain> {
public:
	[[nodiscard]]
	static auto create(std::shared_ptr<logical_device> device, std::shared_ptr<window> wind) -> std::shared_ptr<swapchain> {
		return std::make_shared<swapchain>(std::move(device), std::move(wind));
	}

	swapchain(std::shared_ptr<logical_device> device, std::shared_ptr<window> wind) :
		device(std::move(device)),
		wind(std::move(wind)) {
	}

	~swapchain() {
		invalidate_images();
	}

	auto rebuild(
		vk::SurfaceFormatKHR format,
		vk::ImageUsageFlags usage,
		vk::Extent2D size,
		bool vsync,
		const std::vector<uint32_t>& shared_queues = {}
	) -> void {
		this->format        = format;
		this->usage         = usage;
		this->size          = size;
		this->vsync         = vsync;
		this->shared_queues = shared_queues;
		create_impl();
	}

	auto resize(vk::Extent2D new_size) -> void {
		device->get_vk_device().waitIdle();

		size = new_size;
		create_impl();
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
	auto get_size() const noexcept -> vk::Extent2D {
		return size;
	}

	[[nodiscard]]
	auto get_format() const noexcept -> vk::SurfaceFormatKHR {
		return format;
	}

	[[nodiscard]]
	auto get_image_count() const noexcept -> size_t {
		return image_count;
	}

	[[nodiscard]]
	auto get_image(size_t idx) -> std::shared_ptr<image> {
		// The image is already a shared_ptr, but we want the returned shared_ptr to extend the entire swapchain's
		// lifetime. The swapchain's reference count is used for the resulting shared_ptr instead of the image's.
		return std::shared_ptr<image>{shared_from_this(), images.at(idx).get()};
	}

	[[nodiscard]]
	auto get_images() -> std::vector<std::shared_ptr<image>> {
		return ranges::to<std::vector>(images | std::views::transform([this](auto&& img) {
			return std::shared_ptr<image>{shared_from_this(), img.get()};
		}));
	}

	[[nodiscard]]
	auto get_image_view(size_t idx) -> std::shared_ptr<image_view> {
		return std::shared_ptr<image_view>{shared_from_this(), &image_views.at(idx)};
	}

	[[nodiscard]]
	auto get_image_views() -> std::vector<std::shared_ptr<image_view>> {
		return ranges::to<std::vector>(image_views | std::views::transform([this](auto&& view) {
			return std::shared_ptr<image_view>{shared_from_this(), &view};
		}));
	}

private:

	auto create_impl() -> void {
		invalidate_images();

		const auto& vk_physical_device = device->get_vk_physical_device();

		const auto surface_capabilities  = vk_physical_device->getSurfaceCapabilitiesKHR(*wind->get_surface());
		const auto surface_present_modes = vk_physical_device->getSurfacePresentModesKHR(*wind->get_surface());

		const auto present_mode     = vsync ? vk::PresentModeKHR::eFifo : select_present_mode(surface_present_modes);
		const auto pre_transform    = select_transform(surface_capabilities);
		const auto composite_alpha  = select_composite_alpha(surface_capabilities);

		size = select_swapchain_extent(surface_capabilities, size);

		// If the associated queues are from different queue families, we either have to explicitly
		// transfer ownership of images between the queues, or we have to create the swapchain with
		// imageSharingMode as vk::SharingMode::eConcurrent
		const auto swap_chain_create_info = vk::SwapchainCreateInfoKHR{
			vk::SwapchainCreateFlagsKHR{},
			*wind->get_surface(),
			surface_capabilities.minImageCount,
			format.format,
			format.colorSpace,
			size,
			1,
			usage,
			shared_queues.empty() ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent,
			shared_queues,
			pre_transform,
			composite_alpha,
			present_mode,
			true,
			vk_swapchain ? **vk_swapchain : vk::SwapchainKHR{}
		};

		vk_swapchain = std::make_unique<vk::raii::SwapchainKHR>(device->get_vk_device(), swap_chain_create_info);

		const auto swap_images = vk_swapchain->getImages();
		image_count = swap_images.size();

		auto img_info = image_info{};
		img_info.create_info = vk::ImageCreateInfo{
			vk::ImageCreateFlags{},
			vk::ImageType::e2D,
			format.format,
			vk::Extent3D{size, 1},
			1,
			1,
			vk::SampleCountFlagBits::e1,
			vk::ImageTiling::eOptimal,
			usage,
			shared_queues.empty() ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent,
			shared_queues,
			vk::ImageLayout::eUndefined
		};

		const auto view_info = image_view_info{
			.format = format.format,
			.components = vk::ComponentMapping{vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA},
			.subresource_range = vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
		};

		images.reserve(swap_images.size());
		image_views.reserve(swap_images.size());
		for (auto img : swap_images) {
			images.push_back(image::create(device, vk::Image{img}, img_info));
			image_views.push_back(image_view{images.back(), view_info});
		}
	}

	auto invalidate_images() noexcept -> void {
		for (auto& img : images) {
			// Hacky way to prevent destruction of the swapchain-owned images
			const_cast<vk::raii::Image&>(img->get_vk_image()) = vk::raii::Image{nullptr};
		}
		image_views.clear();
		images.clear();
	}


	std::shared_ptr<logical_device> device;
	std::shared_ptr<window> wind;

	vk::SurfaceFormatKHR format = {};
	vk::ImageUsageFlags usage = {};
	vk::Extent2D size = {};
	bool vsync = false;
	size_t image_count = 0;
	std::vector<uint32_t> shared_queues = {};

	std::unique_ptr<vk::raii::SwapchainKHR> vk_swapchain;
	std::vector<std::shared_ptr<image>> images;
	std::vector<image_view> image_views;
};

} //namespace vkw

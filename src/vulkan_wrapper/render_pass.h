#pragma once

#include <functional>
#include <iterator>
#include <ranges>
#include <vector>

#include <vulkan/vulkan_raii.hpp>


namespace vkw {

// TODO: support subpasses
class render_pass {
public:
	render_pass(
		vk::raii::Device& device,
		const std::vector<vk::AttachmentDescription>& attachment_descriptions,
		const std::vector<std::vector<vk::raii::ImageView>>& target_attachments,
		const vk::Rect2D& area_rect
	) : area(area_rect) {

		// Create the render pass
		const auto create_info = vk::RenderPassCreateInfo{
			vk::RenderPassCreateFlags{},
			attachment_descriptions,
			{},
			{}
		};
		pass = std::make_unique<vk::raii::RenderPass>(device, create_info);

		// Create the framebuffers
		framebuffers.reserve(target_attachments.size());

		for (const auto& attachment : target_attachments) {
			auto vk_image_views = std::vector<vk::ImageView>(attachment.size());

			std::ranges::copy(
				std::views::transform(attachment, &vk::raii::ImageView::operator*),
				std::back_inserter(vk_image_views)
			);

			const auto create_info = vk::FramebufferCreateInfo{
				vk::FramebufferCreateFlags{},
				**pass,
				vk_image_views,
				area.extent.width,
				area.extent.height,
				1
			};

			framebuffers.emplace_back(device, create_info);
		}
	}

	auto set_clear_values(const std::vector<vk::ClearValue>& values) -> void {
		clear_values = values;
	}

	auto get_clear_values() const noexcept -> const std::vector<vk::ClearValue>& {
		return clear_values;
	}

	auto get_render_pass_begin_info(uint32_t frame) -> vk::RenderPassBeginInfo {
		return vk::RenderPassBeginInfo{
			**pass,
			*framebuffers[frame],
			area,
			clear_values
		};
	}

private:

	std::unique_ptr<vk::raii::RenderPass> pass;
	std::vector<vk::raii::Framebuffer> framebuffers;
	std::vector<vk::ClearValue> clear_values;
	vk::Rect2D area;
};

} //namespace vkw

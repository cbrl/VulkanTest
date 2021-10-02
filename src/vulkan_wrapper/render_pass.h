#pragma once

#include <functional>
#include <iterator>
#include <ranges>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

#include "logical_device.h"
#include "subpass.h"


namespace vkw {

class render_pass {
public:
	render_pass(const logical_device& device) : device(device) {
	}

	auto create(const std::vector<std::vector<vk::raii::ImageView>>& target_attachments, const vk::Rect2D& area_rect) -> void {
		create_render_pass();
		create_framebuffers(target_attachments, area_rect);
	}

	auto create(const std::vector<std::vector<vk::ImageView>>& target_attachments, const vk::Rect2D& area_rect) -> void {
		create_render_pass();
		create_framebuffers(target_attachments, area_rect);
	}

	auto add_attachment(const vk::AttachmentDescription& attachment) -> void {
		attachment_descriptions.push_back(attachment);
	}

	auto add_subpass(const subpass& pass) -> void {
		subpasses.push_back(pass);
	}

	auto add_subpass_dependency(const vk::SubpassDependency& dependency) -> void {
		subpass_dependencies.push_back(dependency);
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

	auto create_render_pass() -> void {
		auto desc_view = std::views::transform(subpasses, &subpass::get_description);
		const auto subpass_descriptions = std::vector<vk::SubpassDescription>{desc_view.begin(), desc_view.end()};

		const auto create_info = vk::RenderPassCreateInfo{
			vk::RenderPassCreateFlags{},
			attachment_descriptions,
			subpass_descriptions,
			subpass_dependencies
		};

		pass = std::make_unique<vk::raii::RenderPass>(device.get().get_vk_device(), create_info);
	}

	auto create_framebuffers(const std::vector<std::vector<vk::raii::ImageView>>& target_attachments, const vk::Rect2D& area_rect) -> void {
		assert(framebuffers.empty());

		area = area_rect;
		framebuffers.reserve(target_attachments.size());

		for (const auto& attachment : target_attachments) {
			auto vk_image_views = std::vector<vk::ImageView>{};
			vk_image_views.reserve(attachment.size());

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

			framebuffers.emplace_back(device.get().get_vk_device(), create_info);
		}
	}

	auto create_framebuffers(const std::vector<std::vector<vk::ImageView>>& target_attachments, const vk::Rect2D& area_rect) -> void {
		assert(framebuffers.empty());

		area = area_rect;
		framebuffers.reserve(target_attachments.size());

		for (const auto& attachment : target_attachments) {
			const auto create_info = vk::FramebufferCreateInfo{
				vk::FramebufferCreateFlags{},
				**pass,
				attachment,
				area.extent.width,
				area.extent.height,
				1
			};

			framebuffers.emplace_back(device.get().get_vk_device(), create_info);
		}
	}

	std::reference_wrapper<const logical_device> device;
	std::unique_ptr<vk::raii::RenderPass> pass;
	std::vector<std::reference_wrapper<const subpass>> subpasses;
	std::vector<vk::SubpassDependency> subpass_dependencies;
	std::vector<vk::raii::Framebuffer> framebuffers;
	std::vector<vk::AttachmentDescription> attachment_descriptions;
	std::vector<vk::ClearValue> clear_values;
	vk::Rect2D area;
};

} //namespace vkw

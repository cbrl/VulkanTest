module;

#include <cassert>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <ranges>
#include <utility>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module vkw.render_pass;

import vkw.image;
import vkw.logical_device;
import vkw.util;
import vkw.subpass;


namespace vkw {

export struct render_pass_info {
	vk::Rect2D area_rect;

	std::vector<std::reference_wrapper<const subpass>> subpasses;
	std::vector<vk::SubpassDependency> subpass_dependencies;

	std::vector<std::vector<std::shared_ptr<image_view>>> target_attachments;
	std::vector<vk::AttachmentDescription> attachment_descriptions;
};


[[nodiscard]]
auto create_render_pass(const logical_device& device, const render_pass_info& info) -> vk::raii::RenderPass {
	const auto subpass_descriptions = ranges::to<std::vector>(info.subpasses | std::views::transform(&subpass::get_description));

	const auto create_info = vk::RenderPassCreateInfo{
		vk::RenderPassCreateFlags{},
		info.attachment_descriptions,
		subpass_descriptions,
		info.subpass_dependencies
	};

	return vk::raii::RenderPass{device.get_vk_handle(), create_info};
}

[[nodiscard]]
auto create_framebuffers(const logical_device& device, const vk::raii::RenderPass& pass, const render_pass_info& info) -> std::vector<vk::raii::Framebuffer> {
	auto framebuffers = std::vector<vk::raii::Framebuffer>{};
	framebuffers.reserve(info.target_attachments.size());

	for (const auto& attachment : info.target_attachments) {
		const auto view_handles = ranges::to<std::vector>(attachment | util::as_handles());

		const auto create_info = vk::FramebufferCreateInfo{
			vk::FramebufferCreateFlags{},
			*pass,
			view_handles,
			info.area_rect.extent.width,
			info.area_rect.extent.height,
			1
		};

		framebuffers.emplace_back(device.get_vk_handle(), create_info);
	}

	return framebuffers;
}


export class render_pass {
public:
	render_pass(std::shared_ptr<logical_device> logic_device, const render_pass_info& info) :
		device(std::move(logic_device)),
		info(info),
		pass(create_render_pass(*device, info)),
		framebuffers(create_framebuffers(*device, pass, info)) {
	}

	[[nodiscard]]
	auto get_pass_info() const noexcept -> const render_pass_info& {
		return info;
	}

	[[nodiscard]]
	auto get_vk_handle() const noexcept -> const vk::raii::RenderPass& {
		return pass;
	}

	[[nodiscard]]
	auto get_framebuffers() const noexcept -> const std::vector<vk::raii::Framebuffer>& {
		return framebuffers;
	}

	[[nodiscard]]
	auto get_clear_values() const noexcept -> const std::vector<vk::ClearValue>& {
		return clear_values;
	}

	auto set_clear_values(std::initializer_list<vk::ClearValue> values) noexcept -> void {
		clear_values = std::move(values);
	}

	auto set_clear_values(std::span<const vk::ClearValue> values) -> void {
		clear_values.clear();
		std::ranges::copy(values, std::back_insert_iterator(clear_values));
	}

	[[nodiscard]]
	auto get_render_pass_begin_info(uint32_t frame) -> vk::RenderPassBeginInfo {
		assert(clear_values.size() >= info.target_attachments.size());

		return vk::RenderPassBeginInfo{
			*pass,
			*framebuffers[frame],
			info.area_rect,
			clear_values
		};
	}

private:
	std::shared_ptr<logical_device> device;

	render_pass_info info;

	vk::raii::RenderPass pass;
	std::vector<vk::raii::Framebuffer> framebuffers;
	std::vector<vk::ClearValue> clear_values;
};

} //namespace vkw

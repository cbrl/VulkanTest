module;

#include <span>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module vkw.render_pass_single;


export namespace vkw {

class render_pass_single {
public:
	auto get_rendering_info(uint32_t frame, vk::RenderingFlagsKHR flags = {}) const -> vk::RenderingInfoKHR {
		return vk::RenderingInfoKHR{
			flags,
			area_rect,
			1,
			0,
			color_attachments.at(frame),
			&depth_stencil_attachment,
			&depth_stencil_attachment,
		};
	}

	std::vector<std::vector<vk::RenderingAttachmentInfoKHR>> color_attachments;
	vk::RenderingAttachmentInfoKHR depth_stencil_attachment;
	vk::Rect2D area_rect;
};

} //namespace vkw

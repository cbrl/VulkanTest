module;

#include <algorithm>
#include <ranges>
#include <span>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module vkw.render_pass_single;

import vkw.image;

export namespace vkw {

class render_pass_single {
public:
	[[nodiscard]]
	auto get_area() const noexcept -> vk::Rect2D {
		return area_rect;
	}

	auto set_area(vk::Rect2D area) -> void {
		area_rect = area;
	}

	auto set_frame_color_attachments(
		uint32_t frame,
		vk::RenderingAttachmentInfoKHR info,
		vk::Image img,
		vk::ImageLayout initial_layout,
		vk::ImageLayout final_layout
	) -> void {
		set_frame_color_attachments(frame, std::span{&info, 1}, std::span{&img, 1}, std::span{&initial_layout, 1}, std::span{&final_layout, 1});
	}

	auto set_frame_color_attachments(
		uint32_t frame,
		std::span<const vk::RenderingAttachmentInfoKHR> info,
		std::span<const vk::Image> images,
		std::span<const vk::ImageLayout> initial_layouts,
		std::span<const vk::ImageLayout> final_layouts
	) -> void {
		assert(info.size() == images.size() == initial_layouts.size() == final_layouts.size());

		color_attachments.resize(std::max<size_t>(color_attachments.size(), frame + 1));
		color_attachments[frame].insert(color_attachments[frame].end(), info.begin(), info.end());

		color_images.resize(std::max<size_t>(color_images.size(), frame + 1));
		color_images[frame].insert(color_images[frame].end(), images.begin(), images.end());

		color_initial_layouts.resize(std::max<size_t>(color_initial_layouts.size(), frame + 1));
		color_initial_layouts[frame].insert(color_initial_layouts[frame].end(), initial_layouts.begin(), initial_layouts.end());

		color_final_layouts.resize(std::max<size_t>(color_final_layouts.size(), frame + 1));
		color_final_layouts[frame].insert(color_final_layouts[frame].end(), final_layouts.begin(), final_layouts.end());
	}

	auto add_frame_color_attachments(
		vk::RenderingAttachmentInfoKHR info,
		vk::Image img,
		vk::ImageLayout initial_layout,
		vk::ImageLayout final_layout
	) -> void {
		add_frame_color_attachments(std::span{&info, 1}, std::span{&img, 1}, std::span{&initial_layout, 1}, std::span{&final_layout, 1});
	}

	auto add_frame_color_attachments(
		std::span<const vk::RenderingAttachmentInfoKHR> info,
		std::span<const vk::Image> images,
		std::span<const vk::ImageLayout> initial_layouts,
		std::span<const vk::ImageLayout> final_layouts
	) -> void {
		set_frame_color_attachments(color_attachments.size(), info, images, initial_layouts, final_layouts);
	}

	auto set_depth_stencil_attachment(
		vk::RenderingAttachmentInfoKHR info,
		const image& img,
		vk::ImageLayout initial_layout,
		vk::ImageLayout final_layout
	) -> void {
		const auto has_stencil = (img.get_info().aspect_flags & vk::ImageAspectFlagBits::eStencil) != vk::ImageAspectFlagBits{};
		set_depth_stencil_attachment(info, *img.get_vk_image(), initial_layout, final_layout, has_stencil);
	}

	auto set_depth_stencil_attachment(
		vk::RenderingAttachmentInfoKHR info,
		vk::Image img,
		vk::ImageLayout initial_layout,
		vk::ImageLayout final_layout,
		bool has_stencil_buffer = false
	) -> void {
		depth_stencil_attachment = info;
		depth_stencil_image      = img;
		depth_initial_layout     = initial_layout;
		depth_final_layout       = final_layout;
		stencil_buffer           = has_stencil_buffer;
	}

	[[nodiscard]]
	auto get_rendering_info(uint32_t frame, vk::RenderingFlagsKHR flags = {}) const -> vk::RenderingInfoKHR {
		return vk::RenderingInfoKHR{
			flags,
			area_rect,
			1,
			0,
			color_attachments.at(frame),
			&depth_stencil_attachment,
			stencil_buffer ? &depth_stencil_attachment : nullptr,
		};
	}

	auto begin(uint32_t frame, const vk::raii::CommandBuffer& buffer) const -> void {
		for (auto i : std::views::iota(size_t{0}, color_images.at(frame).size())) {
			const auto color_barrier = vk::ImageMemoryBarrier{
				vk::AccessFlagBits{},
				vk::AccessFlagBits::eColorAttachmentWrite,
				color_initial_layouts[frame][i],
				color_attachments[frame][i].imageLayout,
				VK_QUEUE_FAMILY_IGNORED,
				VK_QUEUE_FAMILY_IGNORED,
				color_images[frame][i],
				vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}
			};

			buffer.pipelineBarrier(
				vk::PipelineStageFlagBits::eTopOfPipe,
				vk::PipelineStageFlagBits::eColorAttachmentOutput,
				vk::DependencyFlagBits{},
				nullptr,
				nullptr,
				color_barrier
			);
		}

		const auto depth_barrier = vk::ImageMemoryBarrier{
			vk::AccessFlagBits{},
			vk::AccessFlagBits::eDepthStencilAttachmentWrite,
			depth_initial_layout,
			depth_stencil_attachment.imageLayout,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			depth_stencil_image,
			vk::ImageSubresourceRange{
				vk::ImageAspectFlagBits::eDepth | (stencil_buffer ? vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits{}),
				0,
				VK_REMAINING_MIP_LEVELS,
				0,
				VK_REMAINING_ARRAY_LAYERS
			}
		};

		buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
			vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
			vk::DependencyFlagBits{},
			nullptr,
			nullptr,
			depth_barrier
		);

		buffer.beginRenderingKHR(get_rendering_info(frame));
	}

	auto end(uint32_t frame, const vk::raii::CommandBuffer& buffer) const -> void {
		buffer.endRenderingKHR();

		for (auto i : std::views::iota(size_t{0}, color_images.at(frame).size())) {
			const auto color_output_barrier = vk::ImageMemoryBarrier{
				vk::AccessFlagBits::eColorAttachmentWrite,
				vk::AccessFlagBits{},
				color_attachments[frame][i].imageLayout,
				color_final_layouts[frame][i],
				VK_QUEUE_FAMILY_IGNORED,
				VK_QUEUE_FAMILY_IGNORED,
				color_images[frame][i],
				vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}
			};

			buffer.pipelineBarrier(
				vk::PipelineStageFlagBits::eColorAttachmentOutput,
				vk::PipelineStageFlagBits::eBottomOfPipe,
				vk::DependencyFlagBits{},
				nullptr,
				nullptr,
				color_output_barrier
			);
		}

		const auto depth_barrier = vk::ImageMemoryBarrier{
			vk::AccessFlagBits::eDepthStencilAttachmentWrite,
			vk::AccessFlagBits{},
			depth_stencil_attachment.imageLayout,
			depth_final_layout,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			depth_stencil_image,
			vk::ImageSubresourceRange{
				vk::ImageAspectFlagBits::eDepth | (stencil_buffer ? vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits{}),
				0,
				VK_REMAINING_MIP_LEVELS,
				0,
				VK_REMAINING_ARRAY_LAYERS
			}
		};

		buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
			vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
			vk::DependencyFlagBits{},
			nullptr,
			nullptr,
			depth_barrier
		);
	}


private:
	std::vector<std::vector<vk::RenderingAttachmentInfoKHR>> color_attachments;
	std::vector<std::vector<vk::Image>> color_images;
	std::vector<std::vector<vk::ImageLayout>> color_initial_layouts;
	std::vector<std::vector<vk::ImageLayout>> color_final_layouts;

	vk::RenderingAttachmentInfoKHR depth_stencil_attachment;
	vk::Image depth_stencil_image;
	vk::ImageLayout depth_initial_layout;
	vk::ImageLayout depth_final_layout;
	bool stencil_buffer = false;

	vk::Rect2D area_rect;
};

} //namespace vkw

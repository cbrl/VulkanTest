module;

#include <algorithm>
#include <ranges>
#include <span>
#include <tuple>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module vkw.render_pass_single;

import vkw.image;

export namespace vkw {

class render_pass_single {
public:

	// Member Functions - Render Area
	//----------------------------------------------------------------------------------------------------

	[[nodiscard]]
	auto get_area() const noexcept -> vk::Rect2D {
		return area_rect;
	}

	auto set_area(vk::Rect2D area) -> void {
		area_rect = area;
	}

	// Member Functions - Frame Color Attachment
	//----------------------------------------------------------------------------------------------------

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

		const auto new_size = std::max<size_t>(color_attachments.size(), frame + 1);

		color_attachments.resize(new_size);
		color_attachments[frame].insert(color_attachments[frame].end(), info.begin(), info.end());

		color_images.resize(new_size);
		color_images[frame].insert(color_images[frame].end(), images.begin(), images.end());

		color_initial_layouts.resize(new_size);
		color_initial_layouts[frame].insert(color_initial_layouts[frame].end(), initial_layouts.begin(), initial_layouts.end());

		color_final_layouts.resize(new_size);
		color_final_layouts[frame].insert(color_final_layouts[frame].end(), final_layouts.begin(), final_layouts.end());

		rebuild_color_barriers();
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
		set_frame_color_attachments(static_cast<uint32_t>(color_attachments.size()), info, images, initial_layouts, final_layouts);
	}


	// Member Functions - Depth Stencil Attachment
	//----------------------------------------------------------------------------------------------------

	auto set_depth_stencil_attachment(
		uint32_t frame,
		vk::RenderingAttachmentInfoKHR info,
		const image& img,
		vk::ImageLayout initial_layout,
		vk::ImageLayout final_layout
	) -> void {
		const auto aspect_mask = vkw::util::format_to_aspect(img.get_info().create_info.format);
		assert(aspect_mask & vk::ImageAspectFlagBits::eDepth);

		const auto has_stencil = (aspect_mask & vk::ImageAspectFlagBits::eStencil) == vk::ImageAspectFlagBits::eStencil;
		set_depth_stencil_attachment(frame, info, *img.get_vk_image(), initial_layout, final_layout, has_stencil);
	}

	auto set_depth_stencil_attachment(
		uint32_t frame,
		vk::RenderingAttachmentInfoKHR info,
		vk::Image img,
		vk::ImageLayout initial_layout,
		vk::ImageLayout final_layout,
		bool has_stencil_buffer = false
	) -> void {
		const auto new_size = std::max<size_t>(depth_stencil_attachments.size(), frame + 1);

		depth_stencil_attachments.resize(new_size);
		depth_stencil_attachments[frame] = info;

		depth_stencil_images.resize(new_size);
		depth_stencil_images[frame] = img;

		depth_initial_layouts.resize(new_size);
		depth_initial_layouts[frame] = initial_layout;
		
		depth_final_layouts.resize(new_size);
		depth_final_layouts[frame] = final_layout;

		stencil_buffer.resize(new_size);
		stencil_buffer[frame] = has_stencil_buffer;

		rebuild_depth_barriers();
	}

	auto add_depth_stencil_attachment(
		vk::RenderingAttachmentInfoKHR info,
		const image& img,
		vk::ImageLayout initial_layout,
		vk::ImageLayout final_layout
	) -> void {
		set_depth_stencil_attachment(depth_stencil_attachments.size(), info, img, initial_layout, final_layout);
	}

	auto add_depth_stencil_attachment(
		vk::RenderingAttachmentInfoKHR info,
		vk::Image img,
		vk::ImageLayout initial_layout,
		vk::ImageLayout final_layout,
		bool has_stencil_buffer = false
	) -> void {
		set_depth_stencil_attachment(depth_stencil_attachments.size(), info, img, initial_layout, final_layout, has_stencil_buffer);
	}


	// Member Functions - Render Info
	//----------------------------------------------------------------------------------------------------

	[[nodiscard]]
	auto get_rendering_info(uint32_t frame, vk::RenderingFlagsKHR flags = {}) const -> vk::RenderingInfoKHR {
		assert(color_attachments.size() == depth_stencil_attachments.size());

		return vk::RenderingInfoKHR{
			flags,
			area_rect,
			1,
			0,
			color_attachments.at(frame),
			&depth_stencil_attachments.at(frame),
			stencil_buffer.at(frame) ? &depth_stencil_attachments.at(frame) : nullptr,
		};
	}


	// Member Functions - Begin/End Rendering
	//----------------------------------------------------------------------------------------------------

	auto begin(uint32_t frame, const vk::raii::CommandBuffer& buffer) const -> void {
		assert(color_attachments.size() == depth_stencil_attachments.size());

		buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
			vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
			vk::DependencyFlagBits{},
			nullptr,
			nullptr,
			depth_stencil_begin_barriers[frame]
		);

		buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTopOfPipe,
			vk::PipelineStageFlagBits::eColorAttachmentOutput,
			vk::DependencyFlagBits{},
			nullptr,
			nullptr,
			color_begin_barriers[frame]
		);

		buffer.beginRenderingKHR(get_rendering_info(frame));
	}

	auto end(uint32_t frame, const vk::raii::CommandBuffer& buffer) const -> void {
		assert(color_attachments.size() == depth_stencil_attachments.size());

		buffer.endRenderingKHR();

		buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
			vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
			vk::DependencyFlagBits{},
			nullptr,
			nullptr,
			depth_stencil_end_barriers[frame]
		);

		buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eColorAttachmentOutput,
			vk::PipelineStageFlagBits::eBottomOfPipe,
			vk::DependencyFlagBits{},
			nullptr,
			nullptr,
			color_end_barriers[frame]
		);
	}


private:

	auto rebuild_color_barriers() -> void {
		color_begin_barriers.clear();
		color_begin_barriers.resize(color_attachments.size());

		for (auto frame : std::views::iota(size_t{0}, color_begin_barriers.size())) {
			for (auto i : std::views::iota(size_t{0}, color_images.at(frame).size())) {
				color_begin_barriers[frame].push_back(
					std::get<2>(vkw::util::create_layout_barrier(
						color_images[frame][i],
						vk::ImageAspectFlagBits::eColor,
						color_initial_layouts[frame][i],
						color_attachments[frame][i].imageLayout
					))
				);
			}
		}

		color_end_barriers.clear();
		color_end_barriers.resize(color_attachments.size());

		for (auto frame : std::views::iota(size_t{0}, color_begin_barriers.size())) {
			for (auto i : std::views::iota(size_t{0}, color_images.at(frame).size())) {
				color_end_barriers[frame].push_back(
					std::get<2>(vkw::util::create_layout_barrier(
						color_images[frame][i],
						vk::ImageAspectFlagBits::eColor,
						color_attachments[frame][i].imageLayout,
						color_final_layouts[frame][i]
					))
				);
			}
		}
	}

	auto rebuild_depth_barriers() -> void {
		depth_stencil_begin_barriers.clear();
		depth_stencil_begin_barriers.reserve(depth_stencil_attachments.size());

		for (auto frame : std::views::iota(size_t{0}, depth_stencil_attachments.size())) {
			const auto aspect_flags = vk::ImageAspectFlagBits::eDepth | (stencil_buffer[frame] ? vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits{});

			depth_stencil_begin_barriers.push_back(
				std::get<2>(vkw::util::create_layout_barrier(
					depth_stencil_images[frame],
					aspect_flags,
					depth_initial_layouts[frame],
					depth_stencil_attachments[frame].imageLayout
				))
			);
		}

		depth_stencil_end_barriers.clear();
		depth_stencil_end_barriers.reserve(depth_stencil_attachments.size());

		for (auto frame : std::views::iota(size_t{0}, depth_stencil_attachments.size())) {
			const auto aspect_flags = vk::ImageAspectFlagBits::eDepth | (stencil_buffer[frame] ? vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits{});

			depth_stencil_end_barriers.push_back(
				std::get<2>(vkw::util::create_layout_barrier(
					depth_stencil_images[frame],
					aspect_flags,
					depth_stencil_attachments[frame].imageLayout,
					depth_final_layouts[frame]
				))
			);
		}
	}

	// Render area
	vk::Rect2D area_rect;

	// Color attachment info
	std::vector<std::vector<vk::RenderingAttachmentInfoKHR>> color_attachments;
	std::vector<std::vector<vk::Image>> color_images;
	std::vector<std::vector<vk::ImageLayout>> color_initial_layouts;
	std::vector<std::vector<vk::ImageLayout>> color_final_layouts;

	// Color attachment barriers
	std::vector<std::vector<vk::ImageMemoryBarrier>> color_begin_barriers;
	std::vector<std::vector<vk::ImageMemoryBarrier>> color_end_barriers;

	// Depth stencil attachment info
	std::vector<vk::RenderingAttachmentInfoKHR> depth_stencil_attachments;
	std::vector<vk::Image> depth_stencil_images;
	std::vector<vk::ImageLayout> depth_initial_layouts;
	std::vector<vk::ImageLayout> depth_final_layouts;
	std::vector<bool> stencil_buffer;

	// Depth stencil barriers
	std::vector<vk::ImageMemoryBarrier> depth_stencil_begin_barriers;
	std::vector<vk::ImageMemoryBarrier> depth_stencil_end_barriers;

};

} //namespace vkw

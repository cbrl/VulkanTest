module;

#include <algorithm>
#include <array>
#include <memory>
#include <ranges>
#include <span>
#include <tuple>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module vkw.render_pass_single;

import vkw.image;
import vkw.swapchain;
import vkw.util;


export namespace vkw {

struct color_attachment {
	std::shared_ptr<image_view> img_view;
	vk::ImageLayout             image_layout = vk::ImageLayout::eColorAttachmentOptimal;
	vk::AttachmentLoadOp        load_op = vk::AttachmentLoadOp::eClear;
	vk::AttachmentStoreOp       store_op = vk::AttachmentStoreOp::eStore;
	vk::ClearValue              clear_value = vk::ClearColorValue{std::array{0.0f, 0.0f, 0.0f, 1.0f}};

	std::shared_ptr<image_view> resolve_img_view;
	vk::ResolveModeFlagBits     resolve_mode = vk::ResolveModeFlagBits::eNone;
	vk::ImageLayout             resolve_image_layout = vk::ImageLayout::eUndefined;

	vk::ImageLayout             initial_layout = vk::ImageLayout::eUndefined;
	vk::ImageLayout             final_layout = vk::ImageLayout::ePresentSrcKHR;
};

struct depth_attachment {
	std::shared_ptr<image_view> img_view;
	vk::ImageLayout             image_layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
	vk::AttachmentLoadOp        load_op = vk::AttachmentLoadOp::eClear;
	vk::AttachmentStoreOp       store_op = vk::AttachmentStoreOp::eStore;
	vk::ClearValue              clear_value = vk::ClearDepthStencilValue{1.0f, 0};

	std::shared_ptr<image_view> resolve_img_view;
	vk::ResolveModeFlagBits     resolve_mode = vk::ResolveModeFlagBits::eNone;
	vk::ImageLayout             resolve_image_layout = vk::ImageLayout::eUndefined;

	vk::ImageLayout             initial_layout = vk::ImageLayout::eUndefined;
	vk::ImageLayout             final_layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
};

/// A full render pass consisting of a single pass. Utilizes VK_KHR_dynamic_render_pass
class render_pass_single {
public:
	[[nodiscard]]
	static auto create() -> std::shared_ptr<render_pass_single> {
		return std::make_shared<render_pass_single>();
	}


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

	auto set_frame_color_attachments(uint32_t frame, const color_attachment& attachment) -> void {
		set_frame_color_attachments(frame, std::span{&attachment, 1});
	}

	auto set_frame_color_attachments(uint32_t frame, std::span<const color_attachment> attachments) -> void {
		const auto new_size = std::max<size_t>(color_attachments.size(), frame + 1);

		color_attachments.resize(new_size);
		color_image_views.resize(new_size);
		color_resolve_image_views.resize(new_size);
		color_initial_layouts.resize(new_size);
		color_final_layouts.resize(new_size);

		for (const auto& attachment : attachments) {
			color_attachments[frame].push_back(vk::RenderingAttachmentInfoKHR{
				*attachment.img_view->get_vk_image_view(),
				attachment.image_layout,
				attachment.resolve_mode,
				attachment.resolve_img_view ? *attachment.resolve_img_view->get_vk_image_view() : vk::ImageView{},
				attachment.resolve_image_layout,
				attachment.load_op,
				attachment.store_op,
				attachment.clear_value
			});

			color_image_views[frame].push_back(attachment.img_view);
			color_resolve_image_views[frame].push_back(attachment.resolve_img_view);
			color_initial_layouts[frame].push_back(attachment.initial_layout);
			color_final_layouts[frame].push_back(attachment.final_layout);
		}

		rebuild_color_barriers();

		// Ensure the new attachment's formats are the same as the other frames
		#ifndef NDEBUG
		using namespace std::ranges;
		auto as_formats = views::transform([](auto&& view) { return view->get_info().format; });
		auto as_format_vecs = views::transform([&](auto&& vec) { return vec | as_formats; });
		auto is_permutation_of_new_frame = [&](auto&& vec) { return is_permutation(vec, color_image_views.back() | as_formats); };
		assert(all_of(color_image_views | views::take(color_image_views.size() - 1) | as_format_vecs, is_permutation_of_new_frame));
		#endif //NDEBUG
	}

	auto add_frame_color_attachments(const color_attachment& attachment) -> void {
		add_frame_color_attachments(std::span{&attachment, 1});
	}

	auto add_frame_color_attachments(std::span<const color_attachment> attachments) -> void {
		set_frame_color_attachments(static_cast<uint32_t>(color_attachments.size()), attachments);
	}

	auto add_frame_color_attachments(std::span<const std::shared_ptr<image_view>> views, color_attachment attachment) -> void {
		for (const auto& view : views) {
			attachment.img_view = view;
			add_frame_color_attachments(attachment);
		}
	}

	auto add_frame_color_attachments(std::shared_ptr<swapchain> swap, color_attachment attachment) -> void {
		for (auto i : std::views::iota(size_t{0}, swap->get_image_count())) {
			attachment.img_view = swap->get_image_view(i);
			add_frame_color_attachments(attachment);
		}
	}


	// Member Functions - Depth Stencil Attachment
	//----------------------------------------------------------------------------------------------------

	auto set_depth_stencil_attachment(uint32_t frame, const depth_attachment& attachment) -> void {
		assert(std::ranges::all_of(depth_stencil_image_views, [&](auto&& view) { return view->get_info().format == attachment.img_view->get_info().format; }));
		assert(attachment.img_view->get_info().subresource_range.aspectMask & vk::ImageAspectFlagBits::eDepth);

		const auto new_size = std::max<size_t>(depth_stencil_attachments.size(), frame + 1);

		depth_stencil_attachments.resize(new_size);
		depth_stencil_attachments[frame] = vk::RenderingAttachmentInfoKHR{
			*attachment.img_view->get_vk_image_view(),
			attachment.image_layout,
			attachment.resolve_mode,
			attachment.resolve_img_view ? *attachment.resolve_img_view->get_vk_image_view() : vk::ImageView{},
			attachment.resolve_image_layout,
			attachment.load_op,
			attachment.store_op,
			attachment.clear_value
		};

		depth_stencil_image_views.resize(new_size);
		depth_stencil_image_views[frame] = attachment.img_view;

		depth_stencil_resolve_image_views.resize(new_size);
		depth_stencil_resolve_image_views[frame] = attachment.resolve_img_view;

		depth_initial_layouts.resize(new_size);
		depth_initial_layouts[frame] = attachment.initial_layout;

		depth_final_layouts.resize(new_size);
		depth_final_layouts[frame] = attachment.final_layout;

		stencil_buffer.resize(new_size);
		stencil_buffer[frame] = (attachment.img_view->get_info().subresource_range.aspectMask & vk::ImageAspectFlagBits::eStencil) == vk::ImageAspectFlagBits::eStencil;

		rebuild_depth_barriers();
	}

	auto add_depth_stencil_attachment(const depth_attachment& attachment) -> void {
		set_depth_stencil_attachment(depth_stencil_attachments.size(), attachment);
	}

	auto add_depth_stencil_attachments(std::span<const std::shared_ptr<image_view>> views, depth_attachment attachment) -> void {
		for (const auto& view : views) {
			attachment.img_view = view;
			add_depth_stencil_attachment(attachment);
		}
	}


	// Member Functions - Image Formats
	//----------------------------------------------------------------------------------------------------

	[[nodiscard]]
	auto get_color_formats() const -> std::vector<vk::Format> {
		using namespace std::views;

		if (std::ranges::all_of(color_image_views, [](auto&& vec) { return vec.empty(); }) == 0) {
			return {vk::Format::eUndefined};
		}

		// All frames should have the same attachment formats
		auto result = ranges::to<std::vector>(color_image_views.front() | transform([](auto&& view) { return view->get_info().format; }));

		const auto [ret, last] = std::ranges::unique(result);
		result.erase(ret, last);

		return result;
	}

	[[nodiscard]]
	auto get_depth_stencil_format() const -> vk::Format {
		// All frames must have depth attachments with the same format
		return depth_stencil_image_views.empty() ? vk::Format::eUndefined : depth_stencil_image_views[0]->get_info().format;
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
			for (auto i : std::views::iota(size_t{0}, color_image_views.at(frame).size())) {
				color_begin_barriers[frame].push_back(
					std::get<2>(vkw::util::create_layout_barrier(
						*color_image_views[frame][i]->get_image()->get_vk_image(),
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
			for (auto i : std::views::iota(size_t{0}, color_image_views.at(frame).size())) {
				color_end_barriers[frame].push_back(
					std::get<2>(vkw::util::create_layout_barrier(
						*color_image_views[frame][i]->get_image()->get_vk_image(),
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
					*depth_stencil_image_views[frame]->get_image()->get_vk_image(),
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
					*depth_stencil_image_views[frame]->get_image()->get_vk_image(),
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
	std::vector<std::vector<std::shared_ptr<image_view>>> color_image_views;
	std::vector<std::vector<std::shared_ptr<image_view>>> color_resolve_image_views;
	std::vector<std::vector<vk::ImageLayout>> color_initial_layouts;
	std::vector<std::vector<vk::ImageLayout>> color_final_layouts;

	// Color attachment barriers
	std::vector<std::vector<vk::ImageMemoryBarrier>> color_begin_barriers;
	std::vector<std::vector<vk::ImageMemoryBarrier>> color_end_barriers;

	// Depth stencil attachment info
	std::vector<vk::RenderingAttachmentInfoKHR> depth_stencil_attachments;
	std::vector<std::shared_ptr<image_view>> depth_stencil_image_views;
	std::vector<std::shared_ptr<image_view>> depth_stencil_resolve_image_views;
	std::vector<vk::ImageLayout> depth_initial_layouts;
	std::vector<vk::ImageLayout> depth_final_layouts;
	std::vector<bool> stencil_buffer;

	// Depth stencil barriers
	std::vector<vk::ImageMemoryBarrier> depth_stencil_begin_barriers;
	std::vector<vk::ImageMemoryBarrier> depth_stencil_end_barriers;
};

} //namespace vkw

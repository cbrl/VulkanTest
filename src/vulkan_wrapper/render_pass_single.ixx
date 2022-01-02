module;

#include <ranges>
#include <span>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module vkw.render_pass_single;


export namespace vkw {

class render_pass_single {
public:
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

	auto begin(uint32_t frame, const vk::raii::CommandBuffer& buffer, vk::Image color_image, vk::Image depth_stencil_image) const -> void {
		begin(frame, buffer, std::span{&color_image, 1}, depth_stencil_image);
	}

	auto begin(
		uint32_t frame,
		const vk::raii::CommandBuffer& buffer,
		std::span<const vk::Image> color_images,
		vk::Image depth_stencil_image
	) const -> void {
		for (auto i : std::views::iota(size_t{0}, color_images.size())) {
			const auto color_barrier = vk::ImageMemoryBarrier{
				vk::AccessFlagBits{},
				vk::AccessFlagBits::eColorAttachmentWrite,
				color_initial_layouts.at(frame).at(i),
				color_attachments.at(frame).at(i).imageLayout,
				VK_QUEUE_FAMILY_IGNORED,
				VK_QUEUE_FAMILY_IGNORED,
				color_images[i],
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


	auto end(uint32_t frame, const vk::raii::CommandBuffer& buffer, vk::Image color_image, vk::Image depth_stencil_image) const -> void {
		end(frame, buffer, std::span{&color_image, 1}, depth_stencil_image);
	}

	auto end(
		uint32_t frame,
		const vk::raii::CommandBuffer& buffer,
		std::span<const vk::Image> color_images,
		vk::Image depth_stencil_image
	) const -> void {
		buffer.endRenderingKHR();

		for (auto i : std::views::iota(size_t{0}, color_images.size())) {
			const auto color_output_barrier = vk::ImageMemoryBarrier{
				vk::AccessFlagBits::eColorAttachmentWrite,
				vk::AccessFlagBits{},
				color_attachments.at(frame).at(i).imageLayout,
				color_final_layouts.at(frame).at(i),
				VK_QUEUE_FAMILY_IGNORED,
				VK_QUEUE_FAMILY_IGNORED,
				color_images[i],
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


	std::vector<std::vector<vk::RenderingAttachmentInfoKHR>> color_attachments;
	std::vector<std::vector<vk::ImageLayout>> color_initial_layouts;
	std::vector<std::vector<vk::ImageLayout>> color_final_layouts;

	vk::RenderingAttachmentInfoKHR depth_stencil_attachment;
	vk::ImageLayout depth_initial_layout;
	vk::ImageLayout depth_final_layout;
	bool stencil_buffer = false;

	vk::Rect2D area_rect;
};

} //namespace vkw

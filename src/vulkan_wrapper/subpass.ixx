module;

#include <cstdint>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module vkw.subpass;

namespace vkw {

class subpass_base {
	friend class subpass;

public:
	[[nodiscard]]
	auto get_description() const -> const vk::SubpassDescription& {
		return description;
	}

	auto set_flags(vk::SubpassDescriptionFlags flags) noexcept -> void {
		description.flags = flags;
	}

	auto set_bind_point(vk::PipelineBindPoint bind_point) noexcept -> void {
		description.pipelineBindPoint = bind_point;
	}


	// Color Attachments
	//--------------------------------------------------------------------------------
	auto add_color_attachment(const vk::AttachmentReference& attachment) -> void {
		color_attachments.push_back(attachment);
		update_color_attachments();
	}

	auto set_color_attachment(const vk::AttachmentReference& attachment) -> void {
		color_attachments.clear();
		color_attachments.push_back(attachment);
		update_color_attachments();
	}

	auto set_color_attachments(const std::vector<vk::AttachmentReference>& attachments) -> void {
		color_attachments = attachments;
		update_color_attachments();
	}


	// Input Attachments
	//--------------------------------------------------------------------------------
	auto add_input_attachment(const vk::AttachmentReference& attachment) -> void {
		input_attachments.push_back(attachment);
		update_input_attachments();
	}

	auto set_input_attachment(const vk::AttachmentReference& attachment) -> void {
		input_attachments.clear();
		input_attachments.push_back(attachment);
		update_input_attachments();
	}

	auto set_input_attachments(const std::vector<vk::AttachmentReference>& attachments) -> void {
		input_attachments = attachments;
		update_input_attachments();
	}


	// Resolve Attachments
	//--------------------------------------------------------------------------------
	auto add_resolve_attachment(const vk::AttachmentReference& attachment) -> void {
		resolve_attachments.push_back(attachment);
		update_resolve_attachments();
	}

	auto set_resolve_attachment(const vk::AttachmentReference& attachment) -> void {
		resolve_attachments.clear();
		resolve_attachments.push_back(attachment);
		update_resolve_attachments();
	}

	auto set_resolve_attachments(const std::vector<vk::AttachmentReference>& attachments) -> void {
		resolve_attachments = attachments;
		update_resolve_attachments();
	}


	// Depth Stencil Attachment
	//--------------------------------------------------------------------------------
	auto set_depth_stencil_attachment(const vk::AttachmentReference& attachment) -> void {
		depth_stencil_attachment = attachment;
		update_depth_stencil_attachment();
	}


	// Preserve Attachments
	//--------------------------------------------------------------------------------
	auto add_preserve_attachment(uint32_t attachment) -> void {
		preserve_attachments.push_back(attachment);
		update_preserve_attachments();
	}

	auto set_preserve_attachment(uint32_t attachment) -> void {
		preserve_attachments.clear();
		preserve_attachments.push_back(attachment);
		update_preserve_attachments();
	}

	auto set_preserve_attachments(const std::vector<uint32_t>& attachments) -> void {
		preserve_attachments = attachments;
		update_preserve_attachments();
	}

private:

	auto update_color_attachments() noexcept -> void {
		description.setColorAttachments(color_attachments);
	}

	auto update_depth_stencil_attachment() noexcept -> void {
		description.setPDepthStencilAttachment(&depth_stencil_attachment);
	}

	auto update_input_attachments() noexcept -> void {
		description.setInputAttachments(input_attachments);
	}

	auto update_resolve_attachments() noexcept -> void {
		description.setResolveAttachments(resolve_attachments);
	}

	auto update_preserve_attachments() noexcept -> void {
		description.setPreserveAttachments(preserve_attachments);

	}

	vk::SubpassDescription description = {};

	std::vector<vk::AttachmentReference> color_attachments;
	std::vector<vk::AttachmentReference> input_attachments;
	std::vector<vk::AttachmentReference> resolve_attachments;
	vk::AttachmentReference depth_stencil_attachment = {};
	std::vector<uint32_t> preserve_attachments;
};


export class subpass : public subpass_base {
public:
	subpass() = default;

	subpass(const subpass& other) : subpass_base(other) {
		update_color_attachments();
		update_depth_stencil_attachment();
		update_input_attachments();
		update_resolve_attachments();
		update_preserve_attachments();
	}

	// The default move constrctor is fine, because the destination vectors will take ownership of the
	// moved-from vectors' data pointers, instead of copying the data to another array.
	subpass(subpass&&) noexcept = default;

	~subpass() = default;

	auto operator=(const subpass& other) noexcept -> subpass& {
		subpass_base::operator=(other);
		update_color_attachments();
		update_depth_stencil_attachment();
		update_input_attachments();
		update_resolve_attachments();
		update_preserve_attachments();
		return *this;
	}

	auto operator=(subpass&&) noexcept -> subpass& = default;
};

} //namespace vkw

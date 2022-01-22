module;

#include <memory>
#include <span>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module vkw.shader;

import vkw.logical_device;


namespace vkw {

[[nodiscard]]
static auto create_shader_module(const logical_device& device, std::span<const uint32_t> data) -> vk::raii::ShaderModule {
	const auto create_info = vk::ShaderModuleCreateInfo{
		vk::ShaderModuleCreateFlags{},
		data.size_bytes(),
		data.data()
	};

	return vk::raii::ShaderModule{device.get_vk_device(), create_info};
}

export class shader_stage {
public:
	static constexpr const char* entry_point_name = "main";

	[[nodiscard]]
	static auto create(
		std::shared_ptr<logical_device> device,
		vk::ShaderStageFlagBits stage,
		std::span<const uint32_t> shader_data,
		std::span<const vk::SpecializationMapEntry> specializations = {},
		std::span<const uint32_t> specialization_data = {}
	) -> std::shared_ptr<shader_stage> {
		return std::make_shared<shader_stage>(std::move(device), stage, shader_data, specializations, specialization_data);
	}

	shader_stage(
		std::shared_ptr<logical_device> logic_device,
		vk::ShaderStageFlagBits stage,
		std::span<const uint32_t> shader_data,
		std::span<const vk::SpecializationMapEntry> specializations = {},
		std::span<const uint32_t> specialization_data = {}
	) :
		device(std::move(logic_device)),
		specialization_entries(specializations.begin(), specializations.end()),
		specialization_data(specialization_data.begin(), specialization_data.end()),
		shader_module(create_shader_module(*device, shader_data)) {

		create_info.flags               = vk::PipelineShaderStageCreateFlags{};
		create_info.stage               = stage;
		create_info.module              = *shader_module;
		create_info.pName               = entry_point_name;
		create_info.pSpecializationInfo = &specialization_info;

		specialization_info.setMapEntries(this->specialization_entries);
		specialization_info.setData<uint32_t>(this->specialization_data);
	}

	[[nodiscard]]
	auto get_create_info() const noexcept -> const vk::PipelineShaderStageCreateInfo& {
		return create_info;
	}

	[[nodiscard]]
	auto get_specialization_info() const noexcept -> const vk::SpecializationInfo& {
		return specialization_info;
	}

	[[nodiscard]]
	auto get_specialization_entries() const noexcept -> const std::vector<vk::SpecializationMapEntry>& {
		return specialization_entries;
	}

private:
	std::shared_ptr<logical_device> device;

	vk::PipelineShaderStageCreateInfo create_info;
	vk::SpecializationInfo specialization_info;
	std::vector<vk::SpecializationMapEntry> specialization_entries;
	std::vector<uint32_t> specialization_data;

	vk::raii::ShaderModule shader_module;
};

} //namespace vkw

module;

#include <span>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module vkw.shader;

import vkw.logical_device;


export namespace vkw {

class shader_stage {
public:
	static constexpr const char* entry_point_name = "main";

	shader_stage(
		const logical_device& device,
		vk::ShaderStageFlagBits stage,
		std::span<const uint32_t> shader_data,
		std::span<const vk::SpecializationMapEntry> specializations = {},
		std::span<const uint32_t> specialization_data = {}
	) :
		specialization_entries(specializations.begin(), specializations.end()),
		specialization_data(specialization_data.begin(), specialization_data.end()),
		shader_module(create_shader_module(device, shader_data)) {

		create_info.flags               = vk::PipelineShaderStageCreateFlags{};
		create_info.stage               = stage;
		create_info.module              = *shader_module;
		create_info.pName               = entry_point_name;
		create_info.pSpecializationInfo = &specialization_info;

		specialization_info.mapEntryCount = static_cast<uint32_t>(specialization_entries.size());
		specialization_info.pMapEntries   = specialization_entries.data();
		specialization_info.dataSize      = specialization_data.size_bytes();
		specialization_info.pData         = this->specialization_data.data();
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

	[[nodiscard]]
	static auto create_shader_module(const logical_device& device, std::span<const uint32_t> data) -> vk::raii::ShaderModule {
		const auto create_info = vk::ShaderModuleCreateInfo{
			vk::ShaderModuleCreateFlags{},
			data.size_bytes(),
			data.data()
		};

		return vk::raii::ShaderModule{device.get_vk_device(), create_info};
	}

	vk::PipelineShaderStageCreateInfo create_info;
	vk::SpecializationInfo specialization_info;
	std::vector<vk::SpecializationMapEntry> specialization_entries;
	std::vector<uint32_t> specialization_data;

	vk::raii::ShaderModule shader_module;
};

} //namespace vkw
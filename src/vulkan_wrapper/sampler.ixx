module;

#include <memory>
#include <vulkan/vulkan_raii.hpp>

export module vkw.sampler;

import vkw.logical_device;


export namespace vkw {
struct sampler_info {
	vk::Filter              min_filter = vk::Filter::eLinear;
	vk::Filter              mag_filter = vk::Filter::eLinear;
	vk::SamplerMipmapMode   sampler_mipmap_mode = vk::SamplerMipmapMode::eLinear;
	vk::SamplerAddressMode  sampler_address_mode = vk::SamplerAddressMode::eRepeat;
	bool                    request_anisotropy = true;
};
} //namespace vkw


[[nodiscard]]
auto build_sampler_create_info(const vkw::logical_device& device, const vkw::sampler_info& info) -> vk::SamplerCreateInfo {
	const auto features   = device.get_vk_physical_device()->getFeatures();
	const auto properties = device.get_vk_physical_device()->getProperties();

	return vk::SamplerCreateInfo{
		vk::SamplerCreateFlags{},
		info.mag_filter,
		info.min_filter,
		info.sampler_mipmap_mode,
		info.sampler_address_mode,
		info.sampler_address_mode,
		info.sampler_address_mode,
		0.0f,
		info.request_anisotropy && features.samplerAnisotropy,
		properties.limits.maxSamplerAnisotropy,
		false,
		vk::CompareOp::eNever,
		0.0f,
		VK_LOD_CLAMP_NONE,
		vk::BorderColor::eFloatOpaqueBlack,
		false
	};
}


export namespace vkw {

class sampler {
public:
	[[nodiscard]]
	static auto create(std::shared_ptr<logical_device> device, const sampler_info& info) -> std::shared_ptr<sampler> {
		return std::make_shared<sampler>(std::move(device), info);
	}

	sampler(std::shared_ptr<logical_device> logic_device, const sampler_info& sample_info) :
		device(std::move(logic_device)),
		info(build_sampler_create_info(*device, sample_info)),
		handle(device->get_vk_device(), info) {
	}

	[[nodiscard]]
	auto get_info() const noexcept -> const vk::SamplerCreateInfo& {
		return info;
	}

	[[nodiscard]]
	auto get_vk_sampler() const noexcept -> const vk::raii::Sampler& {
		return handle;
	}

private:
	std::shared_ptr<logical_device> device;
	vk::SamplerCreateInfo info;
	vk::raii::Sampler handle;
};

} //namespace vkw

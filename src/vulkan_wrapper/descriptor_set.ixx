module;

#include <memory>
#include <numeric>
#include <ranges>
#include <set>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

#include "descriptor_pool_fwd.h"

export module vkw.descriptor_set;

import vkw.buffer;
import vkw.logical_device;
import vkw.sampler;
import vkw.texture;
import vkw.util;


template<typename T>
struct write_descriptor_set {
	uint32_t binding;
	std::vector<std::reference_wrapper<const T>> data;
};


export namespace vkw {

using write_image_set        = write_descriptor_set<texture>;
using write_sampler_set      = write_descriptor_set<sampler>;
using write_buffer_set       = write_descriptor_set<buffer<void>>;
using write_texel_buffer_set = write_descriptor_set<vk::raii::BufferView>;


class descriptor_set_layout {
private:
	struct binding_comparator {
		using is_transparent = void;

		constexpr auto operator()(const vk::DescriptorSetLayoutBinding& lhs, const vk::DescriptorSetLayoutBinding& rhs) const -> bool {
			return lhs.binding < rhs.binding;
		}
		constexpr auto operator()(uint32_t lhs, const vk::DescriptorSetLayoutBinding& rhs) const -> bool {
			return lhs < rhs.binding;
		}
		constexpr auto operator()(const vk::DescriptorSetLayoutBinding& lhs, uint32_t rhs) const -> bool {
			return lhs.binding < rhs;
		}
	};

public:
	[[nodiscard]]
	static auto create(
		std::shared_ptr<logical_device> device,
		std::span<const vk::DescriptorSetLayoutBinding> layout_bindings,
		vk::DescriptorSetLayoutCreateFlags flags = {}
	) -> std::shared_ptr<descriptor_set_layout> {
		return std::make_shared<descriptor_set_layout>(std::move(device), layout_bindings, flags);
	}

	descriptor_set_layout(
		std::shared_ptr<logical_device> logic_device,
		std::span<const vk::DescriptorSetLayoutBinding> layout_bindings,
		vk::DescriptorSetLayoutCreateFlags flags = {}
	) :
		device(std::move(logic_device)),
		layout(nullptr),
		bindings(layout_bindings.begin(), layout_bindings.end()) {

		const auto create_info = vk::DescriptorSetLayoutCreateInfo{flags, layout_bindings};
		layout = vk::raii::DescriptorSetLayout{device->get_vk_device(), create_info};
	}

	[[nodiscard]]
	auto get_vk_layout() const noexcept -> const vk::raii::DescriptorSetLayout& {
		return layout;
	}

	[[nodiscard]]
	auto get_bindings() const noexcept -> const std::set<vk::DescriptorSetLayoutBinding, binding_comparator>& {
		return bindings;
	}

private:
	std::shared_ptr<logical_device> device;

	vk::raii::DescriptorSetLayout layout;
	std::set<vk::DescriptorSetLayoutBinding, binding_comparator> bindings;
};


class descriptor_set {
	friend class descriptor_pool;

	using write_set_variant = std::variant<write_image_set, write_sampler_set, write_buffer_set, write_texel_buffer_set>;

public:
	[[nodiscard]]
	static auto create(
		std::shared_ptr<logical_device> device,
		std::shared_ptr<descriptor_pool> pool,
		std::shared_ptr<descriptor_set_layout> layout,
		vk::raii::DescriptorSet&& handle
	) -> std::shared_ptr<descriptor_set> {
		return std::make_shared<descriptor_set>(std::move(device), std::move(pool), std::move(layout), std::move(handle));
	}

	descriptor_set(
		std::shared_ptr<logical_device> device,
		std::shared_ptr<descriptor_pool> pool,
		std::shared_ptr<descriptor_set_layout> layout,
		vk::raii::DescriptorSet&& handle
	) :
		device(std::move(device)),
		pool(std::move(pool)),
		layout(std::move(layout)),
		handle(std::move(handle)) {
	}

	[[nodiscard]]
	auto get_vk_descriptor_set() const noexcept -> const vk::raii::DescriptorSet& {
		return handle;
	}

	auto update(const write_image_set& images) -> void {
		const auto texture_infos = vkw::util::to_vector(std::views::transform(images.data, [](const texture& tex) {
			return vk::DescriptorImageInfo{
				vk::Sampler{},
				*tex.get_image_view()->get_vk_image_view(),
				vk::ImageLayout::eShaderReadOnlyOptimal
			};
		}));

		const auto write_descriptor_set = vk::WriteDescriptorSet{
			*handle,
			images.binding,
			0,
			get_binding(images.binding).descriptorType,
			texture_infos,
		};

		device->get_vk_device().updateDescriptorSets(write_descriptor_set, nullptr);
	}

	auto update(const write_sampler_set& samplers) -> void {
		const auto sampler_infos = vkw::util::to_vector(std::views::transform(samplers.data, [](const sampler& samp) {
			return vk::DescriptorImageInfo{
				*samp.get_vk_sampler(),
				vk::ImageView{},
				vk::ImageLayout::eShaderReadOnlyOptimal
			};
		}));

		const auto write_descriptor_set = vk::WriteDescriptorSet{
			*handle,
			samplers.binding,
			0,
			get_binding(samplers.binding).descriptorType,
			sampler_infos,
		};

		device->get_vk_device().updateDescriptorSets(write_descriptor_set, nullptr);
	}

	auto update(const write_buffer_set& buffers) -> void {
		const auto buffer_infos = vkw::util::to_vector(std::views::transform(buffers.data, [](const buffer<>& buf) {
			return vk::DescriptorBufferInfo{*buf.get_vk_buffer(), 0, VK_WHOLE_SIZE};
		}));

		const auto write_descriptor_set = vk::WriteDescriptorSet{
			*handle,
			buffers.binding,
			0,
			get_binding(buffers.binding).descriptorType,
			nullptr,
			buffer_infos
		};

		device->get_vk_device().updateDescriptorSets(write_descriptor_set, nullptr);
	}

	auto update(const write_texel_buffer_set& buffers) -> void {
		const auto view_handles = vkw::util::to_vector(vkw::util::as_handles(buffers.data));

		const auto write_descriptor_set = vk::WriteDescriptorSet{
			*handle,
			buffers.binding,
			0,
			get_binding(buffers.binding).descriptorType,
			nullptr,
			nullptr,
			view_handles
		};
	
		device->get_vk_device().updateDescriptorSets(write_descriptor_set, nullptr);
	}

	auto update(const std::vector<write_set_variant>& buffer_data) -> void {
		auto image_infos        = std::vector<std::vector<vk::DescriptorImageInfo>>{};
		auto sampler_infos      = std::vector<std::vector<vk::DescriptorImageInfo>>{};
		auto buffer_infos       = std::vector<std::vector<vk::DescriptorBufferInfo>>{};
		auto texel_buffer_infos = std::vector<std::vector<vk::BufferView>>{};

		auto write_descriptor_sets = std::vector<vk::WriteDescriptorSet>{};
		write_descriptor_sets.reserve(buffer_data.size());

		for (const auto& write_set : buffer_data) {
			auto& set_info = write_descriptor_sets.emplace_back(vk::WriteDescriptorSet{*handle, 0, 0});

			if (const auto* image_set = std::get_if<write_image_set>(&write_set)) {
				set_info.setDstBinding(image_set->binding);
				set_info.setDescriptorType(get_binding(image_set->binding).descriptorType);

				auto& info_vec = image_infos.emplace_back();
				for (const texture& tex : image_set->data) {
					info_vec.push_back(vk::DescriptorImageInfo{
						vk::Sampler{},
						*tex.get_image_view()->get_vk_image_view(),
						vk::ImageLayout::eShaderReadOnlyOptimal
					});
				}

				set_info.setImageInfo(info_vec);
			}
			else if (const auto* sampler_set = std::get_if<write_sampler_set>(&write_set)) {
				set_info.setDstBinding(sampler_set->binding);
				set_info.setDescriptorType(get_binding(sampler_set->binding).descriptorType);

				auto& info_vec = sampler_infos.emplace_back();
				for (const sampler& samp : sampler_set->data) {
					info_vec.push_back(vk::DescriptorImageInfo{
						*samp.get_vk_sampler(),
						vk::ImageView{},
						vk::ImageLayout::eShaderReadOnlyOptimal
					});
				}

				set_info.setImageInfo(info_vec);
			}
			else if (const auto* buffer_set = std::get_if<write_buffer_set>(&write_set)) {
				set_info.setDstBinding(buffer_set->binding);
				set_info.setDescriptorType(get_binding(buffer_set->binding).descriptorType);

				auto& info_vec = buffer_infos.emplace_back();
				for (const buffer<void>& buffer : buffer_set->data) {
					info_vec.push_back(vk::DescriptorBufferInfo{*buffer.get_vk_buffer(), 0, VK_WHOLE_SIZE});
				}

				set_info.setBufferInfo(info_vec);
			}
			else if (const auto* texel_buffer_set = std::get_if<write_texel_buffer_set>(&write_set)) {
				set_info.setDstBinding(texel_buffer_set->binding);
				set_info.setDescriptorType(get_binding(texel_buffer_set->binding).descriptorType);

				auto& view_vec = texel_buffer_infos.emplace_back();
				view_vec = vkw::util::to_vector(vkw::util::as_handles(texel_buffer_set->data));

				set_info.setTexelBufferView(view_vec);
			}
		}

		device->get_vk_device().updateDescriptorSets(write_descriptor_sets, nullptr);
	}

private:

	[[nodiscard]]
	auto get_binding(uint32_t binding_index) const -> const vk::DescriptorSetLayoutBinding& {
		assert(layout->get_bindings().contains(binding_index));
		return *layout->get_bindings().find(binding_index);
	}

	std::shared_ptr<logical_device> device;
	std::shared_ptr<descriptor_pool> pool;
	std::shared_ptr<descriptor_set_layout> layout;

	vk::raii::DescriptorSet handle;
};

} //namespace vkw
module;

#include <numeric>
#include <ranges>
#include <span>
#include <set>
#include <utility>
#include <variant>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module vkw.descriptor_set;

import vkw.buffer;
import vkw.logical_device;
import vkw.texture;
import vkw.util;


template<typename T>
struct write_descriptor_set {
	uint32_t binding;
	std::vector<std::reference_wrapper<const T>> data;
};


export namespace vkw {

using write_image_set        = write_descriptor_set<texture>;
using write_buffer_set       = write_descriptor_set<buffer<void>>;
using write_texel_buffer_set = write_descriptor_set<vk::raii::BufferView>;

class descriptor_set {
	using write_set_variant = std::variant<write_image_set, write_buffer_set, write_texel_buffer_set>;

	struct descriptor_binding_comparator {
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
	descriptor_set(
		vk::raii::DescriptorSet&& handle,
		std::span<const vk::DescriptorSetLayoutBinding> layout_bindings
	) :
		handle(std::move(handle)),
		bindings(layout_bindings.begin(), layout_bindings.end()) {
	}

	[[nodiscard]]
	auto get_vk_descriptor_set() const noexcept -> const vk::raii::DescriptorSet& {
		return handle;
	}

	auto update(const logical_device& device, const write_image_set& images) -> void {
		const auto texture_infos = vkw::util::to_vector(std::views::transform(images.data, [](const texture& tex) {
			return vk::DescriptorImageInfo{
				*tex.get_sampler(),
				*tex.get_image().get_vk_image_view(),
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

		device.get_vk_device().updateDescriptorSets(write_descriptor_set, nullptr);
	}

	auto update(const logical_device& device, const write_buffer_set& buffers) -> void {
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

		device.get_vk_device().updateDescriptorSets(write_descriptor_set, nullptr);
	}

	auto update(const logical_device& device, const write_texel_buffer_set& buffers) -> void {
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
	
		device.get_vk_device().updateDescriptorSets(write_descriptor_set, nullptr);
	}

	auto update(const logical_device& device, const std::vector<write_set_variant>& buffer_data) -> void {
		auto image_infos        = std::vector<std::vector<vk::DescriptorImageInfo>>{};
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
						*tex.get_sampler(),
						*tex.get_image().get_vk_image_view(),
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

		device.get_vk_device().updateDescriptorSets(write_descriptor_sets, nullptr);
	}

private:

	[[nodiscard]]
	auto get_binding(uint32_t binding_index) const -> const vk::DescriptorSetLayoutBinding& {
		assert(bindings.contains(binding_index));
		return *bindings.find(binding_index);
	}

	vk::raii::DescriptorSet handle;
	std::set<vk::DescriptorSetLayoutBinding, descriptor_binding_comparator> bindings;
};

} //namespace vkw
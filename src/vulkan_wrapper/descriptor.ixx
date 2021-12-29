module;

#include <numeric>
#include <ranges>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module vkw.descriptor;

import vkw.logical_device;
import vkw.util;


export namespace vkw {
	
class descriptor_set_layout {
public:
	descriptor_set_layout(
		const logical_device& device,
		std::span<const vk::DescriptorSetLayoutBinding> layout_bindings,
		vk::DescriptorSetLayoutCreateFlags flags = {}
	) :
		layout(create_layout(device, layout_bindings, flags)),
		bindings(layout_bindings.begin(), layout_bindings.end()) {
	}

	[[nodiscard]]
	auto get_vk_layout() const noexcept -> const vk::raii::DescriptorSetLayout& {
		return layout;
	}

	[[nodiscard]]
	auto get_bindings() const noexcept -> const std::vector<vk::DescriptorSetLayoutBinding>& {
		return bindings;
	}

private:
	static auto create_layout(
		const logical_device& device,
		std::span<const vk::DescriptorSetLayoutBinding> bindings,
		vk::DescriptorSetLayoutCreateFlags flags = {}
	) -> vk::raii::DescriptorSetLayout {

		const auto create_info = vk::DescriptorSetLayoutCreateInfo{flags, bindings};
		return vk::raii::DescriptorSetLayout{device.get_vk_device(), create_info};
	}

	vk::raii::DescriptorSetLayout layout;
	std::vector<vk::DescriptorSetLayoutBinding> bindings;
};


class descriptor_set {
	template<typename T>
	struct write_descriptor_set {
		vk::DescriptorType descriptor_type;
		std::vector<std::reference_wrapper<const T>> data;
	};

public:
	//using write_image_set        = write_descriptor_set<texture>;
	using write_buffer_set       = write_descriptor_set<vk::raii::Buffer>;
	using write_texel_buffer_set = write_descriptor_set<vk::raii::BufferView>;

	descriptor_set(vk::raii::DescriptorSet&& handle) : handle(std::move(handle)) {
	}

	[[nodiscard]]
	auto get_vk_descriptor_set() const noexcept -> const vk::raii::DescriptorSet& {
		return handle;
	}

	/*
	auto update(
		const logical_device& device,
		vk::DescriptorType type,
		std::span<const texture> textures,
		uint32_t binding_offset = 0
	) -> void {
		const auto texture_infos = vkw::util::to_vector(std::views::transform(textures, [](auto&& tex) {
			return vk::DescriptorImageInfo{
				*tex.sampler,
				*tex.image_data->image_view,
				vk::ImageLayout::eShaderReadOnlyOptimal
			};
		}));

		const auto write_descriptor_set = vk::WriteDescriptorSet{
			*handle,
			binding_offset,
			0,
			type,
			texture_infos,
		};

		device.get_vk_device().updateDescriptorSets(write_descriptor_set, nullptr);
	}
	*/

	auto update(
		const logical_device& device,
		vk::DescriptorType type,
		std::span<const vk::raii::Buffer> buffers,
		uint32_t binding_offset = 0
	) -> void {
		const auto buffer_infos = vkw::util::to_vector(std::views::transform(buffers, [](auto&& buf) {
			return vk::DescriptorBufferInfo{*buf, 0, VK_WHOLE_SIZE};
		}));

		const auto write_descriptor_set = vk::WriteDescriptorSet{
			*handle,
			binding_offset,
			0,
			type,
			{},
			buffer_infos
		};

		device.get_vk_device().updateDescriptorSets(write_descriptor_set, nullptr);
	}

	auto update(
		const logical_device& device,
		vk::DescriptorType type,
		std::span<const vk::raii::BufferView> buffer_views,
		uint32_t binding_offset = 0
	) -> void {
		const auto view_handles = vkw::util::to_vector(vkw::util::as_handles(buffer_views));

		const auto write_descriptor_set = vk::WriteDescriptorSet{
			*handle,
			binding_offset,
			0,
			type,
			{},
			{},
			view_handles
		};
	
		device.get_vk_device().updateDescriptorSets(write_descriptor_set, nullptr);
	}

	auto update(
		const logical_device& device,
		const std::vector<std::variant</*write_image_set, */write_buffer_set, write_texel_buffer_set>>& buffer_data,
		uint32_t binding_offset = 0
	) -> void {
		//auto image_infos        = std::vector<std::vector<vk::DescriptorImageInfo>>{};
		auto buffer_infos       = std::vector<std::vector<vk::DescriptorBufferInfo>>{};
		auto texel_buffer_infos = std::vector<std::vector<vk::BufferView>>{};

		auto write_descriptor_sets = std::vector<vk::WriteDescriptorSet>{};
		write_descriptor_sets.reserve(buffer_data.size());

		for (const auto& write_set : buffer_data) {
			auto& set_info = write_descriptor_sets.emplace_back(vk::WriteDescriptorSet{*handle, binding_offset++, 0});

			/*
			if (const auto* image_set = std::get_if<write_image_set>(&write_set)) {
				set_info.setDescriptorType(image_set->descriptor_type);

				auto& info_vec = image_infos.emplace_back();
				for (const auto& image : image_set->data) {
					info_vec.push_back(vk::DescriptorImageInfo{
						*image.sampler,
						*image.image_data->image_view,
						vk::ImageLayout::eShaderReadOnlyOptimal
					});
				}

				set_info.setImageInfo(info_vec);
			}
			else */if (const auto* buffer_set = std::get_if<write_buffer_set>(&write_set)) {
				set_info.setDescriptorType(buffer_set->descriptor_type);

				auto& info_vec = buffer_infos.emplace_back();
				for (const auto& buffer : buffer_set->data) {
					info_vec.push_back(vk::DescriptorBufferInfo{*buffer.get(), 0, VK_WHOLE_SIZE});
				}

				set_info.setBufferInfo(info_vec);
			}
			else if (const auto* texel_buffer_set = std::get_if<write_texel_buffer_set>(&write_set)) {
				set_info.setDescriptorType(texel_buffer_set->descriptor_type);

				auto& view_vec = texel_buffer_infos.emplace_back();
				view_vec = vkw::util::to_vector(vkw::util::as_handles(texel_buffer_set->data));

				set_info.setTexelBufferView(view_vec);
			}
		}

		device.get_vk_device().updateDescriptorSets(write_descriptor_sets, nullptr);
	}

private:

	vk::raii::DescriptorSet handle;
};


class descriptor_pool {
public:
	descriptor_pool(
		const logical_device& device,
		std::span<const vk::DescriptorPoolSize> pool_sizes,
		uint32_t max_sets,
		vk::DescriptorPoolCreateFlags flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet
	) :
		device(device),
		sizes(pool_sizes.begin(), pool_sizes.end()),
		max(max_sets),
		pool(make_descriptor_pool(device, pool_sizes, max_sets, flags)) {
	}
	
	descriptor_pool(
		const logical_device& device,
		std::span<const vk::DescriptorPoolSize> pool_sizes,
		vk::DescriptorPoolCreateFlags flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet
	) :
		descriptor_pool(
			device,
			pool_sizes,
			std::reduce(pool_sizes.begin(), pool_sizes.end(), uint32_t{0}, [](uint32_t sum, const auto& size) { return sum + size.descriptorCount; }),
			flags
		) {
	}

	[[nodiscard]]
	auto get_pool() const noexcept -> const vk::raii::DescriptorPool& {
		return pool;
	}

	[[nodiscard]]
	auto get_sizes() const noexcept -> const std::vector<vk::DescriptorPoolSize>& {
		return sizes;
	}

	[[nodiscard]]
	auto get_max() const noexcept -> uint32_t {
		return max;
	}

	[[nodiscard]]
	auto allocate(const descriptor_set_layout& layout) -> descriptor_set {
		const auto allocate_info = vk::DescriptorSetAllocateInfo{*pool, *layout.get_vk_layout()};
		auto descriptor = std::move(vk::raii::DescriptorSets{device.get().get_vk_device(), allocate_info}.front());
		return descriptor_set{std::move(descriptor)};
	}

	[[nodiscard]]
	auto allocate(std::span<const descriptor_set_layout> layouts) -> std::vector<descriptor_set> {
		const auto vk_layouts = vkw::util::to_vector(vkw::util::as_handles(std::views::transform(layouts, &descriptor_set_layout::get_vk_layout)));

		const auto allocate_info = vk::DescriptorSetAllocateInfo{*pool, vk_layouts};
		auto sets = vk::raii::DescriptorSets{device.get().get_vk_device(), allocate_info};
		
		auto result = std::vector<descriptor_set>{};
		result.reserve(sets.size());
		for (auto&& set : sets) {
			result.emplace_back(std::move(set));
		}

		return result;
	}

private:

	static auto make_descriptor_pool(
		const logical_device& device,
		std::span<const vk::DescriptorPoolSize> pool_sizes,
		uint32_t max_sets,
		vk::DescriptorPoolCreateFlags flags
	) -> vk::raii::DescriptorPool {

		const auto pool_create_info = vk::DescriptorPoolCreateInfo{flags, max_sets, pool_sizes};
		return vk::raii::DescriptorPool(device.get_vk_device(), pool_create_info);
	}


	std::reference_wrapper<const logical_device> device;
	
	std::vector<vk::DescriptorPoolSize> sizes;
	uint32_t max = 0;

	vk::raii::DescriptorPool pool;
};

} //namespace vkw
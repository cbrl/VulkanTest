#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <limits>
#include <memory>
#include <ranges>
#include <string>
#include <thread>
#include <vector>

import vkw;
import utils.handle;
import utils.handle_table;


// TODO:
//   - Add buffer_view class to encapsulate vk::raii::BufferView
//   - Use descriptor indexing (core in Vulkan 1.2)
//     - Single descriptor_set allocated from a single descriptor_pool
//     - Configurable descriptor counts with large defaults
//     - Track free indices and assign them at resource creation
//   - Integrate VMA
//   - Upgrade to Vulkan 1.3 minimum
//     - https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/chap50.html#roadmap-2022


template<typename ResourceT>
class indexed_shader_resource : public ResourceT {
	friend class bindless_descriptor_manager;

public:
	using ResourceT::ResourceT;

	virtual ~indexed_shader_resource() {
		if (descriptor_manager) {
			descriptor_manager->remove(this);
		}
	}

	[[nodiscard]]
	auto get_handle() const noexcept -> handle64 {
		return id;
	}

	[[nodiscard]]
	auto get_descriptor_manager() const noexcept -> const std::shared_ptr<bindless_descriptor_manager>& {
		return descriptor_manager;
	}

private:
	auto set_handle(handle64 h) noexcept {
		id = h;
	}

	auto set_descriptor_manager(std::shared_ptr<bindless_descriptor_manager> manager) {
		descriptor_manager = std::move(manager);
	}

	std::shared_ptr<bindless_descriptor_manager> descriptor_manager;
	handle64 id = handle64::invalid_handle();
};


template<typename T = void>
using /*shader_buffer*/indexed_buffer = indexed_shader_resource<vkw::buffer<T>>;
using /*shader_image_view*/indexed_image_view = indexed_shader_resource<vkw::image_view>;
using /*shader_sampler*/indexed_sampler = indexed_shader_resource<vkw::sampler>;

class bindless_descriptor_manager : public std::enable_shared_from_this<bindless_descriptor_manager> {
private:
	enum descriptor_index : uint32_t {
		storage_buffer,
		sampled_image,
		storage_image,
		sampler,
		count
	};

public:
	struct descriptor_sizes {
		uint32_t storage_buffers = 128 * 1024;
		uint32_t sampled_images = 128 * 1024;
		uint32_t storage_images = 32 * 1024;
		uint32_t samplers = 1024;
	};

	template<typename... ArgsT>
	[[nodiscard]]
	static auto create(ArgsT&&... args) -> std::shared_ptr<bindless_descriptor_manager> {
		return std::make_shared<bindless_descriptor_manager>(std::forward<ArgsT>(args)...);
	}

	bindless_descriptor_manager(const std::shared_ptr<vkw::logical_device>& logical_device, const descriptor_sizes& sizes = {}) {
		const auto pool_sizes = std::array{
			vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, sizes.storage_buffers},
			vk::DescriptorPoolSize{vk::DescriptorType::eSampledImage,  sizes.sampled_images},
			vk::DescriptorPoolSize{vk::DescriptorType::eStorageImage,  sizes.storage_images},
			vk::DescriptorPoolSize{vk::DescriptorType::eSampler,       sizes.samplers}
		};

		const auto bindings = std::array{
			vk::DescriptorSetLayoutBinding{descriptor_index::storage_buffer, vk::DescriptorType::eStorageBuffer, sizes.storage_buffers, vk::ShaderStageFlagBits::eAll, nullptr},
			vk::DescriptorSetLayoutBinding{descriptor_index::sampled_image,  vk::DescriptorType::eSampledImage,  sizes.sampled_images,  vk::ShaderStageFlagBits::eAll, nullptr},
			vk::DescriptorSetLayoutBinding{descriptor_index::storage_image,  vk::DescriptorType::eStorageImage,  sizes.storage_images,  vk::ShaderStageFlagBits::eAll, nullptr},
			vk::DescriptorSetLayoutBinding{descriptor_index::sampler,        vk::DescriptorType::eSampler,       sizes.samplers,        vk::ShaderStageFlagBits::eAll, nullptr}
		};

		descriptor_pool   = vkw::descriptor_pool::create(logical_device, pool_sizes, vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind | vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
		descriptor_layout = vkw::descriptor_set_layout::create(logical_device, bindings, vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool);
		descriptor_set    = descriptor_pool->allocate(descriptor_layout);
	}

	[[nodiscard]]
	auto get_descriptor_set_layout() const noexcept -> const std::shared_ptr<vkw::descriptor_set_layout>& {
		return descriptor_layout;
	}

	[[nodiscard]]
	auto get_descriptor_set() const noexcept -> const std::shared_ptr<vkw::descriptor_set>& {
		return descriptor_set;
	}

	template<typename T, typename... ArgsT>
	[[nodiscard]]
	auto create_storage_buffer(ArgsT&&... args) -> std::shared_ptr<indexed_buffer<T>> {
		auto result = std::make_shared<indexed_buffer<T>>(std::forward<ArgsT>(args)...);
		assert(result->get_usage() & vk::BufferUsageFlagBits::eStorageBuffer);

		result->set_handle(buffer_table.create_handle());
		result->set_descriptor_manager(shared_from_this());
		buffers[result->get_handle()] = result;

		descriptor_set->update(vkw::write_buffer_set{
			.binding = descriptor_index::storage_buffer,
			.array_offset = static_cast<uint32_t>(result->get_handle().index),
			.buffers = {std::cref(*result)}
		});

		return result;
	}

	template<typename T>
	auto remove(indexed_buffer<T>* buffer) -> void {
		buffer_table.release_handle(buffer->get_handle());
	}

private:

	std::shared_ptr<vkw::descriptor_pool> descriptor_pool;
	std::shared_ptr<vkw::descriptor_set_layout> descriptor_layout;
	std::shared_ptr<vkw::descriptor_set> descriptor_set;

	handle_table<handle64> buffer_table;
	handle_table<handle64> sampled_image_table;
	handle_table<handle64> storage_image_table;
	handle_table<handle64> sampler_table;

	std::unordered_map<handle64, std::weak_ptr<vkw::buffer<void>>> buffers;
	std::unordered_map<handle64, std::weak_ptr<vkw::image_view>> sampled_images;
	std::unordered_map<handle64, std::weak_ptr<vkw::image_view>> storage_images;
	std::unordered_map<handle64, std::weak_ptr<vkw::sampler>> samplers;
};


/*
class BufferAllocator {
public:
	BufferAllocator(vk::DeviceSize max_size) {
		ranges[0] = max_size;
		reverse_ranges[max_size] = 0;
	}

	auto allocate(vk::DeviceSize size) -> vk::DeviceSize {
		for (const auto [start, end] : ranges) {
			if ((end - start) <= size) {
				ranges[start] = start + size;
				reverse_ranges[end] = start + size + 1;
				return start;
			}
		}
		assert(false && "No memory available");
	}

	auto deallocate(vk::DeviceSize start, vk::DeviceSize size) -> void {
		const auto range_end = start + size;

		// If there's a free block ending just before this block, then merge them. Otherwise, just insert the entry.
		if (const auto end_it = reverse_ranges.find(start - 1); end_it != reverse_ranges.end()) {
			// Update the end of the prior block to be the end of the current block
			ranges[end_it->second] = range_end;

			// Insert an entry in the reverse map pointing from the end of the current block to the beginning of the prior block
			reverse_ranges[range_end] = ranges[end_it->second];

			// Erase the outdated entry in the reverse map
			reverse_ranges.erase(end_it);
		}
		else {
			ranges[start] = range_end;
			reverse_ranges[range_end] = start;
		}

		// If there's a free block starting just after this block, then merge them.
		if (const auto begin_it = ranges.find(range_end + 1); begin_it != ranges.end()) {
			// The beginning of the current block will not be equal to "start" if there was an unallocated block before the current one
			const auto block_begin = reverse_ranges[range_end];

			// Update the end of the next block's reverse map to point to the beginning of the current block
			reverse_ranges[begin_it->second] = block_begin;

			// Update the end of the current block to be the end of the next block
			ranges[block_begin] = begin_it->second;

			// Erase the outdated entry
			ranges.erase(begin_it);
		}
	}

private:
	std::unordered_map<vk::DeviceSize, vk::DeviceSize> ranges;
	std::unordered_map<vk::DeviceSize, vk::DeviceSize> reverse_ranges;
};

class BufferAllocator {
public:
	BufferAllocator(vk::DeviceSize max_size) {
		start_to_size[0] = max_size;
		size_to_start.emplace(max_size, 0);
	}

	auto allocate(vk::DeviceSize size) -> vk::DeviceSize {
		if (const auto it = size_to_start.find(size); it != size_to_start.end()) {
			const auto start = it->second;
			start_to_size.erase(it->second);
			size_to_start.erase(it);
			return start;
		}

		if (const auto it = size_to_start.upper_bound(size); it != size_to_start.end()) {
			const auto start = it->second;

			start_to_size[start] = size;
			size_to_start.emplace(size, start);

			start_to_size[start + size + 1] = it->first - size;
			size_to_start.emplace(it->first - size, start + size + 1);

			size_to_start.erase(it);

			return start;
		}
	}

	auto deallocate(vk::DeviceSize start, vk::DeviceSize size) -> void {
		assert(false);
	}

private:
	std::map<vk::DeviceSize, vk::DeviceSize> start_to_size;
	std::multimap<vk::DeviceSize, vk::DeviceSize> size_to_start;
};
*/

class mesh {
public:
	std::string name;

	std::shared_ptr<vkw::buffer> vertex_buffer;
	std::shared_ptr<vkw::buffer> index_buffer;

	uint32_t stride;
};

class material {
public:
	std::string name;

	struct {
		std::array<float, 4> base_color;
		float metalness;
		float roughness;
		std::array<float, 3> emissive;
	} params;

	struct {
		std::shared_ptr<vkw::image_view> base_color;
		std::shared_ptr<vkw::image_view> material_params; //G: roughness, B: metalness
		std::shared_ptr<vkw::image_view> normal;
		std::shared_ptr<vkw::image_view> emissive;
	} maps;
};

class model {
	//name
	//mesh
	//material
};

#pragma once

#include <limits>
#include <span>

#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "logical_device.h"
#include "queue.h"


namespace vkw {

template<typename T>
class buffer {
public:
	buffer(
		const logical_device&   device,
		size_t                  count,
		vk::BufferUsageFlags    usage,
		vk::MemoryPropertyFlags property_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	) :
		vk_buffer(create_buffer(device, count, usage)),
		device_memory(create_device_memory(device, vk_buffer, property_flags)),
		count(count),
	    usage(usage),
	    property_flags(property_flags) {

		vk_buffer.bindMemory(*device_memory, 0);
	}

	auto upload(const T& data) const -> void {
		assert(property_flags & vk::MemoryPropertyFlagBits::eHostCoherent);
		assert(property_flags & vk::MemoryPropertyFlagBits::eHostVisible);

		void* mapped = device_memory.mapMemory(0, sizeof(T));
		std::memcpy(mapped, &data, sizeof(T));
		device_memory.unmapMemory();
	}

	template<size_t N>
	auto upload(const T (&data)[N]) const -> void {
		assert(property_flags & vk::MemoryPropertyFlagBits::eHostCoherent);
		assert(property_flags & vk::MemoryPropertyFlagBits::eHostVisible);
		assert(N <= count);

		const size_t data_size = N * sizeof(T);
		
		void* mapped = device_memory.mapMemory(0, data_size);
		std::memcpy(mapped, data, data_size);
        device_memory.unmapMemory();
	}

	auto upload(std::span<const T> data) const -> void {
		assert(property_flags & vk::MemoryPropertyFlagBits::eHostCoherent);
		assert(property_flags & vk::MemoryPropertyFlagBits::eHostVisible);
		assert(data.size() <= count);

		const size_t data_size = data.size() * sizeof(T);
		
		void* mapped = device_memory.mapMemory(0, data_size);
		std::memcpy(mapped, data.data(), data_size);
        device_memory.unmapMemory();
	}

	auto upload(
		const logical_device&        device,
		const vk::raii::CommandPool& command_pool,
		const queue&                 queue,
		std::span<const T>           data
	) const -> void {
		assert(usage & vk::BufferUsageFlagBits::eTransferDst);
		assert(property_flags & vk::MemoryPropertyFlagBits::eDeviceLocal);
		assert(data.size() <= count);

		const size_t data_size = data.size() * sizeof(T);

		auto staging_buffer = buffer<T>{device, data.size(), vk::BufferUsageFlagBits::eTransferSrc};
		staging_buffer.upload(data);

		auto  cmd_buffers    = vk::raii::CommandBuffers{device.get_vk_device(), vk::CommandBufferAllocateInfo{*command_pool, vk::CommandBufferLevel::ePrimary, 1}};
		auto& command_buffer = cmd_buffers.front();

		command_buffer.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
        command_buffer.copyBuffer(**staging_buffer.buffer, **this->buffer, vk::BufferCopy{0, 0, data_size});
        command_buffer.end();

        const auto submit_info = vk::SubmitInfo{nullptr, nullptr, *command_buffer};
        queue.submit(submit_info, nullptr);
        queue.waitIdle();
	}

private:

	[[nodiscard]]
	static auto create_buffer(const logical_device& device, size_t count, vk::BufferUsageFlags usage) -> vk::raii::Buffer {
		assert(count > 0);
		const auto create_info = vk::BufferCreateInfo{vk::BufferCreateFlags{}, sizeof(T) * count, usage};
		return vk::raii::Buffer{device.get_vk_device(), create_info};
	}

	[[nodiscard]]
	static auto create_device_memory(const logical_device& device, const vk::raii::Buffer& buffer, vk::MemoryPropertyFlags properties) -> vk::raii::DeviceMemory {
		const auto memory_properties   = device.get_vk_physical_device().getMemoryProperties();
		const auto memory_requirements = buffer.getMemoryRequirements();

		const auto memory_type_index    = find_memory_type(memory_properties, memory_requirements.memoryTypeBits, properties);
		const auto memory_allocate_info = vk::MemoryAllocateInfo{memory_requirements.size, memory_type_index};

		return vk::raii::DeviceMemory{device.get_vk_device(), memory_allocate_info};
	}

	[[nodiscard]]
	static auto find_memory_type(const vk::PhysicalDeviceMemoryProperties& memory_properties, uint32_t memory_type_bits, vk::MemoryPropertyFlags properties) -> uint32_t {
		auto type_index = std::numeric_limits<uint32_t>::max();

		for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
			if ((memory_type_bits & 1) && ((memory_properties.memoryTypes[i].propertyFlags & properties) == properties)) {
				type_index = i;
				break;
			}

			memory_type_bits >>= 1;
		}

		assert(type_index != std::numeric_limits<uint32_t>::max());
		return type_index;
	}

	vk::raii::Buffer       vk_buffer;
	vk::raii::DeviceMemory device_memory;

	size_t                  count;
	vk::BufferUsageFlags    usage;
	vk::MemoryPropertyFlags property_flags;
};

} //namespace vkw

module;

#include <cassert>
#include <cstddef>
#include <limits>
#include <span>

#include <vulkan/vulkan_raii.hpp>

export module vkw.buffer;

//import vkw.commands;
import vkw.logical_device;
import vkw.queue;
import vkw.util;

export namespace vkw {

template<typename T>
class buffer;

// The void specialization deals in bytes instead of types. It takes its max size in bytes, and the upload functions
// take spans of bytes instead of objects.
template<>
class buffer<void> {
public:
	buffer(
		const logical_device&   device,
		size_t                  size_bytes,
		vk::BufferUsageFlags    usage,
		vk::MemoryPropertyFlags property_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	) :
		vk_buffer(create_buffer(device, size_bytes, usage)),
		device_memory(device.create_device_memory(vk_buffer.getMemoryRequirements(), property_flags)),
		size_bytes(size_bytes),
	    usage(usage),
	    property_flags(property_flags) {

		vk_buffer.bindMemory(*device_memory, 0);
	}

	[[nodiscard]]
	auto get_vk_buffer() const noexcept -> const vk::raii::Buffer& {
		return vk_buffer;
	}

	[[nodiscard]]
	auto get_size_bytes() const noexcept -> size_t {
		return size_bytes;
	}

	auto upload(std::span<const std::byte> data) -> void {
		assert(property_flags & vk::MemoryPropertyFlagBits::eHostCoherent);
		assert(property_flags & vk::MemoryPropertyFlagBits::eHostVisible);
		assert(data.size_bytes() <= size_bytes);
		
		void* mapped = device_memory.mapMemory(0, data.size_bytes());
		std::memcpy(mapped, data.data(), data.size_bytes());
        device_memory.unmapMemory();

		size_bytes = data.size_bytes();
	}

	auto upload(
		const logical_device&        device,
		const vk::raii::CommandPool& command_pool,
		const queue&                 queue,
		std::span<const std::byte>   data
	) -> void {
		auto command_buffers = vk::raii::CommandBuffers{
			device.get_vk_device(),
			vk::CommandBufferAllocateInfo{*command_pool, vk::CommandBufferLevel::ePrimary, 1}
		};

		upload(device, command_buffers.front(), queue, data);
	}

	auto upload(
		const logical_device&          device,
		const vk::raii::CommandBuffer& command_buffer,
		const queue&                   queue,
		std::span<const std::byte>     data
	) -> void {
		assert(usage & vk::BufferUsageFlagBits::eTransferDst);
		assert(property_flags & vk::MemoryPropertyFlagBits::eDeviceLocal);
		assert(data.size_bytes() <= size_bytes);

		auto staging_buffer = buffer<void>{device, data.size_bytes(), vk::BufferUsageFlagBits::eTransferSrc};
		staging_buffer.upload(data);

		command_buffer.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
        command_buffer.copyBuffer(*staging_buffer.vk_buffer, *this->vk_buffer, vk::BufferCopy{0, 0, data.size_bytes()});
        command_buffer.end();

		const auto fence = vk::raii::Fence{device.get_vk_device(), vk::FenceCreateInfo{}};

        const auto submit_info = vk::SubmitInfo{nullptr, nullptr, *command_buffer};
        queue.submit(submit_info, *fence);
		const auto result = device.get_vk_device().waitForFences(*fence, VK_TRUE, std::numeric_limits<uint64_t>::max());

		if (result != vk::Result::eSuccess) {

		}

		size_bytes = data.size_bytes();
	}

/*
	auto stage_upload(command_batch& batch, std::span<const std::byte> data) -> void {
		assert(usage & vk::BufferUsageFlagBits::eTransferDst);
		assert(property_flags & vk::MemoryPropertyFlagBits::eDeviceLocal);
		assert(data.size_bytes() <= size_bytes);

		auto staging_buffer = buffer<void>{device, data.size_bytes(), vk::BufferUsageFlagBits::eTransferSrc};
		staging_buffer.upload(data);

		batch.add_onetime_command([dest_buffer = *this->vk_buffer, staging = std::move(staging_buffer)](const vk::raii::CommandBuffer& command_buffer) {
			command_buffer.copyBuffer(*staging.vk_buffer, dest_buffer, vk::BufferCopy{0, 0, data.size_bytes()});
		});

		size_bytes = data.size_bytes();
	}
*/

private:

	[[nodiscard]]
	static auto create_buffer(const logical_device& device, size_t bytes, vk::BufferUsageFlags usage) -> vk::raii::Buffer {
		assert(bytes > 0);
		const auto create_info = vk::BufferCreateInfo{vk::BufferCreateFlags{}, bytes, usage};
		return vk::raii::Buffer{device.get_vk_device(), create_info};
	}

	vk::raii::Buffer       vk_buffer;
	vk::raii::DeviceMemory device_memory;

	size_t                  size_bytes;
	vk::BufferUsageFlags    usage;
	vk::MemoryPropertyFlags property_flags;
};


// The non-void specializations inherit from the void specialization. They take the max size as a count of T, and their
// upload functions take objects instead of bytes. These functions delegate to the void specialization's upload functions.
template<typename T = void>
class buffer : public buffer<void> {
public:
	buffer(
		const logical_device&   device,
		size_t                  count,
		vk::BufferUsageFlags    usage,
		vk::MemoryPropertyFlags property_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	) :
		buffer<void>(device, count * sizeof(T), usage, property_flags) {
	}

	[[nodiscard]]
	auto get_size() const noexcept -> size_t {
		return size;
	}


	auto upload(const T& data) -> void {
		upload(std::span{&data, 1});
	}

	auto upload(std::span<const T> data) -> void {
		buffer<void>::upload(std::as_bytes(data));
		size = data.size();
	}


	auto upload(
		const logical_device&        device,
		const vk::raii::CommandPool& command_pool,
		const queue&                 queue,
		const T&                     data
	) -> void {
		upload(device, command_pool, queue, std::span{&data, 1});
	}

	auto upload(
		const logical_device&        device,
		const vk::raii::CommandPool& command_pool,
		const queue&                 queue,
		std::span<const T>           data
	) -> void {
		buffer<void>::upload(device, command_pool, queue, std::as_bytes(data));
		size = data.size();
	}


	auto upload(
		const logical_device&          device,
		const vk::raii::CommandBuffer& command_buffer,
		const queue&                   queue,
		const T&                       data
	) -> void {
		upload(device, command_buffer, queue, std::span{&data, 1});
	}

	auto upload(
		const logical_device&          device,
		const vk::raii::CommandBuffer& command_buffer,
		const queue&                   queue,
		std::span<const T>             data
	) -> void {
		buffer<void>::upload(device, command_buffer, queue, std::as_bytes(data));
		size = data.size();
	}

/*
	auto stage_upload(command_batch& batch, const T& data) -> void {
		stage_upload(batch, std::span{&data, 1});
	}

	auto stage_upload(command_batch& batch, std::span<const T> data) -> void {
		buffer<void>::stage_upload(batch, std::as_bytes(data));
		size = data.size();
	}
*/

private:
	size_t size = 0;
};

} //namespace vkw

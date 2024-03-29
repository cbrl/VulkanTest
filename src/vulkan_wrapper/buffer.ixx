module;

#include <cassert>
#include <cstddef>
#include <limits>
#include <memory>
#include <span>

#include <vulkan/vulkan_raii.hpp>

export module vkw.buffer;

//import vkw.commands;
import vkw.logical_device;
import vkw.queue;
import vkw.util;


export namespace vkw {

template<typename T = void>
class buffer;

// The void specialization deals in bytes instead of types. It takes its max size in bytes, and the upload functions
// take spans of bytes instead of objects.
template<>
class buffer<void> {
public:
	[[nodiscard]]
	static auto create(
		std::shared_ptr<logical_device> device,
		size_t                          size_bytes,
		vk::BufferUsageFlags            usage,
		vk::MemoryPropertyFlags         property_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	) -> std::shared_ptr<buffer<void>> {
		return std::make_shared<buffer<void>>(std::move(device), size_bytes, usage, property_flags);
	}

	buffer(
		std::shared_ptr<logical_device> logic_device,
		size_t                          size_bytes,
		vk::BufferUsageFlags            usage,
		vk::MemoryPropertyFlags         property_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	) :
		device(std::move(logic_device)),
		vk_buffer(nullptr),
		device_memory(nullptr),
		size_bytes(size_bytes),
	    usage(usage),
	    property_flags(property_flags) {

		assert(size_bytes > 0);

		vk_buffer = vk::raii::Buffer{device->get_vk_handle(), vk::BufferCreateInfo{vk::BufferCreateFlags{}, size_bytes, usage}};
		device_memory = device->create_device_memory(vk_buffer.getMemoryRequirements(), property_flags);

		vk_buffer.bindMemory(*device_memory, 0);

		if (property_flags & vk::MemoryPropertyFlagBits::eHostVisible) {
			mapped_memory = device_memory.mapMemory(0, get_size_bytes());
		}
	}

	virtual ~buffer() {
		if (mapped_memory) {
			device_memory.unmapMemory();
		}
	}

	[[nodiscard]]
	auto get_vk_handle() const noexcept -> const vk::raii::Buffer& {
		return vk_buffer;
	}

	[[nodiscard]]
	auto get_usage() const noexcept -> vk::BufferUsageFlags {
		return usage;
	}

	[[nodiscard]]
	auto get_size_bytes() const noexcept -> size_t {
		return size_bytes;
	}

	auto upload(std::span<const std::byte> data, vk::DeviceSize offset = 0) -> void {
		assert(property_flags & vk::MemoryPropertyFlagBits::eHostCoherent);
		assert(property_flags & vk::MemoryPropertyFlagBits::eHostVisible);
		assert((data.size_bytes() + offset) <= size_bytes);
		
		std::memcpy(reinterpret_cast<std::byte*>(mapped_memory) + offset, data.data(), data.size_bytes());
	}

	auto upload(
		const vk::raii::CommandPool& command_pool,
		const queue&                 queue,
		std::span<const std::byte>   data,
		vk::DeviceSize               offset = 0
	) -> void {
		auto command_buffers = vk::raii::CommandBuffers{
			device->get_vk_handle(),
			vk::CommandBufferAllocateInfo{*command_pool, vk::CommandBufferLevel::ePrimary, 1}
		};

		upload(command_buffers.front(), queue, data, offset);
	}

	auto upload(
		const vk::raii::CommandBuffer& command_buffer,
		const queue&                   queue,
		std::span<const std::byte>     data,
		vk::DeviceSize                 offset = 0
	) -> void {
		assert(usage & vk::BufferUsageFlagBits::eTransferDst);
		assert(property_flags & vk::MemoryPropertyFlagBits::eDeviceLocal);
		assert((data.size_bytes() + offset) <= size_bytes);

		auto staging_buffer = buffer<void>{device, data.size_bytes(), vk::BufferUsageFlagBits::eTransferSrc};
		staging_buffer.upload(data);

		command_buffer.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
        command_buffer.copyBuffer(*staging_buffer.vk_buffer, *this->vk_buffer, vk::BufferCopy{0, offset, data.size_bytes()});
        command_buffer.end();

		const auto fence = vk::raii::Fence{device->get_vk_handle(), vk::FenceCreateInfo{}};

        const auto submit_info = vk::SubmitInfo{nullptr, nullptr, *command_buffer};
        queue.submit(submit_info, *fence);
		const auto result = device->get_vk_handle().waitForFences(*fence, VK_TRUE, std::numeric_limits<uint64_t>::max());

		if (result != vk::Result::eSuccess) {
			// TODO: report error
		}
	}

/*
	auto stage_upload(command_batch& batch, std::span<const std::byte> data, vk::DeviceSize offset = 0) -> void {
		assert(usage & vk::BufferUsageFlagBits::eTransferDst);
		assert(property_flags & vk::MemoryPropertyFlagBits::eDeviceLocal);
		assert(data.size_bytes() <= size_bytes);

		auto staging_buffer = buffer<void>{device, data.size_bytes(), vk::BufferUsageFlagBits::eTransferSrc};
		staging_buffer.upload(data);

		batch.add_onetime_command([dest_buffer = *this->vk_buffer, staging = std::move(staging_buffer)](const vk::raii::CommandBuffer& command_buffer) {
			command_buffer.copyBuffer(*staging.vk_buffer, dest_buffer, vk::BufferCopy{0, offset, data.size_bytes()});
		});
	}
*/

private:
	std::shared_ptr<logical_device> device;

	vk::raii::Buffer       vk_buffer;
	vk::raii::DeviceMemory device_memory;

	size_t                  size_bytes;
	vk::BufferUsageFlags    usage;
	vk::MemoryPropertyFlags property_flags;

	void* mapped_memory = nullptr;
};


// The non-void specializations inherit from the void specialization. They take the max size as a count of T, and their
// upload functions take objects instead of bytes. These functions delegate to the void specialization's upload functions.
template<typename T>
class buffer : public buffer<void> {
public:
	[[nodiscard]]
	static auto create(
		std::shared_ptr<logical_device> device,
		size_t                          count,
		vk::BufferUsageFlags            usage,
		vk::MemoryPropertyFlags         property_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	) -> std::shared_ptr<buffer<T>> {
		return std::make_shared<buffer<T>>(std::move(device), count, usage, property_flags);
	}

	buffer(
		std::shared_ptr<logical_device> device,
		size_t                          count,
		vk::BufferUsageFlags            usage,
		vk::MemoryPropertyFlags         property_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	) :
		buffer<void>(std::move(device), count * sizeof(T), usage, property_flags) {
	}

	[[nodiscard]]
	auto get_size() const noexcept -> size_t {
		return get_size_bytes() / sizeof(T);
	}


	auto upload(const T& data, size_t element_offset = 0) -> void {
		upload(std::span{&data, 1}, element_offset);
	}

	auto upload(std::span<const T> data, size_t element_offset = 0) -> void {
		buffer<void>::upload(std::as_bytes(data), element_offset * sizeof(T));
	}


	auto upload(
		const vk::raii::CommandPool& command_pool,
		const queue&                 queue,
		const T&                     data,
		size_t                       element_offset = 0
	) -> void {
		upload(command_pool, queue, std::span{&data, 1}, element_offset);
	}

	auto upload(
		const vk::raii::CommandPool& command_pool,
		const queue&                 queue,
		std::span<const T>           data,
		size_t                       element_offset = 0
	) -> void {
		buffer<void>::upload(command_pool, queue, std::as_bytes(data), element_offset * sizeof(T));
	}


	auto upload(
		const vk::raii::CommandBuffer& command_buffer,
		const queue&                   queue,
		const T&                       data,
		size_t                         element_offset = 0
	) -> void {
		upload(command_buffer, queue, std::span{&data, 1}, element_offset);
	}

	auto upload(
		const vk::raii::CommandBuffer& command_buffer,
		const queue&                   queue,
		std::span<const T>             data,
		size_t                         element_offset = 0
	) -> void {
		buffer<void>::upload(command_buffer, queue, std::as_bytes(data), element_offset * sizeof(T));
	}

/*
	auto stage_upload(command_batch& batch, const T& data, size_t element_offset = 0) -> void {
		stage_upload(batch, std::span{&data, 1}, element_offset);
	}

	auto stage_upload(command_batch& batch, std::span<const T> data, size_t element_offset = 0) -> void {
		buffer<void>::stage_upload(batch, std::as_bytes(data), element_offset * sizeof(T));
	}
*/
};

} //namespace vkw

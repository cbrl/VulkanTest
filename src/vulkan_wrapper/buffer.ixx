module;

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
class buffer {
public:
	buffer(
		const logical_device&   device,
		size_t                  count,
		vk::BufferUsageFlags    usage,
		vk::MemoryPropertyFlags property_flags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	) :
		vk_buffer(create_buffer(device, count, usage)),
		device_memory(device.create_device_memory(vk_buffer.getMemoryRequirements(), property_flags)),
		count(count),
	    usage(usage),
	    property_flags(property_flags) {

		vk_buffer.bindMemory(*device_memory, 0);
	}

	[[nodiscard]]
	auto get_vk_buffer() const noexcept -> const vk::raii::Buffer& {
		return vk_buffer;
	}

	auto upload(const T& data) const -> void {
		upload(std::span{&data, 1});
	}

	auto upload(std::span<const T> data) const -> void {
		assert(property_flags & vk::MemoryPropertyFlagBits::eHostCoherent);
		assert(property_flags & vk::MemoryPropertyFlagBits::eHostVisible);
		assert(data.size() <= count);

		const auto data_size = data.size() * sizeof(T);
		
		void* mapped = device_memory.mapMemory(0, data_size);
		std::memcpy(mapped, data.data(), data_size);
        device_memory.unmapMemory();
	}


	auto upload(
		const logical_device&        device,
		const vk::raii::CommandPool& command_pool,
		const queue&                 queue,
		const T&                     data
	) const -> void {
		upload(device, command_pool, queue, std::span{&data, 1});
	}

	auto upload(
		const logical_device&        device,
		const vk::raii::CommandPool& command_pool,
		const queue&                 queue,
		std::span<const T>           data
	) const -> void {
		auto command_buffer = vk::raii::CommandBuffers{
			device.get_vk_device(),
			vk::CommandBufferAllocateInfo{*command_pool, vk::CommandBufferLevel::ePrimary, 1}
		}.front();

		upload(device, command_buffer, queue, data);
	}


	auto upload(
		const logical_device&          device,
		const vk::raii::CommandBuffer& command_buffer,
		const queue&                   queue,
		const T&                       data
	) const -> void {
		upload(device, command_buffer, queue, std::span{&data, 1});
	}

	auto upload(
		const logical_device&          device,
		const vk::raii::CommandBuffer& command_buffer,
		const queue&                   queue,
		std::span<const T>             data
	) const -> void {
		assert(usage & vk::BufferUsageFlagBits::eTransferDst);
		assert(property_flags & vk::MemoryPropertyFlagBits::eDeviceLocal);
		assert(data.size() <= count);

		const auto data_size = data.size() * sizeof(T);

		auto staging_buffer = buffer<T>{device, data.size(), vk::BufferUsageFlagBits::eTransferSrc};
		staging_buffer.upload(data);

		command_buffer.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
        command_buffer.copyBuffer(*staging_buffer.vk_buffer, *this->vk_buffer, vk::BufferCopy{0, 0, data_size});
        command_buffer.end();

		const auto fence = vk::raii::Fence{device.get_vk_device(), vk::FenceCreateInfo{}};

        const auto submit_info = vk::SubmitInfo{nullptr, nullptr, *command_buffer};
        queue.submit(submit_info, *fence);
		device.get_vk_device().waitForFences(*fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
	}

/*
	auto upload(command_batch& batch, const T& data) const -> void {
		upload(batch, std::span{&data, 1});
	}

	auto upload(command_batch& batch, std::span<const T> data) const -> void {
		assert(usage & vk::BufferUsageFlagBits::eTransferDst);
		assert(property_flags & vk::MemoryPropertyFlagBits::eDeviceLocal);
		assert(data.size() <= count);

		auto staging_buffer = buffer<T>{device, data.size(), vk::BufferUsageFlagBits::eTransferSrc};
		staging_buffer.upload(data);

		batch.add_onetime_command([dest_buffer = *this->vk_buffer, staging = std::move(stagin_buffer)](vk::raii::CommandBuffer& command_buffer) {
			command_buffer.copyBuffer(*staging.vk_buffer, dest_buffer, vk::BufferCopy{0, 0, data_size});
		});
	}
*/

private:

	[[nodiscard]]
	static auto create_buffer(const logical_device& device, size_t count, vk::BufferUsageFlags usage) -> vk::raii::Buffer {
		assert(count > 0);
		const auto create_info = vk::BufferCreateInfo{vk::BufferCreateFlags{}, sizeof(T) * count, usage};
		return vk::raii::Buffer{device.get_vk_device(), create_info};
	}

	vk::raii::Buffer       vk_buffer;
	vk::raii::DeviceMemory device_memory;

	size_t                  count;
	vk::BufferUsageFlags    usage;
	vk::MemoryPropertyFlags property_flags;
};

} //namespace vkw

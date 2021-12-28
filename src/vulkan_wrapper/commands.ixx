module;

#include <functional>
#include <iterator>
#include <ranges>
#include <utility>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module vkw.commands;

import vkw.logical_device;


export namespace vkw {

class command {
	friend class command_batch;

public:
	// Size of command_pools span is the frame count
	command(const logical_device& device, std::span<const vk::raii::CommandPool> command_pools) :
		buffers(create_buffers(device, command_pools)) {
	}

	[[nodiscard]]
	auto get_buffers() const noexcept -> const std::vector<vk::raii::CommandBuffer>& {
		return buffers;
	}

	[[nodiscard]]
	auto get_buffer(uint32_t frame) const -> const vk::raii::CommandBuffer& {
		return buffers.at(frame);
	}

private:
	[[nodiscard]]
	static auto create_buffers(
		const logical_device& device,
		std::span<const vk::raii::CommandPool> command_pools
	) -> std::vector<vk::raii::CommandBuffer> {

		auto result = std::vector<vk::raii::CommandBuffer>{};
		result.reserve(command_pools.size());

		for (const auto& pool : command_pools) {
			const auto allocate_info = vk::CommandBufferAllocateInfo{
				*pool,
				vk::CommandBufferLevel::ePrimary,
				1
			};
			
			auto buffers = vk::raii::CommandBuffers{device.get_vk_device(), allocate_info};
			assert(not buffers.empty());

			result.insert(result.end(), std::make_move_iterator(buffers.begin()), std::make_move_iterator(buffers.end()));
		}

		return result;
	}


public:
	std::function<void(vk::raii::CommandBuffer&)> command_func;

private:
	std::vector<vk::raii::CommandBuffer> buffers;
};


class command_batch {
public:
	command_batch(const logical_device& device, uint32_t frame_count, uint32_t queue_family) :
		device(device),
		pools(create_pools(device, frame_count, queue_family)),
		queue_family(queue_family) {
	}

	[[nodiscard]]
	auto get_vk_pools() const noexcept -> const std::vector<vk::raii::CommandPool>& {
		return pools;
	}

	[[nodiscard]]
	auto get_vk_pool(uint32_t frame) const -> const vk::raii::CommandPool& {
		return pools.at(frame);
	}

	[[nodiscard]]
	auto get_queue_family() const noexcept -> uint32_t {
		return queue_family;
	}

	[[nodiscard]]
	auto get_command_buffers(uint32_t frame) const -> std::vector<std::reference_wrapper<const vk::raii::CommandBuffer>> {
		auto result = std::vector<std::reference_wrapper<const vk::raii::CommandBuffer>>{};
		result.reserve(commands.size());

		for (const auto& cmd : commands) {
			result.push_back(cmd.get_buffer(frame));
		}

		return result;
	}

	auto add_command(const std::function<void(vk::raii::CommandBuffer&)>& func) -> void {
		assert(func != nullptr);
		commands.emplace_back(device.get(), pools).command_func = func;
	}

	// TODO: remove command

	auto run_commands(uint32_t frame) -> void {
		pools.at(frame).reset(vk::CommandPoolResetFlags{});

		for (auto& cmd : commands) {
			auto& buffer = cmd.buffers.at(frame);

			const auto begin_info = vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
			buffer.begin(begin_info);

			cmd.command_func(buffer);

			buffer.end();
		}
	}

private:
	[[nodiscard]]
	static auto create_pools(
		const logical_device& device,
		uint32_t frame_count,
		uint32_t queue_family
	) -> std::vector<vk::raii::CommandPool> {

		auto result = std::vector<vk::raii::CommandPool>{};
		result.reserve(frame_count);

		for (auto _ : std::views::iota(uint32_t{0}, frame_count)) {
			const auto create_info = vk::CommandPoolCreateInfo{
				vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
				queue_family
			};
			
			result.emplace_back(device.get_vk_device(), create_info);
		}

		return result;
	}

	std::reference_wrapper<const logical_device> device;
	std::vector<vk::raii::CommandPool> pools;
	std::vector<command> commands;
	uint32_t queue_family;
};

} //namespace vkw

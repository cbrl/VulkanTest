module;

#include <functional>
#include <iterator>
#include <memory>
#include <ranges>
#include <utility>
#include <vector>

#include <vulkan/vulkan_raii.hpp>

export module vkw.commands;

import vkw.logical_device;


namespace vkw {

[[nodiscard]]
static auto create_pools(const logical_device& device, uint32_t frame_count, uint32_t queue_family) -> std::vector<vk::raii::CommandPool> {
	auto result = std::vector<vk::raii::CommandPool>{};
	result.reserve(frame_count);

	for (auto _ : std::views::iota(uint32_t{0}, frame_count)) {
		const auto create_info = vk::CommandPoolCreateInfo{
			vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			queue_family
		};

		result.emplace_back(device.get_vk_handle(), create_info);
	}

	return result;
}

[[nodiscard]]
static auto create_buffers(const logical_device& device, std::span<const vk::raii::CommandPool> command_pools) -> std::vector<vk::raii::CommandBuffer> {
	auto result = std::vector<vk::raii::CommandBuffer>{};
	result.reserve(command_pools.size());

	for (const auto& pool : command_pools) {
		const auto allocate_info = vk::CommandBufferAllocateInfo{*pool, vk::CommandBufferLevel::ePrimary, 1};

		auto buffers = vk::raii::CommandBuffers{device.get_vk_handle(), allocate_info};
		assert(not buffers.empty());

		result.insert(result.end(), std::make_move_iterator(buffers.begin()), std::make_move_iterator(buffers.end()));
	}

	return result;
}


class command {
public:
	// Size of command_pools span is the frame count
	command(const logical_device& device, std::span<const vk::raii::CommandPool> command_pools) :
		buffers(create_buffers(device, command_pools)) {
	}

	[[nodiscard]]
	auto get_vk_handles() const noexcept -> const std::vector<vk::raii::CommandBuffer>& {
		return buffers;
	}

	[[nodiscard]]
	auto get_vk_handle(uint32_t frame) const -> const vk::raii::CommandBuffer& {
		return buffers.at(frame);
	}

	[[nodiscard]]
	auto get_function() const noexcept -> const std::function<void(const vk::raii::CommandBuffer&)>& {
		return command_func;
	}

	auto set_function(const std::function<void(const vk::raii::CommandBuffer&)>& function) {
		command_func = function;
	}

private:
	std::function<void(const vk::raii::CommandBuffer&)> command_func;
	std::vector<vk::raii::CommandBuffer> buffers;
};


export class command_batch {
public:
	[[nodiscard]]
	static auto create(std::shared_ptr<logical_device> device, uint32_t frame_count, uint32_t queue_family) -> std::shared_ptr<command_batch> {
		return std::make_shared<command_batch>(std::move(device), frame_count, queue_family);
	}

	command_batch(std::shared_ptr<logical_device> logic_device, uint32_t frame_count, uint32_t queue_family) :
		device(std::move(logic_device)),
		pools(create_pools(*device, frame_count, queue_family)),
		queue_family(queue_family) {
	}

	[[nodiscard]]
	auto get_vk_pools() const noexcept -> const std::vector<vk::raii::CommandPool>& {
		return pools;
	}

	[[nodiscard]]
	auto get_vk_handle(uint32_t frame) const -> const vk::raii::CommandPool& {
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
			result.push_back(cmd.get_vk_handle(frame));
		}

		return result;
	}

	auto add_command(const std::function<void(const vk::raii::CommandBuffer&)>& func) -> void {
		assert(func != nullptr);
		commands.emplace_back(*device, pools).set_function(func);
	}

	// TODO
	//auto remove_command(command_handle handle) -> void;

	auto run_commands(uint32_t frame) -> void {
		pools.at(frame).reset(vk::CommandPoolResetFlags{});

		for (auto& cmd : commands) {
			auto& buffer = cmd.get_vk_handle(frame);

			const auto begin_info = vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
			buffer.begin(begin_info);

			cmd.get_function()(buffer);

			buffer.end();
		}
	}

private:

	std::shared_ptr<logical_device> device;
	std::vector<vk::raii::CommandPool> pools;
	std::vector<command> commands;
	uint32_t queue_family;
};

} //namespace vkw

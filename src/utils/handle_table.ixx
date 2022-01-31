module;

#include <cassert>
#include <vector>

export module utils.handle_table;

export import utils.handle;

export template<handle_like HandleT>
class handle_table {
	using container_type = std::vector<HandleT>;

public:
	using handle_type = HandleT;
	using size_type = size_t;

	handle_table() = default;
	handle_table(const handle_table&) = default;
	handle_table(handle_table&&) noexcept = default;

	~handle_table() = default;

	auto operator=(const handle_table&) -> handle_table& = default;
	auto operator=(handle_table&&) noexcept -> handle_table& = default;

	[[nodiscard]]
	auto create_handle() -> handle_type {
		if (available > 0) {
			// Remove the first free handle from the list
			const auto result = handle_type{next, table[next].counter};
			next = table[next].index;
			--available;
			return result;
		}
		else {
			// Verify that the table size doesn't exceed the max handle index
			if (table.size() >= handle_type::index_max) {
				assert(false && "handle_table::create_handle() - max size reached");
				return handle_type::invalid_handle();
			}

			// Insert and return a new handle
			const auto result = table.emplace_back(table.size(), 0);
			return result;
		}
	}

	auto release_handle(handle_type h) -> void {
		if (not valid(h)) {
			assert(false && "handle_table::release_handle() - invalid handle specified for release");
			return;
		}

		// Increment the handle version
		++table[h.index].counter;

		// Extend list of free handles
		if (available > 0) {
			table[h.index].index = next;
		}

		next = h.index;
		++available;
	}

	auto clear() noexcept -> void {
		table.clear();
		available = 0;
	}


	[[nodiscard]]
	auto valid(handle_type handle) const noexcept -> bool {
		if ((handle != handle_type::invalid_handle()) and (handle.index < table.size())) {
			return table[handle.index].counter == handle.counter;
		}
		return false;
	}

	[[nodiscard]]
	auto empty() const noexcept -> bool {
		return table.empty();
	}

	[[nodiscard]]
	auto size() const noexcept -> size_type {
		return table.size();
	}

	auto reserve(size_type new_cap) -> void {
		if (table.capacity() < handle_type::index_max) {
			table.reserve(std::min(new_cap, handle_type::index_max));
		}
		else {
			assert(false && "HandleTable::reserve() - max size reached");
			return;
		}
	}

private:
	container_type table;

	typename handle_type::value_type next = 0; //index of next free handle
	typename handle_type::value_type available = 0; //number of free handles
};

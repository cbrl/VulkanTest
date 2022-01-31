module;

#include <cstdint>
#include <functional>
#include <limits>

export module utils.handle;

export {

template<std::unsigned_integral T, size_t IndexBits, size_t CounterBits>
struct handle {
public:
	using value_type = T;

	//----------------------------------------------------------------------------------
	// Assertions
	//----------------------------------------------------------------------------------
	static_assert(
		(CounterBits > 0) and (CounterBits < (sizeof(T) * 8)),
		"Invalid number of counter bits specified for handle"
	);

	static_assert(
		(IndexBits > 0) and (IndexBits < (sizeof(T) * 8)),
		"Invalid number of index bits specified for handle"
	);

	static_assert(
		(CounterBits + IndexBits) == (sizeof(T) * 8),
		"Size of handle's value_type is not equal to IndexBits + CounterBits"
	);


	//----------------------------------------------------------------------------------
	// Constructors
	//----------------------------------------------------------------------------------
	constexpr handle() noexcept = default;

	constexpr explicit handle(T value) noexcept :
		index((value& index_bitmask) >> counter_bits),
		counter(value& counter_bitmask) {
	}

	constexpr explicit handle(T index, T counter) noexcept :
		index(index),
		counter(counter) {
	}

	constexpr handle(const handle&) noexcept = default;
	constexpr handle(handle&&) noexcept = default;


	//----------------------------------------------------------------------------------
	// Destructor
	//----------------------------------------------------------------------------------
	~handle() = default;


	//----------------------------------------------------------------------------------
	// Operators
	//----------------------------------------------------------------------------------
	constexpr auto operator=(const handle&) noexcept -> handle& = default;
	constexpr auto operator=(handle&&) noexcept -> handle& = default;


	//----------------------------------------------------------------------------------
	// Member Functions
	//----------------------------------------------------------------------------------
	[[nodiscard]]
	constexpr operator T() const noexcept {
		return (index << counter_bits) | counter;
	}

	[[nodiscard]]
	auto hash() const noexcept -> size_t {
		return std::hash<T>{}(this->operator T());
	}

	[[nodiscard]]
	static constexpr auto invalid_handle() noexcept -> handle {
		return handle{std::numeric_limits<T>::max()};
	}

	//----------------------------------------------------------------------------------
	// Member Variables
	//----------------------------------------------------------------------------------
	T index : IndexBits = std::numeric_limits<T>::max();
	T counter : CounterBits = std::numeric_limits<T>::max();


	//----------------------------------------------------------------------------------
	// Static Variables
	//----------------------------------------------------------------------------------

	// Max values
	static constexpr T index_max = (T{1} << IndexBits) - T{2};
	static constexpr T counter_max = (T{1} << CounterBits) - T{2};

	// Number of bits
	static constexpr size_t index_bits = IndexBits;
	static constexpr size_t counter_bits = CounterBits;

private:

	// Bitmasks
	static constexpr T index_bitmask = ((T{1} << IndexBits) - T{1}) << CounterBits;
	static constexpr T counter_bitmask = (T{1} << CounterBits) - T{1};
};

using handle32 = handle<uint32_t, 20, 12>;
using handle64 = handle<uint64_t, 40, 24>;

namespace std {
template<typename T, size_t IB, size_t CB>
struct hash<handle<T, IB, CB>> {
	auto operator()(const handle<T, IB, CB>& handle) const noexcept -> size_t {
		return handle.hash();
	}
};
} //namespace std

template<typename T>
concept handle_like = requires(T v) {
	typename T::value_type;
	v.index;
	v.counter;
	std::same_as<decltype(T::index), typename T::value_type>;
	std::same_as<decltype(T::counter), typename T::value_type>;
	requires sizeof(T) == sizeof(typename T::value_type);
	std::convertible_to<T, typename T::value_type>;
	typename std::hash<T>;
};

} //export

#pragma once

#include <bit>
#include <type_traits>
#include <cstddef>
#include <cstring>
#include <algorithm>
#include <array>
#include <system_error>

// Intrinsic functions
#if defined(_MSC_VER)
# include <intrin.h>
#endif

// -------------------------------------------------------------------------
namespace evi { 

// Forward declaration
template<typename... Ts>
class Union;

namespace detail {

// -------------------------------------------------------------------------
// Union Implementation for variadic elements
template <typename, typename...>
union UnionImpl;

template <typename T>
union UnionImpl<T> {
    T value;
};

template <typename T, typename... Ts>
union UnionImpl {
    T value;
    UnionImpl<Ts...> rest;
};

// -------------------------------------------------------------------------
// Recursive specialization to get values by index.
template<size_t i>
struct getter_index
{
	template<typename... Ts>
	[[nodiscard]]
	static inline auto& get(UnionImpl<Ts...>& u) {
		return getter_index<i - 1>::get(u.rest);
	}
};

template<>
struct getter_index<0>
{
	template<typename T, typename... Ts>
	[[nodiscard]]
	static inline auto& get(UnionImpl<T, Ts...>& u) {
		return u.value;
	}
};

template<size_t i, typename... Ts>
[[nodiscard]] 
static inline auto& get_by_index(UnionImpl<Ts...>& u) 
{
	static_assert(i < sizeof...(Ts), "index is too big!");
	return getter_index<i>::get(u);
}

// -------------------------------------------------------------------------
// Recursive specialization to get values by typename.
template<size_t i, typename Ret>
struct getter_type
{
	template<typename... Ts>
	[[nodiscard]]
	static inline Ret& get(UnionImpl<Ts...>& u) 
	{
		if constexpr(std::is_same_v<decltype(u.value), Ret>)
			return u.value;

		return getter_type<i - 1, Ret>::get(u.rest);
	}
};

template<typename Ret>
struct getter_type<0, Ret>
{
	template<typename T, typename... Ts>
	[[nodiscard]]
	static inline Ret& get(UnionImpl<T, Ts...>& u) 
	{
		if constexpr(std::is_same_v<decltype(u.value), Ret>)
			return u.value;

		// Unreachable code
		throw 0;	
	}
};

template<typename T, typename... Ts>
[[nodiscard]]
static inline T& get_by_type(UnionImpl<Ts...>& u) 
{
	static_assert(std::disjunction_v<std::is_same<T, Ts>...>, "T does not exists in the union.");
	return getter_type<sizeof...(Ts) - 1, T>::get(u);
}

// -------------------------------------------------------------------------
// Checks if type is an Union
template<typename T>
struct is_union
	: std::false_type {};

template<typename... Ts>
struct is_union<Union<Ts...>>
	: std::true_type {};

template<typename T>
static constexpr bool is_union_v = is_union<T>::value;

template<typename T>
concept only_union = is_union_v<T>;

// -------------------------------------------------------------------------
// Check if a type is std::array<T, N> or array[N]
// array[] will cause a compile-time error.
template<typename T>
struct is_bounded_array
	: std::is_bounded_array<T> {};

template<typename T, size_t Len>
struct is_bounded_array<std::array<T, Len>>
	: std::true_type {};

template<typename T>
static constexpr bool is_bounded_array_v = is_bounded_array<T>::value;

// -------------------------------------------------------------------------
// Checks if type a is NOT cv ( const / volatile ) and it's not a pointer / reference.
template<typename T>
struct is_plain_type 
{
	static constexpr bool value = 
		std::is_same_v<
			std::remove_const_t<std::remove_volatile_t<
					std::remove_pointer_t<std::remove_reference_t<T>>>>, 
		T>;
};

template<typename T>
static constexpr bool is_plain_type_v = is_plain_type<T>::value;

// -------------------------------------------------------------------------
// Checks if a struct is POD aka standard layout.
template<typename T>
struct is_struct_standard_layout {
	static constexpr bool value = std::is_standard_layout_v<T> && std::is_class_v<T>;
};

template<typename T>
static constexpr bool is_struct_standard_layout_v = is_struct_standard_layout<T>::value;

// -------------------------------------------------------------------------
// Checks if a type is an union possiblity
template<typename T>
struct is_union_possible_type 
{
	static constexpr bool value = 
		is_plain_type_v<T> 				 && 
		 (is_struct_standard_layout_v<T> || 
		  is_bounded_array_v<T> 		 || 
		  std::is_arithmetic_v<T>) 		 &&
		!std::is_union_v<T> 			 &&
		!std::is_enum_v<T>;
};

template<typename T>
static constexpr bool is_union_possible_type_v = is_union_possible_type<T>::value;

// -------------------------------------------------------------------------
// Swap endiannes:
// - 8 bytes - remain the same.
// - 16 bytes
// - 32 bytes
// - 64 bytes
// - Others may cause compile-time errors.
class swap_endian
{
private:
	template<typename T>
	[[nodiscard]]
	static inline T byte_order_swap(T value)
		requires ( sizeof(T) == sizeof(uint8_t) )
	{
		// Reversing the bits.
		value = (value & 0xF0) >> 4 | (value & 0x0F) << 4;
		value = (value & 0xCC) >> 2 | (value & 0x33) << 2;
		value = (value & 0xAA) >> 1 | (value & 0x55) << 1;
	
		return value;
	}
	
	template<typename T>
	[[nodiscard]]
	static inline T byte_order_swap(T value)
		requires ( sizeof(T) == sizeof(uint16_t) && std::is_integral_v<T> )
	{
#if defined(_MSC_VER)
		return _byteswap_ushort(value);
#elif defined(__GNUC__) || defined(__clang__)
		return __builtin_bswap16(value);
#else
		return (value >> 8) | (value << 8);
#endif
	}

	template<typename T>
	[[nodiscard]]
	static inline T byte_order_swap(T value)
		requires ( sizeof(T) == sizeof(uint32_t) && std::is_integral_v<T> )
	{
#if defined(_MSC_VER)
		return _byteswap_ulong(value);
#elif defined(__GNUC__) || defined(__clang__)
		return __builtin_bswap32(value);
#else
		return ( value >> 24) 			   |
			   ((value << 8) & 0x00FF0000) | 
			   ((value >> 8) & 0x0000FF00) |
			   ( value << 24);
#endif
	}

	template<typename T>
	[[nodiscard]]
	static inline T byte_order_swap(T value)
		requires ( sizeof(T) == sizeof(uint64_t) && std::is_integral_v<T> )
	{
#if defined(_MSC_VER)
		return _byteswap_uint64(value);
#elif defined(__GNUC__) || defined(__clang__)
		return __builtin_bswap64(value);
#else
		return ( value >> 56) 						|
			   ((value << 40) & 0x00FF000000000000) |
			   ((value << 24) & 0x0000FF0000000000) |
			   ((value <<  8) & 0x000000FF00000000) |
			   ((value >>  8) & 0x00000000FF000000) |
			   ((value >> 24) & 0x0000000000FF0000) |
			   ((value >> 40) & 0x000000000000FF00) |
			   ( value << 56);
#endif
	}

	template<typename T>
	[[nodiscard]]
	static inline T byte_order_swap(T value)
		requires ( sizeof(float) == sizeof(uint32_t) && std::is_floating_point_v<T> )
	{
		uint32_t temp;
		std::memcpy(&temp, reinterpret_cast<const void*>(&value), sizeof(uint32_t));
		temp = byte_order_swap(temp);
		std::memcpy(&value, reinterpret_cast<void*>(&temp), sizeof(float));
		
		return value;
	}

	template<typename T>
	[[nodiscard]]
	static inline T byte_order_swap(T value)
		requires ( sizeof(double) == sizeof(uint64_t) && std::is_floating_point_v<T> )
	{
		uint64_t temp;
		std::memcpy(&temp, reinterpret_cast<const void*>(&value), sizeof(uint64_t));
		temp = byte_order_swap(temp);
		std::memcpy(&value, reinterpret_cast<void*>(&temp), sizeof(double));
		
		return value;
	}

	template<typename T>
	[[nodiscard]] [[noreturn]]
	static inline T byte_order_swap(T value) {
		static_assert(std::is_same_v<T, void>, "System has unknown size.");
	}

public:
	template<typename T>
	[[nodiscard]]
	static inline T swap(const T& value)
		requires( std::is_arithmetic_v<T> )
	{
		return byte_order_swap(value);
	}

	template<typename T>
	[[nodiscard]]
	static inline T swap(const T& src)
		// requires data structure or array
	{
		T dest;
		std::memcpy(&dest, &src, sizeof(T));

		std::byte* dest_casted = reinterpret_cast<std::byte*>(&dest);
		std::reverse(dest_casted, dest_casted + sizeof(T));

		return *reinterpret_cast<T*>(dest_casted);
	}
};

} // namespace detail

// -------------------------------------------------------------------------
// Union all of the types.
template<typename... Ts>
class Union
{
	static_assert(sizeof...(Ts) > 0, "Insufficient amount of types.");
	static_assert(std::conjunction_v<detail::is_union_possible_type<Ts>...>, "Type is incorrect!");

protected:
	detail::UnionImpl<Ts...> m_union;
	
#if 0
public:
	Union() { std::cout << sizeof(m_union) << std::endl; }
#endif
};

// -------------------------------------------------------------------------
// Which endianness the struct is.
enum class ByteOrder 
{ 
	Little = static_cast<int>(std::endian::little), 
	Big    = static_cast<int>(std::endian::big)
};

// -------------------------------------------------------------------------
// Safe Endian Union
template<ByteOrder Endianness, detail::only_union UnionT>
class SafeEndianUnion
	: protected UnionT
{
private:
	template<typename T>
	inline T check_and_fix_endianness(const T& value)
	{
		T ret = value;

		static constexpr std::endian endian = static_cast<std::endian>(Endianness);
		if constexpr(endian != std::endian::native)
		{
			if(m_type_code != typeid(T).hash_code())
				ret = detail::swap_endian::swap(value);
		}

		return ret;
	}

public:
	constexpr SafeEndianUnion() noexcept = default;
	
	template<typename T>
	constexpr SafeEndianUnion(const T& value) { set(value); }

	constexpr SafeEndianUnion(const SafeEndianUnion& other) {
		this->m_union = other.m_union;
	}

	constexpr auto operator=(const SafeEndianUnion& other)
	{
		this->m_union = other.m_union;
		return *this;
	}

	template<size_t i>
	constexpr auto get()
	{
		auto value = detail::get_by_index<i>(this->m_union);
		return check_and_fix_endianness(value);
	}

	template<typename T>
	constexpr auto get()
	{
		T& value = detail::get_by_type<T>(this->m_union);
		return check_and_fix_endianness(value);
	}

	template<size_t i, typename T>
	constexpr void set(const T& value)
	{
		auto& val = detail::get_by_index<i>(this->m_union);
		val = value;
		m_type_code = typeid(T).hash_code();
	}

	template<typename T>
	constexpr void set(const T& value)
	{
		auto& val = detail::get_by_type<T>(this->m_union);
		val = value;
		m_type_code = typeid(T).hash_code();
	}

	template<typename T>
	constexpr auto operator=(const T& value) 
	{
		set(value);	
		return *this;
	}

private:
	using type_code = size_t;
	type_code m_type_code = 0;
};

} // namespace evi

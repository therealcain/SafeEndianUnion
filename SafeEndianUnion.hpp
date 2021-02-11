#pragma once

#include <bit>
#include <type_traits>
#include <cstddef>
#include <cstring>
#include <algorithm>

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
			std::remove_const_t<
				std::remove_volatile_t<
					std::remove_pointer_t<
						std::remove_reference_t<T>>>>, 
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
		is_plain_type_v<T> && 
		(is_struct_standard_layout_v<T> || is_bounded_array_v<T> || std::is_arithmetic_v<T>) &&
		!std::is_union_v<T> &&
		!std::is_enum_v<T>;
};

template<typename T>
static constexpr bool is_union_possible_type_v = is_union_possible_type<T>::value;
// -------------------------------------------------------------------------

class SwapEndian
{
private:
	template<typename T>
	static inline T reverse_bits(T src)
	{
		T dest = src;

		while(src > 0)
		{
			dest <<= 1;

			if(src & 1 == 1)
				dest ^= 1;

			src >>= 1;
		}

		return dest;
	}

public:
	template<typename T>
	static inline T swap(T value)
		requires( std::is_arithmetic_v<T> )
	{
		return reverse_bits(value);
	}

	template<typename T>
	static inline T swap(const T& src)
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
				ret = detail::SwapEndian::swap(value);
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

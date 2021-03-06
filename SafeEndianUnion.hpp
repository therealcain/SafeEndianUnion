/*
 * MIT License
 *
 * Copyright (c) 2021 Eviatar
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// for std::is_same, std::is_arithmetic, std::conjunction, 
// std::is_standard_layout, std::is_class, std::is_enum, 
// std::is_bounded_array, std::invoke_result, std::is_integral, 
// std::is_floating_point, std::is_trivially_copyable,
// std::disjunction, std::remove_cvref 
#include <type_traits> 
// for std::byte, std::size_t, std::ssize_t, std::nullptr_t
#include <cstddef>
// for std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t
#include <cstdint>
// for std::memcpy
#include <cstring>
// for std::max, std::reverse
#include <algorithm>
#include <array>
// for std::tuple, std::tuple_element
#include <tuple>
// for std::endian, std::bit_cast
# include <bit>

// -------------------------------------------------------------------------
// Intrinsic functions for MSVC
#if defined(_MSC_VER)
// for _byteswap_uint64, _byteswap_ulong, _byteswap_ushort
# include <intrin.h>
#endif

// -------------------------------------------------------------------------
// Some compilers are still not supporting these keywords.
#ifdef __cpp_consteval
# define __EVI_CONSTEVAL consteval
#else
# define __EVI_CONSTEVAL constexpr
#endif

#ifdef __cpp_constinit
# define __EVI_CONSTINIT constinit
#else
# define __EVI_CONSTINIT constexpr
#endif

namespace evi {
// -------------------------------------------------------------------------
// Forward declaration
template<typename... Ts>
class Union;

namespace detail {
// -------------------------------------------------------------------------
// Implementation of std::bit_cast since some compilers are still
// not supporting this function.
template<typename To, typename From>
constexpr To bitcast(const From& from) noexcept 
{
#ifdef __cpp_lib_bit_cast
	// GCC uses __builtin_bit_cast
	return std::bit_cast<To>(from);
#else
	static_assert(sizeof(To) == sizeof(From));
	static_assert(std::is_trivially_copyable_v<To>);
	static_assert(std::is_trivially_copyable_v<From>);

	To dest;
	std::memcpy(&dest, &from, sizeof(To));
	return dest;
#endif
}

// -------------------------------------------------------------------------
// Implementation of the union behaviour.
template<typename... Ts>
class UnionImpl 
{
public:
    template<typename T>
    constexpr void set_data(const T& value)
    {
        static_assert(std::disjunction_v<std::is_same<T, Ts>...>, "T does not exists in the union.");
        data = bitcast<data_t>(value);
    }

    template<size_t i>
    constexpr auto get_by_index()
    {
        static_assert(i < sizeof...(Ts), "index is too big!");
		using element_t = std::tuple_element_t<i, std::tuple<Ts...>>;
		return bitcast<element_t>(data);
    }

    template<typename T>
    constexpr auto get_by_type()
    {
        static_assert(std::disjunction_v<std::is_same<T, Ts>...>, "T does not exists in the union.");
		return bitcast<T>(data);
    }

private:
	static constexpr size_t data_size = std::max({sizeof(Ts)...});

   	// std::aligned_union or std::aligned_storage might be better here
   	// but they require allocation on the heap with the 'new' operator.
   	using data_t = std::array<std::byte, data_size>;
	data_t data;
};

// -------------------------------------------------------------------------
// Checks if type is an Union
template<typename T>
struct is_union
	: std::false_type {};

template<typename... Ts>
struct is_union<Union<Ts...>>
	: std::true_type {};

template<typename T>
constexpr bool is_union_v = is_union<T>::value;

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
constexpr bool is_bounded_array_v = is_bounded_array<T>::value;

// -------------------------------------------------------------------------
// Checks if a type is a plain type, meaning a type is not const, volatile, 
// reference or pointer.
template<typename T>
struct is_plain_type {
	static constexpr bool value = std::is_same_v<T, std::remove_cvref_t<T>>;
};

template<typename T>
constexpr bool is_plain_type_v = is_plain_type<T>::value;

// -------------------------------------------------------------------------
// Checks if a struct is POD aka standard layout.
template<typename T>
struct is_struct_standard_layout {
	static constexpr bool value = std::is_standard_layout_v<T> && std::is_class_v<T>;
};

template<typename T>
constexpr bool is_struct_standard_layout_v = is_struct_standard_layout<T>::value;

// -------------------------------------------------------------------------
// Checks if a type is an union possiblity
template<typename T>
struct is_union_possible_type 
{
	static constexpr bool value = is_plain_type_v<T> 
		&& (is_struct_standard_layout_v<T> || is_bounded_array_v<T> || std::is_arithmetic_v<T>) 
		&& !std::is_union_v<T> 
		&& !std::is_enum_v<T>;
};

template<typename T>
constexpr bool is_union_possible_type_v = is_union_possible_type<T>::value;

// -------------------------------------------------------------------------
// Checks if a type is a possiblity member in a struct.
template<typename T>
struct is_possible_type_in_struct
{
	static constexpr bool value = is_plain_type_v<T> && 
		(is_bounded_array_v<T> || std::is_arithmetic_v<T>);
};

template<typename T>
constexpr bool is_possible_type_in_struct_v = is_possible_type_in_struct<T>::value;

// -------------------------------------------------------------------------
// A class to swap endianness and reverse bits.
class BitsManipulation
{
private:
	// Swap endiannes:
	// - 8 bits.
	// - 16 bits
	// - 32 bits
	// - 64 bits
	// - Others may cause compile-time errors.
	template<typename T>
	static constexpr T byte_order_swap(T value) noexcept // byte
		requires ( sizeof(T) == sizeof(uint8_t) ) 
	{
		return value;
	}
	
	template<typename T>
	static constexpr T byte_order_swap(T value) noexcept // 2 bytes
		requires ( sizeof(T) == sizeof(uint16_t) ) 
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
	static constexpr T byte_order_swap(T value) noexcept // 4 bytes
		requires ( sizeof(T) == sizeof(uint32_t) && std::is_integral_v<T> ) 
	{
#if defined(_MSC_VER)
		return _byteswap_ulong(value);
#elif defined(__GNUC__) || defined(__clang__)
		return __builtin_bswap32(value);
#else
		return ( value >> 24) 		       |
			   ((value << 8) & 0x00FF0000) | 
			   ((value >> 8) & 0x0000FF00) |
			   ( value << 24);
#endif
	}

	template<typename T>
	static constexpr T byte_order_swap(T value) noexcept // 8 bytes
		requires ( sizeof(T) == sizeof(uint64_t) && std::is_integral_v<T> ) 
	{
#if defined(_MSC_VER)
		return _byteswap_uint64(value);
#elif defined(__GNUC__) || defined(__clang__)
		return __builtin_bswap64(value);
#else
		return ( value >> 56)                       |
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
	static constexpr T byte_order_swap(T value) // 4 bytes
		requires ( sizeof(float) == sizeof(uint32_t) && std::is_floating_point_v<T> ) 
	{
		// de-referencing float pointer as uint32_t breaks strict-aliasing rules for C++, even if it normally works.
		// uint32_t temp = byte_order_swap(*(reinterpret_cast<const uint32_t*>(&value)));
		// return *(reinterpret_cast<float*>(&temp));
		
		uint32_t temp;
		std::memcpy(&temp, reinterpret_cast<const void*>(&value), sizeof(uint32_t));
		temp = byte_order_swap(temp);
		std::memcpy(&value, reinterpret_cast<void*>(&temp), sizeof(float));
		
		return value;
	}

	template<typename T>
	static constexpr T byte_order_swap(T value) // 8 bytes
		requires ( sizeof(double) == sizeof(uint64_t) && std::is_floating_point_v<T> )
	{
		uint64_t temp;
		std::memcpy(&temp, reinterpret_cast<const void*>(&value), sizeof(uint64_t));
		temp = byte_order_swap(temp);
		std::memcpy(&value, reinterpret_cast<void*>(&temp), sizeof(double));
		
		return value;
	}

	template<typename T>
	[[noreturn]]
	static constexpr T byte_order_swap(T) noexcept {
		static_assert(std::is_same_v<T, void>, "System has unknown size.");
	}

public:
	template<typename T>
	static constexpr T swap_endian(const T& value)
		requires( std::is_arithmetic_v<T> )
	{
		return byte_order_swap(value);
	}

	template<typename T>
	static constexpr T swap_endian(const T& src)
		// requires data structure or array
	{
		T dest;
		std::memcpy(&dest, &src, sizeof(T));

		std::byte* dest_casted = reinterpret_cast<std::byte*>(&dest);
		std::reverse(dest_casted, dest_casted + sizeof(T));

		return *reinterpret_cast<T*>(dest_casted);
	}

	static constexpr uint8_t reverse_byte(uint8_t value)
	{
		value = (value & 0xF0) >> 4 | (value & 0x0F) << 4;
		value = (value & 0xCC) >> 2 | (value & 0x33) << 2;
		value = (value & 0xAA) >> 1 | (value & 0x55) << 1;

		return value;
	}
};

// -------------------------------------------------------------------------
// ↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓
// -------------------------------------------------------------------------
// Simple reflection system to validate the types inside a struct.

// Class is convertible to anything.
struct UniversalType
{
	template<typename T>
	operator T() const;
};

// -------------------------------------------------------------------------
// Counting the amount of members in a POD.
// NOTE: This only works for aggregate types.
template<typename T, typename... Ts>
__EVI_CONSTEVAL auto count_member_fields(Ts... members)
{
	if constexpr( requires { T{members...}; } == false )
		return sizeof...(members) - 1;
	else
		return count_member_fields<T>(members..., UniversalType{});
} 

// -------------------------------------------------------------------------
// Template specialization up to 32 fields in a struct.
template<size_t size, typename T>
struct StructToTuple;

#define __EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(STRUCT, FIELDS_NUM, ...) \
	template<typename T>                                                   \
	struct STRUCT<FIELDS_NUM, T> {                                         \
		static constexpr auto unevaluated(T& u) noexcept          		   \
		{                                                          		   \
			auto&& [__VA_ARGS__] = u;                          			   \
			return std::tuple{__VA_ARGS__};                    			   \
		}                                                          		   \
	}

// Yeah, Nasty...
// Only if we had variadic structured bindings... :)
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 1 , m1);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 2 , m1, m2);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 3 , m1, m2, m3);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 4 , m1, m2, m3, m4);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 5 , m1, m2, m3, m4, m5);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 6 , m1, m2, m3, m4, m5, m6);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 7 , m1, m2, m3, m4, m5, m6, m7);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 8 , m1, m2, m3, m4, m5, m6, m7, m8);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 9 , m1, m2, m3, m4, m5, m6, m7, m8, m9);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 10, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 11, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 12, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 13, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 14, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 15, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 16, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 17, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 18, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 19, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 20, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 21, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 22, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 23, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 24, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 25, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 26, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 27, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 28, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 29, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 30, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 31, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31);
__EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(StructToTuple, 32, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16, m17, m18, m19, m20, m21, m22, m23, m24, m25, m26, m27, m28, m29, m30, m31, m32);

// -------------------------------------------------------------------------
// Converting a struct into a tuple.
template<typename T>
using struct_to_tuple_t = std::invoke_result_t<
	decltype(StructToTuple<count_member_fields<T>(), T>::unevaluated), T&>;

// -------------------------------------------------------------------------
// Checking all of the members in a tuple to validate them.
template<typename T, typename... Ts>
__EVI_CONSTEVAL bool check_tuple_types(const std::tuple<T, Ts...>*) { 
	return std::conjunction_v<is_possible_type_in_struct<Ts>...> && std::conjunction_v<std::is_same<T, Ts>...>;	
}

// -------------------------------------------------------------------------
// Validating a possible struct.
template<typename T>
__EVI_CONSTEVAL bool validate_possible_structs()
{
	if constexpr(std::is_class_v<T>)
	{
		using tup = struct_to_tuple_t<T>;
		return check_tuple_types(static_cast<tup*>(nullptr));
	}

	return true;
}

// -------------------------------------------------------------------------
// ↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑
// -------------------------------------------------------------------------

// Getting the index of a location in a tuple.
template<size_t i, typename T, typename... Ts>
__EVI_CONSTEVAL size_t get_index_type()
{
	using element_t = std::tuple_element_t<i, std::tuple<Ts...>>;

	if constexpr(std::is_same_v<T, element_t>)
		return i;
	else
		return get_index_type<i + 1, T, Ts...>();
}

template<typename T, typename... Ts>
__EVI_CONSTEVAL size_t get_index_type()
{
	static_assert(std::disjunction_v<std::is_same<T, Ts>...>, "T is not found in the variadic template arguments!");
	return get_index_type<0, T, Ts...>();
}

// -------------------------------------------------------------------------
// Holds the the index of a variadic template, at compile-time.
template<typename... Ts>
class TypeHolder
{
	using type_code_t = ssize_t;

public:
	constexpr TypeHolder()
		: m_current_type(NoneCode) {}

	template<typename T>
	constexpr type_code_t get_index() const {
		return get_index_type<T, Ts...>();
	}

	template<typename T>
	constexpr void set_type() {
		m_current_type = get_index<T>(); 
	}	

	constexpr type_code_t get_type() const { 
		return m_current_type;
	}

	constexpr bool empty() const {
		return m_current_type == NoneCode;
	}

private:
	inline static __EVI_CONSTINIT type_code_t NoneCode = -1;
	inline static __EVI_CONSTINIT type_code_t Size = sizeof...(Ts) + 1;
	type_code_t m_current_type = 0;
};

} // namespace detail

// -------------------------------------------------------------------------
// Union all of the types.
template<typename... Ts>
class Union
{
	static_assert(sizeof...(Ts) > 0, "Insufficient amount of types.");
	static_assert(std::conjunction_v<detail::is_union_possible_type<Ts>...>, "Type is incorrect!");
	static_assert((detail::validate_possible_structs<Ts>() && ...), "Types in your struct are incorrect!");
    
    using first_element_t = std::tuple_element_t<0, std::tuple<Ts...>>;
    static_assert(((sizeof(first_element_t) == sizeof(Ts)) && ...), "Your types with different size!");

protected:
	detail::UnionImpl<Ts...>  m_union;
#ifdef EVI_USE_TYPEID
	using type_code_t = ssize_t;
	inline static __EVI_CONSTINIT type_code_t NoneCode = -1;
	type_code_t m_info = NoneCode;
#else
	detail::TypeHolder<Ts...> m_info;
#endif

#if 0
public:
	Union() { std::cout << sizeof(m_union) << "\n"; }
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
	inline T check_and_fix_endianness(const T& value) noexcept
	{
		T ret = value;

		static constexpr auto endian = static_cast<std::endian>(Endianness);
		if constexpr(endian != std::endian::native)
		{
#ifdef EVI_USE_TYPEID
			if(this->m_info != typeid(std::remove_cvref_t<T>).hash_code())
#else
			if(this->m_info.get_type() != this->m_info. template get_index<std::remove_cvref_t<T>>())
#endif
				ret = detail::BitsManipulation::swap_endian(value);

			// If you have containers or structures then the bits in one of the
			// endianness are reversed from the other, this is just reversing them.
			else if constexpr(sizeof(T) == sizeof(uint8_t) && std::is_integral_v<T>)
				ret = detail::BitsManipulation::reverse_byte(value);
		}

		return ret;
	}

	template<typename T>
	constexpr void assign_value(T& value)
	{
#ifdef EVI_USE_TYPEID
		// Insanely performance decrease.
		this->m_info = typeid(std::remove_cvref_t<T>).hash_code(); 
#else
		this->m_info. template set_type<std::remove_cvref_t<T>>();
#endif

		this->m_union.set_data(check_and_fix_endianness(value));
	}

public:
	constexpr SafeEndianUnion() noexcept = default;

	template<typename T>
	constexpr SafeEndianUnion(const T& value) noexcept { 
		assign_value(value); 
	}

	constexpr SafeEndianUnion(const SafeEndianUnion& other) noexcept 
	{
		this->m_union = other.m_union;
		this->m_info  = other.m_info;
	}

	constexpr SafeEndianUnion(SafeEndianUnion&& other) noexcept 
	{
		this->m_union = other.m_union;
		this->m_info  = other.m_info;
	}

	constexpr auto operator=(const SafeEndianUnion& other) noexcept
	{
		this->m_union = other.m_union;
		this->m_info  = other.m_info;
		return *this;
	}

	constexpr auto operator=(SafeEndianUnion&& other) noexcept
	{
		this->m_union = other.m_union;
		this->m_info  = other.m_info;
		return *this;
	}

	template<size_t i>
	constexpr auto get() noexcept
	{
		const auto value = this->m_union. template get_by_index<i>();
		return check_and_fix_endianness(value);
	}

	template<typename T>
	constexpr auto get() noexcept
	{
		const T value = this->m_union. template get_by_type<T>();
		return check_and_fix_endianness(value);
	}

	template<size_t i, typename T>
	constexpr void set(const T& value) {
       	assign_value(value);
	}

	template<typename T>
	constexpr void set(const T& value) {
		assign_value(value);
	}

	template<typename T>
	constexpr auto operator=(const T& value)
	{
		assign_value(value);
		return *this;
	}

	template<typename T>
	constexpr bool holds_alternative() const noexcept 
	{
#ifdef EVI_USE_TYPEID
		return this->m_info == typeid(T).hash_code();
#else
		return this->m_info.get_type() == this->m_info. template get_index<std::remove_cvref_t<T>>(); 
#endif
	}

	constexpr bool holds_anything() const noexcept 
	{
#ifdef EVI_USE_TYPEID
		return this->m_info == this->NoneCode;
#else
		return !this->m_info.empty();
#endif
	}
};

} // namespace evi


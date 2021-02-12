#include <bit>
#include <type_traits>
#include <cstddef>
#include <cstring>
#include <algorithm>
#include <array>
#include <system_error>
#include <tuple>
#include <utility>
#include <memory>

// Intrinsic functions for MSVC
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

// This code is causing undefined behivour, because of
// C++ Type Punning strict rules.
// http://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#c183-dont-use-a-union-for-type-punning
#if 0 
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
	static constexpr auto& get(UnionImpl<Ts...>& u) {
		return getter_index<i - 1>::get(u.rest);
	}
};

template<>
struct getter_index<0>
{
	template<typename T, typename... Ts>
	[[nodiscard]]
	static constexpr auto& get(UnionImpl<T, Ts...>& u) {
		return u.value;
	}
};

template<size_t i, typename... Ts>
[[nodiscard]] 
static constexpr auto& get_by_index(UnionImpl<Ts...>& u) 
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
	static constexpr Ret& get(UnionImpl<Ts...>& u) 
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
	static constexpr Ret& get(UnionImpl<T, Ts...>& u) 
	{
		if constexpr(std::is_same_v<decltype(u.value), Ret>)
			return u.value;

		// Unreachable code
		throw 0;	
	}
};

template<typename T, typename... Ts>
[[nodiscard]]
static constexpr T& get_by_type(UnionImpl<Ts...>& u) 
{
	static_assert(std::disjunction_v<std::is_same<T, Ts>...>, "T does not exists in the union.");
	return getter_type<sizeof...(Ts) - 1, T>::get(u);
}
#else

template<typename... Ts>
struct UnionImpl 
{
	static constexpr size_t data_size = std::max({sizeof(Ts)...});
	// using data_t = typename std::aligned_union_t<data_size, Ts...>;

    using data_t = std::array<std::byte, data_size>;
	data_t data;
};

template<typename T, typename... Ts>
static constexpr void set_data(const T& value, UnionImpl<Ts...>& u) 
{
    static_assert(std::disjunction_v<std::is_same<T, Ts>...>, "T does not exists in the union.");
    u.data = std::bit_cast<decltype(u.data)>(value);
}

template<size_t i, typename... Ts>
[[nodiscard]]
static constexpr auto get_by_index(UnionImpl<Ts...>& u)
{
	static_assert(i < sizeof...(Ts), "index is too big!");
	using element_t = typename std::tuple_element_t<i, std::tuple<Ts...>>;
	return std::bit_cast<element_t>(u.data);
}

template<typename T, typename... Ts>
[[nodiscard]]
static constexpr T get_by_type(UnionImpl<Ts...>& u)
{
	static_assert(std::disjunction_v<std::is_same<T, Ts>...>, "T does not exists in the union.");
	return std::bit_cast<T>(u.data);
}

#endif

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
			std::remove_cv_t<
				std::remove_pointer_t<std::remove_reference_t<T>>>, 
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
	static constexpr bool value = is_plain_type_v<T> 
		&& (is_struct_standard_layout_v<T> || is_bounded_array_v<T> || std::is_arithmetic_v<T>) 
		&& !std::is_union_v<T> 
		&& !std::is_enum_v<T>;
};

template<typename T>
static constexpr bool is_union_possible_type_v = is_union_possible_type<T>::value;

// Checks if a type is a possiblity member in a struct.
template<typename T>
struct is_possible_type_in_struct
{
	static constexpr bool value = is_plain_type_v<T> && 
		(is_bounded_array_v<T> || std::is_arithmetic_v<T>);
};

template<typename T>
static constexpr bool is_possible_type_in_struct_v = is_possible_type_in_struct<T>::value;

// -------------------------------------------------------------------------
// Swap endiannes:
// - 8 bytes.
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
		return ( value >> 24) 		       |
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
		return ( value >> 56) 			    |
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

#ifdef EVI_ENABLE_REFLECTION_SYSTEM
// -------------------------------------------------------------------------
// Simple reflection system to validate the types inside a struct.

// Class is convertible to anything.
struct UniversalType
{
	template<typename T>
	operator T() const;
};

// Counting the amount of members in a POD.
// NOTE: This only works for aggregate types.
template<typename T, typename... Ts>
// can be also consteval
constexpr auto count_member_fields(Ts... members)
{
	if constexpr( requires { T{members...}; } == false )
		return sizeof...(members) - 1;
	else
		return count_member_fields<T>(members..., UniversalType{});
} 

// Template specialization up to 32 fields in a struct.
template<size_t size, typename T>
struct StructToTuple;

#define __EVI_MAKE_STRUCT_TO_TUPLE_SPECIALIZATION(STRUCT, FIELDS_NUM, ...) \
	template<typename T> 						   \
	struct STRUCT<FIELDS_NUM, T> { 					   \
		constexpr static auto unevaluated(T& u) 		   \
		{							   \
			auto&& [__VA_ARGS__] = u;			   \
			return std::tuple{__VA_ARGS__};			   \
		}							   \
	}

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

// Checking all of the members in a tuple to validate them.
template<typename T, typename... Ts>
static constexpr bool check_tuple_types(std::tuple<T, Ts...>*) 
{
	constexpr bool first_member = is_possible_type_in_struct_v<T>;
	constexpr bool rest_members = std::conjunction_v<is_possible_type_in_struct<Ts>...>;
	constexpr bool all_same     = std::conjunction_v<std::is_same<T, Ts>...>;

	return first_member && rest_members && all_same;
}

// Converting the struct to tuple and validating it.
template<typename T>
constexpr bool validate_possible_structs()
	requires ( std::is_class_v<T> )
{
	constexpr auto size = count_member_fields<T>();
	if constexpr(size >= 2)
	{
		using U = std::invoke_result_t<decltype(StructToTuple<size, T>::unevaluated), T&>;
		return check_tuple_types((U*)nullptr);
	}
	
	return true;
}

#endif // EVI_ENABLE_REFLECTION_SYSTEM

template<typename T>
constexpr bool validate_possible_structs() {
	return true;
}


} // namespace detail

// -------------------------------------------------------------------------
// Union all of the types.
template<typename... Ts>
class Union
{
	static_assert(sizeof...(Ts) > 0, "Insufficient amount of types.");
	static_assert(std::conjunction_v<detail::is_union_possible_type<Ts>...>, "Type is incorrect!");
	static_assert((detail::validate_possible_structs<Ts>() && ...), "Types in your struct is incorrect!");

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

	template<typename T>
	constexpr void assign_value(const T& value)
	{
		m_type_code = typeid(T).hash_code();
	
		if constexpr(detail::is_struct_standard_layout_v<T> || detail::is_bounded_array_v<T>)
			set_data(check_and_fix_endianness(value), this->m_union);
		else
			set_data(value, this->m_union);
	}

public:
	constexpr SafeEndianUnion() noexcept = default;
	
	template<typename T>
	constexpr SafeEndianUnion(const T& value) { 
		set(value); 
	}

	constexpr SafeEndianUnion(const SafeEndianUnion& other) noexcept {
		this->m_union = other.m_union;
	}

	constexpr SafeEndianUnion(SafeEndianUnion&& other) noexcept {
		this->m_union = other.m_union;
	}

	constexpr auto operator=(const SafeEndianUnion& other) noexcept
	{
		this->m_union = other.m_union;
		return *this;
	}

	constexpr auto operator=(SafeEndianUnion&& other) noexcept
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
		T value = detail::get_by_type<T>(this->m_union);
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
	constexpr bool holds_alternative() noexcept {
		return typeid(T).hash_code() == m_type_code;
	}

private:
	using type_code = size_t;
	type_code m_type_code = 0;
};

} // namespace evi

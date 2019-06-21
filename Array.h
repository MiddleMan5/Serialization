/**
 *  @file Array.h
 *  @brief std::array extension which provides range construction and byte-stream concatenation
 *  @author Quinn Mikelson
 *
 */

#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace RangedArray
{
/** Alias for representing the type pointed to by an iterator
 *  Example: std::array<long, 3>().begin::value_type is "long"
 *  @tparam Iterator Any iterator with a value_type
 */
template <typename Iterator>
using ValueType = typename std::iterator_traits<Iterator>::value_type;

/** Compare two value_types from iterators
 *  @tparam ItA Iterator A
 *  @tparam ItB Iterator B
 */
template <typename ItA, typename ItB>
using sameValueType = std::is_same<ValueType<ItA>, ValueType<ItB>>;

/** Alias for determining array form given an Iterator and index pack
 *  Example: TypeArray<char*, 0, 1, 2, 3> returns std::array<char,4>
 *  @tparam Iterator Any iterator with a value_type
 *  @tparam I A std::index_sequence for the given iterator
 */
template <typename Iterator, std::size_t... I>
using TypeArray = std::array<ValueType<Iterator>, sizeof...(I)>;

/** Determine if all members of an index pack exist in some range from min to max
 *  @param min The smallest value an index can be
 *  @param max The largest value an index can be
 *  sequence A std::index_sequence
 *  @return (constexpr) true if all indices exist in range, false otherwise
 */
template <std::size_t... sequence>
static constexpr bool indexInRange(const std::size_t min,
                                   const std::size_t max,
                                   const std::index_sequence<sequence...>)
{
    if (min > max)
        return false;

    for (auto index : {sequence...})
    {
        if (index < min || index > max)
            return false;
    }

    return true;
}

/** Returns total aggregate size of parameter pack types
 *  Note: C++17 introduces fold expressions which would replace the need for this function-template
 *  @tparam T any class, type, or type-pack
 *  @return Total aggregate size, in bytes, of listed types
 */
template <typename... T>
constexpr std::size_t totalSize()
{
    // Create temporary variable to accumulate total type size
    std::size_t total_size = 0;
    // Alias (typedef): functional notation only works with simple types or alias'
    using expander = int[];

    /* Use functional notation to create temporary list that expands parameter pack,
     * use comma operator to sequentially accumulate type size in temporary variable
     */
    (void)expander{0, ((void)(total_size += sizeof(T)), 0)...};
    // Cast the unused variable to void and return accumulated size
    return total_size;
}

/** Construct a std::array using a forward_iterator and an index sequence
 *  @param first An iterator that represents the first element in a data set
 *  <unnamed> std::index_sequence with a size equal or less than original data set
 *  @return a std::array with members of type Iterator::value_type
 */
template <typename Iterator, std::size_t... I>
constexpr TypeArray<Iterator, I...> array(const Iterator first,
                                          const Iterator last,
                                          std::index_sequence<I...>)
{
    // Create array using index_sequence only if iterator range contains size
    const std::size_t range = (last - first >= 0) ? static_cast<std::size_t>(last - first)
                                                  : static_cast<std::size_t>(first - last);
    if (range <= sizeof...(I))
        return {first[I]...};
    else
        return {((void)I, std::numeric_limits<ValueType<Iterator>>::max())...};
}

/** Constexpr ranged construction of std::array. Iterators must be static expressions
 *  @tparam Iterator any iterator with a value::type
 *  @tparam size Explicit number of elements in output array, must be equal or larger than
 * bounds
 *  @param first The first iterator in an iterable type
 *  @param last The last iterator in an iterable type
 *  @return A std::array of the iterator's underlying type, constructed from first to last
 */
template <std::size_t size, typename Iterator>
constexpr auto array(const Iterator first, const Iterator last)
{
    return RangedArray::array(first, last, std::make_index_sequence<size>{});
}

/** Constexpr ranged construction of std::array. Iterators must be static expressions
 *  @tparam Iterator any iterator with a value::type
 *  @tparam size Explicit number of elements in output array, must be equal or larger than bounds
 *  @param first The first iterator in an iterable type
 *  @return A std::array of the iterator's underlying type, constructed from first to last
 */
template <std::size_t size, typename Iterator>
constexpr std::array<ValueType<Iterator>, size> array(const Iterator first)
{
    return RangedArray::array<size>(first, first + size);
}

/** Concatenates two iterators into std::array with the same underlying type
 *  @tparam ItA Iterator A type (should be equal to ItB)
 *  @tparam ItB Iterator B type (should be equal to ItA)
 *  @tparam IA std::index_sequence for iterator A
 *  @tparam IB std::index_sequence for iterator B
 *  @param A first Iterator to expand
 *  <unnamed> std::index_sequence to expand A with
 *  @param B second Iterator to expand
 *  <unnamed> std::index_sequence to expand B with
 *  @return std::array of original type, with size equal to total size of both arrays
 */
template <typename ItA, typename ItB, std::size_t... IA, std::size_t... IB>
constexpr auto merge(const ItA A,
                     std::index_sequence<IA...>,
                     const ItB B,
                     std::index_sequence<IB...>)
{
    static_assert(sameValueType<ItA, ItB>(), "Iterators do not have equivalent value_types");
    return TypeArray<ItA, IA..., IB...>{A[IA]..., B[IB]...};
}

/** Trivial merge (necessary for variadic collapse of merge to single parameter)
 *  @tparam T The type contained within the std::array
 *  @tparam size The number of elements in the std::array
 *  @param arr Any std::array
 *  @return The std::array passed
 */
template <typename T, std::size_t size>
constexpr std::array<T, size> merge(const std::array<T, size>& arr)
{
    return arr;
}

/** Merge the elements of two std::arrays
 *  @tparam T The type contained within the std::arrays
 *  @tparam sizeA The number of elements in arrA
 *  @tparam sizeB The number of elements in arrB
 *  @param arrA The first std::array to merge
 *  @param arrB The second std::array to merge
 *  @return A std::array conatining all elements in arrA, arrB
 */
template <typename T, std::size_t sizeA, std::size_t sizeB>
constexpr std::array<T, sizeA + sizeB> merge(const std::array<T, sizeA>& arrA,
                                             const std::array<T, sizeB>& arrB)
{
    return merge(&arrA.front(),
                 std::make_index_sequence<sizeA>{},
                 &arrB.front(),
                 std::make_index_sequence<sizeB>{});
}

/** Dispatches any number of std::arrays to two-parameter merge
 *  @tparam T The type contained within the std::array
 *  @tparam sizeA The number of elements in the arrA
 *  @tparam sizeB The number of elements in the arrB
 *  @tparam sizeN The number of elements subsequent std::arrays
 *  @param arrA The first std::array to concatenate
 *  @param arrB The second std::array to concatenate
 *  @param arrN Any number of std::arrays
 *  @return A std::array of the same type, containing a concatenation of all parameter arrays
 */
template <typename T, std::size_t sizeA, std::size_t sizeB, std::size_t... sizeN>
constexpr auto merge(const std::array<T, sizeA>& arrA,
                     const std::array<T, sizeB>& arrB,
                     const std::array<T, sizeN>&... arrN)
{
    return merge(RangedArray::merge(arrA, arrB), arrN...);
}

}  // namespace RangedArray

/** Functions used to split and manipulate data types as if they were std::arrays of uint8_t
 */
namespace ByteArray
{
// Fail to compile on big-endian sys*
static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__,
              "This library has only been tested on Little-Endian systems!");

/* Struct is_native evaluates to true on little-endian systems
 */
struct LSB
{
    static constexpr bool is_native = __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__;
};

/* Struct is_native evaluates to true on big-endian systems
 */
struct MSB
{
    static constexpr bool is_native = !(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__);
};

/** Can be used to evaluate LSB and MSB and declare intended reversals
 *  @tparam byte_order A class containing an is_native boolean expression
 */
template <class byte_order>
struct ByteOrder : byte_order
{
    using value_type = decltype(byte_order::is_native);
};

// Use RangedArray's totalSize() functions
using RangedArray::totalSize;

// Use RangedArray's value_type evaluation
using RangedArray::ValueType;

/** Evaluates to array of uint8_t bytes equal to the size of the input type only if the evaluation
 * of ByteOrder<LSB>::is_native is equal to the input argument.
 *  Example: byte_array_if<uint8_t, is_integral, true> evaluates to a std::array, and so does
 *           byte_array_if<MyClass, is_integral, false> (because MyClass is not an integer type)
 */
template <typename T, bool arg>
using byte_array_if =
    std::enable_if_t<ByteOrder<LSB>::is_native == arg, std::array<uint8_t, sizeof(T)>>;

/** Split any integral type into an array of bytes representing the original data (LSB)
 *  @tparam T any integral type
 *  @param value A reference to any integral type
 *  @return a std::array of uint8_t bytes with a size equal to size of specified type
 */
template <typename T, bool is_native = true>
inline static byte_array_if<T, is_native> byteArray(const T& value)
{
    return RangedArray::array<sizeof(T)>(
        &reinterpret_cast<const std::array<uint8_t, sizeof(T)>&>(value).front());
}

/** Split any integral type into an array of bytes in reverse order
 *  @tparam T any integral type
 *  @param value A reference to any integral type
 *  @return a std::array of uint8_t bytes with a size equal to size of specified type
 */
template <typename T, bool is_native = true>
inline static byte_array_if<T, !is_native> byteArray(const T& value)
{
    return RangedArray::array<sizeof(T)>(
        reinterpret_cast<const std::array<uint8_t, sizeof(T)>&>(value).crbegin());
}

/** Sequentially evaluate Iterator elements with byteArray() and merge result
 *  @tparam Iterator Any iterator with a value_type
 *  @tparam I a std::index_sequence representing the indexes to evaluate
 *  @param first The first iterator in an iterable type
 *  @return a std::array of uint8_t bytes with a size equal to size of specified type
 */
template <typename Iterator, std::size_t... I>
constexpr auto byteArray(const Iterator first, std::index_sequence<I...>)
{
    return RangedArray::merge(byteArray(first[I])...);
}

/** Sequentially evaluate Iterator with explicit size
 *  @tparam size Number of elements in iterable sequence
 *  @tparam Iterator Any iterator with a value_type
 *  @param first The first iterator in an iterable type
 *  @return a std::array of uint8_t bytes with a size equal to specified size * value_type
 */
template <std::size_t size, typename Iterator>
constexpr auto byteArray(const Iterator first)
{
    return byteArray<Iterator>(first, std::make_index_sequence<size>{});
}

/** Sequentially split elements in an array into bytes and merge the result
 *  @tparam T The type contained within the std::array
 *  @tparam size The number of elements in the std::array
 *  @param arr A std::array of any type to split into bytes
 *  @return a std::array of uint8_t bytes with a size equal to total size of original array
 */
template <typename T, std::size_t size>
constexpr std::array<uint8_t, size * sizeof(T)> byteArray(const std::array<T, size>& arr)
{
    return byteArray<size>(&arr.front());
}

/** Merge two values of any type and concatenate the result into a std::array
 *  @tparam T1 The type of the first parameter
 *  @tparam T2 The type of the second parameter
 *  @param value1 Any value to split into bytes
 *  @param value2 Any value to split into bytes
 *  @return a std::array of uint8_t bytes with a size equal to combined size of the parameter types
 */
template <typename T1, typename T2>
constexpr std::array<uint8_t, sizeof(T1) + sizeof(T2)> byteArray(const T1& value1, const T2& value2)
{
    return RangedArray::merge(byteArray(value1), byteArray(value2));
}

/** Dispatches any number of types to two-parameter byteArray()
 *  @tparam T1 The type of the first parameter
 *  @tparam T2 The type of the second parameter
 *  @tparam Tn The type of any remaining parameters
 *  @param value1 The first value to dispatch
 *  @param value2 The second value to dispatch
 *  @param valueN Any number of values of any type
 *  @return A std::array of uint8_t bytes, containing a concatenation of all parameters
 */
template <typename T1, typename T2, typename... Tn>
constexpr std::array<uint8_t, totalSize<T1, T2, Tn...>()> byteArray(const T1& value1,
                                                                    const T2& value2,
                                                                    const Tn&... valueN)
{
    return byteArray(byteArray(value1, value2), byteArray(valueN)...);
}

/** Evaluate the elements of an array of any type, splitting the underlying elements first before
 *  reversing the resulting byte array.
 *  @tparam T the type of parameter to operate on
 *  @tparam size The number of elements in the array
 *  @param arr Any std::array
 *  @return A std::array of uint8_t bytes, representing the original data, in reverse
 */
template <typename T, std::size_t size>
constexpr std::array<uint8_t, sizeof(T) * size> reverseByteArray(const std::array<T, size>& arr)
{
    return byteArray(RangedArray::array<size>(arr.crbegin(), arr.crend()));
}

/** Evaluate byteArray with explicit enable set to false (enables reversal behavior)
 *  @tparam T the type of parameter to operate on
 *  @param value The parameter to evaluate
 *  @return A std::array of uint8_t bytes, representing the original data, in reverse
 */
template <typename T>
constexpr std::array<uint8_t, sizeof(T)> reverseByteArray(const T& value)
{
    return byteArray<const T, false>(value);
}

/** Returns specified type using an byte iterator and an index sequence
 *  Completely compile-time constant conversion of array of bytes to integral type
 *  @tparam T the type to convert byte array to
 *  @tparam Iterator The type of constant iterator to use
 *  @tparam I a std::index_sequence
 *  @param LSB the least-significant byte of a byte array (IE: array.cbegin())
 *  @return The specified type with data from array
 */
template <typename T, typename Iterator, std::size_t... I>
constexpr T getInteger(const Iterator LSB, std::index_sequence<I...>)
{
    // Create temporary variable to accumulate integer sequence
    T return_value = 0;

    // Alias (typedef): functional notation only works with simple types or alias'
    using expander = const int[];

    // Use iterator to byte-array (starting at LSB) and convert to specified type T
    (void)expander{0, ((void)(return_value += static_cast<T>(LSB[I]) << (8 * I)), 0)...};
    // Cast the unused variable to void and return accumulated value
    return return_value;
}

/** Returns specified type using an byte iterator and a size,
 *  Completely compile-time constant conversion of array of bytes to integral type
 *  @tparam T the type to convert byte array to
 *  @tparam size The number of bytes to read starting from the iteratorf
 *  @param first A const-iterator to a std::array of bytes (IE: array.cbegin())
 *  @return The specified type with data from array
 */
template <typename T, std::size_t size, typename Iterator>
constexpr T getInteger(const Iterator first)
{
    return getInteger<T, Iterator>(first, std::make_index_sequence<size>{});
}

/** Returns specified type using any std::array of uint8_t bytes
 *  Completely compile-time constant conversion of array of bytes to integral type
 *  @tparam T the type to convert byte array to
 *  @tparam size The size of the array, must be equal to size of type
 *  @param arr A reference to a std::array of uint8_t bytes
 *  @return The specified type with data from array
 */
template <typename T, std::size_t size>
constexpr T getInteger(const std::array<uint8_t, size>& arr)
{
    static_assert(sizeof(T) >= sizeof(arr), "Type specified cannot hold array data");
    return getInteger<T>(&arr.front(), std::make_index_sequence<size>{});
}

/** Trivial case specialization
 *  @tparam T Any integral data type to convert to
 *  @tparam Tval An equivalent type greater or equal than T, represents input data type
 *  @tparam offset The number of bytes to right-shift data before returning
 *  @param value A reference to an integral data type
 *  @return The value supplied, optionally shifted by offset
 */
template <typename T, typename Tval, std::size_t offset = 0>
constexpr T getInteger(const Tval& value)
{
    static_assert(std::is_integral<Tval>::value && std::is_integral<T>::value,
                  "Type specified is not integral");
    static_assert(sizeof(Tval) >= offset, "Shift size is larger than argument");
    return static_cast<T>(value >> 8 * offset);
}

}  // namespace ByteArray


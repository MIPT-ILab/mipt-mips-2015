/**
 * macro.h - Implementation of useful inline functions
 *
 * @author Pavel Kryukov <pavel.kryukov@phystech.edu>
 * Copyright 2017-2018 MIPT-MIPS
 */

// protection from multi-include
#ifndef COMMON__MACRO_H
#define COMMON__MACRO_H

#include <bitset>
#include <limits>
#include <type_traits>
#include <algorithm>

#include <infra/types.h>

/* Returns size of a static array */
template<typename T, size_t N>
constexpr size_t countof( const T (& /* unused */)[N]) noexcept { return N; }

/* Checks if values is power of two */
template<typename T>
constexpr bool is_power_of_two( const T& n) noexcept { return (n & (n - 1)) == 0; }

/* Ignore return value */
template<typename T>
void ignored( const T& /* unused */) noexcept { }

/* Find minimal sizeof */
template<typename ... Args>
constexpr size_t min_sizeof() noexcept { return std::min({sizeof(Args)...}); }

/* Find maximal sizeof */
template<typename ... Args>
constexpr size_t max_sizeof() noexcept { return std::max({sizeof(Args)...}); }

/* Bit width of integer type */
template<typename T>
constexpr size_t bitwidth = std::numeric_limits<T>::digits + std::numeric_limits<T>::is_signed;

/* 128 types have no std::numeric_limits */
template<> constexpr size_t bitwidth<uint128> = 128u;
template<> constexpr size_t bitwidth<int128> = 128u;

// https://stackoverflow.com/questions/109023/how-to-count-the-number-of-set-bits-in-a-32-bit-integer
template<typename T,
         typename = std::enable_if_t<std::is_integral<T>::value>,         // only integral
         typename = std::enable_if_t<std::numeric_limits<T>::radix == 2>, // only binary
         typename = std::enable_if_t<bitwidth<T> <= bitwidth<uint64>> // only narrow
       >
constexpr auto popcount( T x) noexcept
{
    return std::bitset<bitwidth<T>>( static_cast<typename std::make_unsigned<T>::type>( x)).count();
}

template <typename T>
static constexpr T bitmask(unsigned int const onecount)
{
    return onecount != 0 ? (~static_cast<T>(0) >> (bitwidth<T> - onecount)) : static_cast<T>(0);
}

template <typename T>
static constexpr T arifmetic_rs(const T& value, size_t shamt)
{
    using ST = sign_t<T>;
    //NOLINTNEXTLINE
    if constexpr ((static_cast<ST>(-2) >> 1u) == static_cast<ST>(-1)) {
        // Compiler does arithmetic shift for signed values, trust it
        //NOLINTNEXTLINE : clang warns about implementation defined code, but we checked it
        return static_cast<ST>(value) >> shamt;
    }
    return (value & ~bitmask<T>(bitwidth<T> - 1)) == 0 // check MSB
             ? value >> shamt // just shift for positives
             : ~((~value) >> shamt); // propagate ones
}
#endif

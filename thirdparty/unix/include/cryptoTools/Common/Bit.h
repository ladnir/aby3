# pragma once

// boost/core/bit.hpp
//
// A portable version of the C++20 standard header <bit>
//
// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <stdint.h>
#include <limits>
#include <cstring>


#define OC_M_64
#if defined(_MSC_VER)
# include <intrin.h>
# pragma intrinsic(_BitScanForward)
# pragma intrinsic(_BitScanReverse)
# if defined(OC_M_64)
#  pragma intrinsic(_BitScanForward64)
#  pragma intrinsic(_BitScanReverse64)
# endif
#endif // defined(_MSC_VER)

namespace osuCrypto
{

        // bit_cast

        template<class To, class From>
        To bit_cast(From const& from) noexcept
        {
            static_assert(sizeof(To) == sizeof(From), "");

            To to;
            std::memcpy(&to, &from, sizeof(To));
            return to;
        }

        // countl

#if defined(__GNUC__) || defined(__clang__)

        namespace detail
        {

            constexpr inline int countl_impl(unsigned char x) noexcept
            {
                return x ? __builtin_clz(x) - (std::numeric_limits<unsigned int>::digits - std::numeric_limits<unsigned char>::digits) : std::numeric_limits<unsigned char>::digits;
            }

            constexpr inline int countl_impl(unsigned short x) noexcept
            {
                return x ? __builtin_clz(x) - (std::numeric_limits<unsigned int>::digits - std::numeric_limits<unsigned short>::digits) : std::numeric_limits<unsigned short>::digits;
            }

            constexpr inline int countl_impl(unsigned int x) noexcept
            {
                return x ? __builtin_clz(x) : std::numeric_limits<unsigned int>::digits;
            }

            constexpr inline int countl_impl(unsigned long x) noexcept
            {
                return x ? __builtin_clzl(x) : std::numeric_limits<unsigned long>::digits;
            }

            constexpr inline int countl_impl(unsigned long long x) noexcept
            {
                return x ? __builtin_clzll(x) : std::numeric_limits<unsigned long long>::digits;
            }

        } // namespace detail

        template<class T>
        constexpr int countl_zero(T x) noexcept
        {
            return osuCrypto::detail::countl_impl(x);
        }

#else // defined(__GNUC__) || defined(__clang__)

        namespace detail
        {

            inline int countl_impl(std::uint32_t x) noexcept
            {
#if defined(_MSC_VER)

                unsigned long r;

                if (_BitScanReverse(&r, x))
                {
                    return 31 - static_cast<int>(r);
                }
                else
                {
                    return 32;
                }

#else

                static unsigned char const mod37[37] = { 32, 31, 6, 30, 9, 5, 0, 29, 16, 8, 2, 4, 21, 0, 19, 28, 25, 15, 0, 7, 10, 1, 17, 3, 22, 20, 26, 0, 11, 18, 23, 27, 12, 24, 13, 14, 0 };

                x |= x >> 1;
                x |= x >> 2;
                x |= x >> 4;
                x |= x >> 8;
                x |= x >> 16;

                return mod37[x % 37];

#endif
            }

            inline int countl_impl(std::uint64_t x) noexcept
            {
#if defined(_MSC_VER) && defined(OC_M_64)

                unsigned long r;

                if (_BitScanReverse64(&r, x))
                {
                    return 63 - static_cast<int>(r);
                }
                else
                {
                    return 64;
                }

#else

                return static_cast<std::uint32_t>(x >> 32) != 0 ?
                    osuCrypto::detail::countl_impl(static_cast<std::uint32_t>(x >> 32)) :
                    osuCrypto::detail::countl_impl(static_cast<std::uint32_t>(x)) + 32;

#endif
            }

            inline int countl_impl(std::uint8_t x) noexcept
            {
                return osuCrypto::detail::countl_impl(static_cast<std::uint32_t>(x)) - 24;
            }

            inline int countl_impl(std::uint16_t x) noexcept
            {
                return osuCrypto::detail::countl_impl(static_cast<std::uint32_t>(x)) - 16;
            }

        } // namespace detail

        template<class T>
        int countl_zero(T x) noexcept
        {
            static_assert(sizeof(T) == sizeof(std::uint8_t) || sizeof(T) == sizeof(std::uint16_t) || sizeof(T) == sizeof(std::uint32_t) || sizeof(T) == sizeof(std::uint64_t), "");

            if (sizeof(T) == sizeof(std::uint8_t))
            {
                return osuCrypto::detail::countl_impl(static_cast<std::uint8_t>(x));
            }
            else if (sizeof(T) == sizeof(std::uint16_t))
            {
                return osuCrypto::detail::countl_impl(static_cast<std::uint16_t>(x));
            }
            else if (sizeof(T) == sizeof(std::uint32_t))
            {
                return osuCrypto::detail::countl_impl(static_cast<std::uint32_t>(x));
            }
            else
            {
                return osuCrypto::detail::countl_impl(static_cast<std::uint64_t>(x));
            }
        }

#endif // defined(__GNUC__) || defined(__clang__)

        template<class T>
        constexpr int countl_one(T x) noexcept
        {
            return osuCrypto::countl_zero(static_cast<T>(~x));
        }

        // countr

#if defined(__GNUC__) || defined(__clang__)

        namespace detail
        {

            constexpr inline int countr_impl(unsigned char x) noexcept
            {
                return x ? __builtin_ctz(x) : std::numeric_limits<unsigned char>::digits;
            }

            constexpr inline int countr_impl(unsigned short x) noexcept
            {
                return x ? __builtin_ctz(x) : std::numeric_limits<unsigned short>::digits;
            }

            constexpr inline int countr_impl(unsigned int x) noexcept
            {
                return x ? __builtin_ctz(x) : std::numeric_limits<unsigned int>::digits;
            }

            constexpr inline int countr_impl(unsigned long x) noexcept
            {
                return x ? __builtin_ctzl(x) : std::numeric_limits<unsigned long>::digits;
            }

            constexpr inline int countr_impl(unsigned long long x) noexcept
            {
                return x ? __builtin_ctzll(x) : std::numeric_limits<unsigned long long>::digits;
            }

        } // namespace detail

        template<class T>
        constexpr int countr_zero(T x) noexcept
        {
            return osuCrypto::detail::countr_impl(x);
        }

#else // defined(__GNUC__) || defined(__clang__)

        namespace detail
        {

            inline int countr_impl(std::uint32_t x) noexcept
            {
#if defined(_MSC_VER)

                unsigned long r;

                if (_BitScanForward(&r, x))
                {
                    return static_cast<int>(r);
                }
                else
                {
                    return 32;
                }

#else

                static unsigned char const mod37[37] = { 32, 0, 1, 26, 2, 23, 27, 0, 3, 16, 24, 30, 28, 11, 0, 13, 4, 7, 17, 0, 25, 22, 31, 15, 29, 10, 12, 6, 0, 21, 14, 9, 5, 20, 8, 19, 18 };
                return mod37[(-(std::int32_t)x & x) % 37];

#endif
            }

            inline int countr_impl(std::uint64_t x) noexcept
            {
#if defined(_MSC_VER) && defined(OC_M_64)

                unsigned long r;

                if (_BitScanForward64(&r, x))
                {
                    return static_cast<int>(r);
                }
                else
                {
                    return 64;
                }

#else

                return static_cast<std::uint32_t>(x) != 0 ?
                    osuCrypto::detail::countr_impl(static_cast<std::uint32_t>(x)) :
                    osuCrypto::detail::countr_impl(static_cast<std::uint32_t>(x >> 32)) + 32;

#endif
            }

            inline int countr_impl(std::uint8_t x) noexcept
            {
                return osuCrypto::detail::countr_impl(static_cast<std::uint32_t>(x) | 0x100);
            }

            inline int countr_impl(std::uint16_t x) noexcept
            {
                return osuCrypto::detail::countr_impl(static_cast<std::uint32_t>(x) | 0x10000);
            }

        } // namespace detail

        template<class T>
        int countr_zero(T x) noexcept
        {
            static_assert(sizeof(T) == sizeof(std::uint8_t) || sizeof(T) == sizeof(std::uint16_t) || sizeof(T) == sizeof(std::uint32_t) || sizeof(T) == sizeof(std::uint64_t));

            if (sizeof(T) == sizeof(std::uint8_t))
            {
                return osuCrypto::detail::countr_impl(static_cast<std::uint8_t>(x));
            }
            else if (sizeof(T) == sizeof(std::uint16_t))
            {
                return osuCrypto::detail::countr_impl(static_cast<std::uint16_t>(x));
            }
            else if (sizeof(T) == sizeof(std::uint32_t))
            {
                return osuCrypto::detail::countr_impl(static_cast<std::uint32_t>(x));
            }
            else
            {
                return osuCrypto::detail::countr_impl(static_cast<std::uint64_t>(x));
            }
        }

#endif // defined(__GNUC__) || defined(__clang__)

        template<class T>
        constexpr int countr_one(T x) noexcept
        {
            return osuCrypto::countr_zero(static_cast<T>(~x));
        }

        // popcount

#if defined(__GNUC__) || defined(__clang__)

#if defined(__clang__) && __clang_major__ * 100 + __clang_minor__ < 304
# define OC_POPCOUNT_CONSTEXPR
#else
# define OC_POPCOUNT_CONSTEXPR constexpr
#endif

        namespace detail
        {

            OC_POPCOUNT_CONSTEXPR inline int popcount_impl(unsigned char x) noexcept
            {
                return __builtin_popcount(x);
            }

            OC_POPCOUNT_CONSTEXPR inline int popcount_impl(unsigned short x) noexcept
            {
                return __builtin_popcount(x);
            }

            OC_POPCOUNT_CONSTEXPR inline int popcount_impl(unsigned int x) noexcept
            {
                return __builtin_popcount(x);
            }

            OC_POPCOUNT_CONSTEXPR inline int popcount_impl(unsigned long x) noexcept
            {
                return __builtin_popcountl(x);
            }

            OC_POPCOUNT_CONSTEXPR inline int popcount_impl(unsigned long long x) noexcept
            {
                return __builtin_popcountll(x);
            }

        } // namespace detail

#undef OC_POPCOUNT_CONSTEXPR

        template<class T>
        constexpr int popcount(T x) noexcept
        {
            return osuCrypto::detail::popcount_impl(x);
        }

#else // defined(__GNUC__) || defined(__clang__)

        namespace detail
        {

            constexpr inline int popcount_impl(std::uint32_t x) noexcept
            {
                x = x - ((x >> 1) & 0x55555555);
                x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
                x = (x + (x >> 4)) & 0x0F0F0F0F;

                return static_cast<unsigned>((x * 0x01010101) >> 24);
            }

            constexpr inline int popcount_impl(std::uint64_t x) noexcept
            {
                x = x - ((x >> 1) & 0x5555555555555555);
                x = (x & 0x3333333333333333) + ((x >> 2) & 0x3333333333333333);
                x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0F;

                return static_cast<unsigned>((x * 0x0101010101010101) >> 56);
            }

        } // namespace detail

        template<class T>
        constexpr int popcount(T x) noexcept
        {
            static_assert(sizeof(T) <= sizeof(std::uint64_t), "");

            if (sizeof(T) <= sizeof(std::uint32_t))
            {
                return osuCrypto::detail::popcount_impl(static_cast<std::uint32_t>(x));
            }
            else
            {
                return osuCrypto::detail::popcount_impl(static_cast<std::uint64_t>(x));
            }
        }

#endif // defined(__GNUC__) || defined(__clang__)

        // rotating

        template<class T>
        constexpr T rotl(T x, int s) noexcept
        {
            unsigned const mask = std::numeric_limits<T>::digits - 1;
            return x << (s & mask) | x >> ((-s) & mask);
        }

        template<class T>
        constexpr T rotr(T x, int s) noexcept
        {
            unsigned const mask = std::numeric_limits<T>::digits - 1;
            return x >> (s & mask) | x << ((-s) & mask);
        }

        // integral powers of 2

        template<class T>
        constexpr bool has_single_bit(T x) noexcept
        {
            return x != 0 && (x & (x - 1)) == 0;
        }

        template<class T>
        constexpr T bit_width(T x) noexcept
        {
            return std::numeric_limits<T>::digits - osuCrypto::countl_zero(x);
        }

        template<class T>
        constexpr T bit_floor(T x) noexcept
        {
            return x == 0 ? 0 : T(1) << (osuCrypto::bit_width(x) - 1);
        }

        namespace detail
        {

            constexpr inline std::uint32_t bit_ceil_impl(std::uint32_t x) noexcept
            {
                if (x == 0)
                {
                    return 0;
                }

                --x;

                x |= x >> 1;
                x |= x >> 2;
                x |= x >> 4;
                x |= x >> 8;
                x |= x >> 16;

                ++x;

                return x;
            }

            constexpr inline std::uint64_t bit_ceil_impl(std::uint64_t x) noexcept
            {
                if (x == 0)
                {
                    return 0;
                }

                --x;

                x |= x >> 1;
                x |= x >> 2;
                x |= x >> 4;
                x |= x >> 8;
                x |= x >> 16;
                x |= x >> 32;

                ++x;

                return x;
            }

        } // namespace detail

        template<class T>
        constexpr T bit_ceil(T x) noexcept
        {
            static_assert(sizeof(T) <= sizeof(std::uint64_t),"");

            if (sizeof(T) <= sizeof(std::uint32_t))
            {
                return static_cast<T>(osuCrypto::detail::bit_ceil_impl(static_cast<std::uint32_t>(x)));
            }
            else
            {
                return static_cast<T>(osuCrypto::detail::bit_ceil_impl(static_cast<std::uint64_t>(x)));
            }
        }

//        // endian
//
//#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
//
//# define OC_BIT_NATIVE_INITIALIZER =little
//
//#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
//
//# define OC_BIT_NATIVE_INITIALIZER =big
//
//#elif defined(__BYTE_ORDER__) && defined(__ORDER_PDP_ENDIAN__) && __BYTE_ORDER__ == __ORDER_PDP_ENDIAN__
//
//# define OC_BIT_NATIVE_INITIALIZER
//
//#elif defined(__LITTLE_ENDIAN__)
//
//# define OC_BIT_NATIVE_INITIALIZER =little
//
//#elif defined(__BIG_ENDIAN__)
//
//# define OC_BIT_NATIVE_INITIALIZER =big
//
//#elif defined(_MSC_VER) || defined(__i386__) || defined(__x86_64__)
//
//# define OC_BIT_NATIVE_INITIALIZER =little
//
//#else
//
//# define OC_BIT_NATIVE_INITIALIZER
//
//#endif
//
//
//    enum class endian
//    {
//        big,
//        little,
//        native OC_BIT_NATIVE_INITIALIZER
//    };
//
//    typedef endian endian_type;


#undef OC_BIT_NATIVE_INITIALIZER

} // namespace osuCrypto


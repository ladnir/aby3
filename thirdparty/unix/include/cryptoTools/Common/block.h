#pragma once
#include "cryptoTools/Common/config.h"
#include <cstdint>
#include <array>
#include <iostream>
#include <memory>
#include <new>
#include <string.h>
#include <cassert>

#ifdef OC_ENABLE_SSE2
#include <emmintrin.h>
#include <smmintrin.h>
#endif
#ifdef OC_ENABLE_PCLMUL
#include <wmmintrin.h>
#endif

// OC_FORCEINLINE ---------------------------------------------//
// Macro to use in place of 'inline' to force a function to be inline
#if !defined(OC_FORCEINLINE)
#  if defined(_MSC_VER)
#    define OC_FORCEINLINE __forceinline
#  elif defined(__GNUC__) && __GNUC__ > 3
	 // Clang also defines __GNUC__ (as 4)
#    define OC_FORCEINLINE inline __attribute__ ((__always_inline__))
#  else
#    define OC_FORCEINLINE inline
#  endif
#endif


namespace osuCrypto
{
	struct alignas(16) block
	{
#ifdef OC_ENABLE_SSE2
		__m128i mData;
#else
		std::uint64_t mData[2];
#endif

		block() = default;
		block(const block&) = default;
#ifdef OC_ENABLE_SSE2
		block(uint64_t x1, uint64_t x0)
		{
			mData = _mm_set_epi64x(x1, x0);
		}
#else
		block(uint64_t x1, uint64_t x0)
			: block(std::array<std::uint64_t, 2> {x0, x1}) {}
#endif

#ifdef OC_ENABLE_SSE2
		block(char e15, char e14, char e13, char e12, char e11, char e10, char e9, char e8, char e7, char e6, char e5, char e4, char e3, char e2, char e1, char e0)
		{
			mData = _mm_set_epi8(e15, e14, e13, e12, e11, e10, e9, e8, e7, e6, e5, e4, e3, e2, e1, e0);
		}
#else
		block(char e15, char e14, char e13, char e12, char e11, char e10, char e9, char e8, char e7, char e6, char e5, char e4, char e3, char e2, char e1, char e0)
			: block(std::array<char, 16> {
			e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15
		}) {}
#endif

		explicit block(uint64_t x)
		{
			*this = block(0, x);
		}

		template<typename T,
			typename Enable = typename std::enable_if<
			std::is_pod<T>::value &&
			(sizeof(T) <= 16) &&
			(16 % sizeof(T) == 0)
		>::type>
			block(const std::array<T, 16 / sizeof(T)>& arr)
		{
			memcpy(data(), arr.data(), 16);
		}

#ifdef OC_ENABLE_SSE2
		block(const __m128i& x)
		{
			mData = x;
		}

		OC_FORCEINLINE operator const __m128i& () const
		{
			return mData;
		}
		OC_FORCEINLINE operator __m128i& ()
		{
			return mData;
		}

		OC_FORCEINLINE __m128i& m128i()
		{
			return mData;
		}
		OC_FORCEINLINE const __m128i& m128i() const
		{
			return mData;
		}
#endif

		OC_FORCEINLINE unsigned char* data()
		{
			return (unsigned char*)&mData;
		}

		OC_FORCEINLINE const unsigned char* data() const
		{
			return (const unsigned char*)&mData;
		}

#ifdef OC_ENABLE_DEPRECATED_BLOCK_AS
		template<typename T>
		typename std::enable_if<
		    std::is_standard_layout<T>::value&&
		    std::is_trivial<T>::value &&
		    (sizeof(T) <= 16) &&
		    (16 % sizeof(T) == 0)
		    ,
		    std::array<T, 16 / sizeof(T)>&
		>::type as()
		{
		    return *(std::array<T, 16 / sizeof(T)>*)this;
		}
#endif

		template<typename T>
		OC_FORCEINLINE typename std::enable_if<
			std::is_standard_layout<T>::value&&
			std::is_trivial<T>::value &&
			(sizeof(T) <= 16) &&
			(16 % sizeof(T) == 0)
			,
			std::array<T, 16 / sizeof(T)>
		>::type get() const
		{
			std::array<T, 16 / sizeof(T)> output;
			memcpy(output.data(), data(), 16);
			return output;
		}


		template<typename T>
		OC_FORCEINLINE typename std::enable_if<
			std::is_standard_layout<T>::value&&
			std::is_trivial<T>::value &&
			(sizeof(T) <= 16) &&
			(16 % sizeof(T) == 0)
			,
			T
		>::type get(size_t index) const
		{
			assert(index < 16 / sizeof(T));
			T output;
			memcpy(&output, data() + sizeof(T) * index, sizeof(T));
			return output;
		}

		template<typename T>
		OC_FORCEINLINE typename std::enable_if<
			std::is_standard_layout<T>::value&&
			std::is_trivial<T>::value &&
			(sizeof(T) <= 16) &&
			(16 % sizeof(T) == 0)
		>::type set(size_t index, T v)
		{
			memcpy(data() + sizeof(T) * index, &v, sizeof(T));
		}

		// For integer types, this will be specialized with SSE futher down.
		template<typename T>
		OC_FORCEINLINE static typename std::enable_if<
			std::is_pod<T>::value &&
			(sizeof(T) <= 16) &&
			(16 % sizeof(T) == 0),
			block>::type allSame(T val)
		{
			std::array<T, 16 / sizeof(T)> arr;
			for (T& x : arr)
				x = val;
			return arr;
		}

		OC_FORCEINLINE  osuCrypto::block operator^(const osuCrypto::block& rhs) const
		{
#ifdef OC_ENABLE_SSE2
			return mm_xor_si128(rhs);
#else
			return cc_xor_si128(rhs);
#endif
		}
#ifdef OC_ENABLE_SSE2
		OC_FORCEINLINE  osuCrypto::block mm_xor_si128(const osuCrypto::block& rhs) const
		{
			return _mm_xor_si128(*this, rhs);
		}
#endif
		OC_FORCEINLINE  osuCrypto::block cc_xor_si128(const osuCrypto::block& rhs) const
		{
			auto ret = get<std::uint64_t>();
			auto rhsa = rhs.get<std::uint64_t>();
			ret[0] ^= rhsa[0];
			ret[1] ^= rhsa[1];
			return ret;
		}

		OC_FORCEINLINE  osuCrypto::block& operator^=(const osuCrypto::block& rhs)
		{
			*this = *this ^ rhs;
			return *this;
		}

		OC_FORCEINLINE  block operator~() const
		{
			return *this ^ block(-1, -1);
		}



#ifdef OC_ENABLE_SSE2
		OC_FORCEINLINE block mm_andnot_si128(const block& rhs) const
		{
			return ::_mm_andnot_si128(*this, rhs);
		}
#endif

		OC_FORCEINLINE block cc_andnot_si128(const block& rhs) const
		{
			return ~*this & rhs;
		}

		OC_FORCEINLINE block andnot_si128(const block& rhs) const
		{
#ifdef OC_ENABLE_SSE2
			return mm_andnot_si128(rhs);
#else
			return cc_andnot_si128(rhs);
#endif
		}

		OC_FORCEINLINE  osuCrypto::block operator&(const osuCrypto::block& rhs)const
		{
#ifdef OC_ENABLE_SSE2
			return mm_and_si128(rhs);
#else
			return cc_and_si128(rhs);
#endif
		}

		OC_FORCEINLINE  osuCrypto::block& operator&=(const osuCrypto::block& rhs)
		{
			*this = *this & rhs;
			return *this;
		}

#ifdef OC_ENABLE_SSE2
		OC_FORCEINLINE  osuCrypto::block mm_and_si128(const osuCrypto::block& rhs)const
		{
			return _mm_and_si128(*this, rhs);
		}
#endif
		OC_FORCEINLINE  osuCrypto::block cc_and_si128(const osuCrypto::block& rhs)const
		{
			auto ret = get<std::uint64_t>();
			auto rhsa = rhs.get<std::uint64_t>();
			ret[0] &= rhsa[0];
			ret[1] &= rhsa[1];
			return ret;
		}


		OC_FORCEINLINE  osuCrypto::block operator|(const osuCrypto::block& rhs)const
		{
#ifdef OC_ENABLE_SSE2
			return mm_or_si128(rhs);
#else
			return cc_or_si128(rhs);
#endif
		}
#ifdef OC_ENABLE_SSE2
		OC_FORCEINLINE  osuCrypto::block mm_or_si128(const osuCrypto::block& rhs)const
		{
			return _mm_or_si128(*this, rhs);
		}
#endif
		OC_FORCEINLINE  osuCrypto::block cc_or_si128(const osuCrypto::block& rhs)const
		{
			auto ret = get<std::uint64_t>();
			auto rhsa = rhs.get<std::uint64_t>();
			ret[0] |= rhsa[0];
			ret[1] |= rhsa[1];
			return ret;
		}

		OC_FORCEINLINE  osuCrypto::block& operator|=(const osuCrypto::block& rhs)
		{
			*this = *this | rhs;
			return *this;
		}


		OC_FORCEINLINE  osuCrypto::block operator<<(const std::uint8_t& rhs)const
		{
#ifdef OC_ENABLE_SSE2
			return mm_slli_epi64(rhs);
#else
			return cc_slli_epi64(rhs);
#endif
		}

		OC_FORCEINLINE  osuCrypto::block& operator<<=(const std::uint8_t& rhs)
		{
			*this = *this << rhs;
			return *this;
		}

#ifdef OC_ENABLE_SSE2
		OC_FORCEINLINE  osuCrypto::block mm_slli_epi64(const std::uint8_t& rhs)const
		{
			return _mm_slli_epi64(*this, rhs);
		}
#endif
		OC_FORCEINLINE  osuCrypto::block cc_slli_epi64(const std::uint8_t& rhs)const
		{
			auto ret = get<std::uint64_t>();
			ret[0] <<= rhs;
			ret[1] <<= rhs;
			return ret;
		}

		OC_FORCEINLINE  block operator>>(const std::uint8_t& rhs)const
		{
#ifdef OC_ENABLE_SSE2
			return mm_srli_epi64(rhs);
#else
			return cc_srli_epi64(rhs);
#endif
		}

		OC_FORCEINLINE  osuCrypto::block& operator>>=(const std::uint8_t& rhs)
		{
			*this = *this >> rhs;
			return *this;
		}

#ifdef OC_ENABLE_SSE2
		OC_FORCEINLINE  block mm_srli_epi64(const std::uint8_t& rhs) const
		{
			return _mm_srli_epi64(*this, rhs);
		}
#endif
		OC_FORCEINLINE  block cc_srli_epi64(const std::uint8_t& rhs) const
		{
			auto ret = get<std::uint64_t>();
			ret[0] >>= rhs;
			ret[1] >>= rhs;
			return ret;;
		}


		OC_FORCEINLINE  osuCrypto::block operator+(const osuCrypto::block& rhs)const
		{
#ifdef OC_ENABLE_SSE2
			return mm_add_epi64(rhs);
#else
			return cc_add_epi64(rhs);
#endif
		}

		OC_FORCEINLINE  osuCrypto::block& operator+=(const osuCrypto::block& rhs)
		{
			*this = *this + rhs;
			return *this;
		}

#ifdef OC_ENABLE_SSE2
		OC_FORCEINLINE  block mm_add_epi64(const osuCrypto::block& rhs) const
		{
			return _mm_add_epi64(*this, rhs);

		}
#endif
		OC_FORCEINLINE  block cc_add_epi64(const osuCrypto::block& rhs) const
		{
			auto ret = get<std::uint64_t>();
			auto rhsa = rhs.get<std::uint64_t>();
			ret[0] += rhsa[0];
			ret[1] += rhsa[1];
			return ret;
		}


		OC_FORCEINLINE  osuCrypto::block operator-(const osuCrypto::block& rhs)const
		{
#ifdef OC_ENABLE_SSE2
			return mm_sub_epi64(rhs);
#else
			return cc_sub_epi64(rhs);
#endif
		}

		OC_FORCEINLINE  osuCrypto::block& operator-=(const osuCrypto::block& rhs)
		{
			*this = *this - rhs;
			return *this;
		}

#ifdef OC_ENABLE_SSE2
		OC_FORCEINLINE  block mm_sub_epi64(const osuCrypto::block& rhs) const
		{
			return _mm_sub_epi64(*this, rhs);

		}
#endif
		OC_FORCEINLINE  block cc_sub_epi64(const osuCrypto::block& rhs) const
		{
			auto ret = get<std::uint64_t>();
			auto rhsa = rhs.get<std::uint64_t>();
			ret[0] -= rhsa[0];
			ret[1] -= rhsa[1];
			return ret;
		}

		OC_FORCEINLINE  block& cmov(const osuCrypto::block& rhs, bool cond);

		// Same, but expects cond to be either 0x00 or 0xff.
		OC_FORCEINLINE  block& cmovBytes(const osuCrypto::block& rhs, uint8_t cond);

		OC_FORCEINLINE  bool operator==(const osuCrypto::block& rhs) const
		{
#ifdef OC_ENABLE_SSE2
			auto neq = _mm_xor_si128(*this, rhs);
			return _mm_test_all_zeros(neq, neq) != 0;
#else
			return get<std::uint64_t>() == rhs.get<std::uint64_t>();
#endif
		}

		OC_FORCEINLINE  bool operator!=(const osuCrypto::block& rhs)const
		{
			return !(*this == rhs);
		}


		OC_FORCEINLINE  bool operator<(const osuCrypto::block& rhs)const
		{
			auto lhsa = get<std::uint64_t>();
			auto rhsa = rhs.get<std::uint64_t>();
			return lhsa[1] < rhsa[1] || (lhsa[1] == rhsa[1] && lhsa[0] < rhsa[0]);
		}

		OC_FORCEINLINE  bool operator>(const block& rhs) const
		{
			return rhs < *this;
		}

		OC_FORCEINLINE  bool operator<=(const block& rhs) const
		{
			return !(*this > rhs);
		}

		OC_FORCEINLINE  bool operator>=(const block& rhs) const
		{
			return !(*this < rhs);
		}



		OC_FORCEINLINE  block srai_epi16(int imm8) const
		{
#ifdef OC_ENABLE_SSE2
			return mm_srai_epi16(imm8);
#else
			return cc_srai_epi16(imm8);
#endif
		}

#ifdef OC_ENABLE_SSE2
		OC_FORCEINLINE  block mm_srai_epi16(char imm8) const
		{
			return _mm_srai_epi16(*this, imm8);
		}
#endif
		OC_FORCEINLINE  block cc_srai_epi16(char imm8) const
		{
			auto v = get<std::int16_t>();
			std::array<std::int16_t, 8> r;
			if (imm8 <= 15)
			{
				r[0] = v[0] >> imm8;
				r[1] = v[1] >> imm8;
				r[2] = v[2] >> imm8;
				r[3] = v[3] >> imm8;
				r[4] = v[4] >> imm8;
				r[5] = v[5] >> imm8;
				r[6] = v[6] >> imm8;
				r[7] = v[7] >> imm8;
			}
			else
			{

				r[0] = v[0] ? 0xFFFF : 0;
				r[1] = v[1] ? 0xFFFF : 0;
				r[2] = v[2] ? 0xFFFF : 0;
				r[3] = v[3] ? 0xFFFF : 0;
				r[4] = v[4] ? 0xFFFF : 0;
				r[5] = v[5] ? 0xFFFF : 0;
				r[6] = v[6] ? 0xFFFF : 0;
				r[7] = v[7] ? 0xFFFF : 0;
			}
			return r;

		}


		OC_FORCEINLINE  int movemask_epi8() const
		{
#ifdef OC_ENABLE_SSE2
			return mm_movemask_epi8();
#else
			return cc_movemask_epi8();
#endif
		}

#ifdef OC_ENABLE_SSE2
		OC_FORCEINLINE  int mm_movemask_epi8() const
		{
			return _mm_movemask_epi8(*this);
		}
#endif

		OC_FORCEINLINE  int cc_movemask_epi8() const
		{
			int ret{ 0 };
			auto v = get<unsigned char>();
			int j = 0;
			for (int i = 7; i >= 0; --i)
				ret |= std::uint16_t(v[j++] & 128) >> i;

			for (size_t i = 1; i <= 8; i++)
				ret |= std::uint16_t(v[j++] & 128) << i;

			return ret;
		}

		OC_FORCEINLINE  int testc(const block& b) const
		{
#ifdef OC_ENABLE_SSE2
			return mm_testc_si128(b);
#else
			return cc_testc_si128(b);
#endif
		}

		OC_FORCEINLINE  int cc_testc_si128(const block& rhs) const
		{
			auto lhsa = get<std::uint64_t>();
			auto rhsa = rhs.get<std::uint64_t>();
			auto v0 = ~lhsa[0] & rhsa[0];
			auto v1 = ~lhsa[1] & rhsa[1];
			return (v0 || v1) ? 0 : 1;
		}

#ifdef OC_ENABLE_SSE2
		OC_FORCEINLINE  int mm_testc_si128(const block& b) const
		{
			return _mm_testc_si128(*this, b);
		}
#endif

		OC_FORCEINLINE  void gf128Mul(const block& y, block& xy1, block& xy2) const
		{
#ifdef OC_ENABLE_PCLMUL
			mm_gf128Mul(y, xy1, xy2);
#else
			cc_gf128Mul(y, xy1, xy2);
#endif // !OC_ENABLE_PCLMUL
		}

		OC_FORCEINLINE  block gf128Mul(const block& y) const
		{
			block xy1, xy2;
#ifdef OC_ENABLE_PCLMUL
			mm_gf128Mul(y, xy1, xy2);
#else
			cc_gf128Mul(y, xy1, xy2);
#endif // !OC_ENABLE_PCLMUL

			return xy1.gf128Reduce(xy2);
		}

		OC_FORCEINLINE  block gf128Pow(std::uint64_t i) const
		{
			if (*this == block(0, 0))
				return block(0, 0);

			block pow2 = *this;
			block s = block(0, 1);
			while (i)
			{
				if (i & 1)
				{
					//s = 1 * i_0 * x^{2^{1}} * ... * i_j x^{2^{j+1}}
					s = s.gf128Mul(pow2);
				}

				// pow2 = x^{2^{j+1}}
				pow2 = pow2.gf128Mul(pow2);
				i >>= 1;
			}

			return s;
		}


#ifdef OC_ENABLE_PCLMUL
		OC_FORCEINLINE  void mm_gf128Mul(const block& y, block& xy1, block& xy2) const
		{
			auto& x = *this;

			block t1 = _mm_clmulepi64_si128(x, y, (int)0x00);
			block t2 = _mm_clmulepi64_si128(x, y, 0x10);
			block t3 = _mm_clmulepi64_si128(x, y, 0x01);
			block t4 = _mm_clmulepi64_si128(x, y, 0x11);
			t2 = (t2 ^ t3);
			t3 = _mm_slli_si128(t2, 8);
			t2 = _mm_srli_si128(t2, 8);
			t1 = (t1 ^ t3);
			t4 = (t4 ^ t2);

			xy1 = t1;
			xy2 = t4;
		}
#endif
		OC_FORCEINLINE  void cc_gf128Mul(const block& y, block& xy1, block& xy2) const
		{
			static const constexpr std::uint64_t mod = 0b10000111;
			auto shifted = get<uint64_t>();
			auto ya = y.get<uint64_t>();
			std::array<uint64_t, 2> result0, result1;

			result0[0] = 0;
			result0[1] = 0;
			result1[0] = 0;
			result1[1] = 0;

			for (int64_t i = 0; i < 2; ++i) {
				for (int64_t j = 0; j < 64; ++j) {
					if (ya[i] & (1ull << j)) {
						result0[0] ^= shifted[0];
						result0[1] ^= shifted[1];
					}

					if (shifted[1] & (1ull << 63)) {
						shifted[1] = (shifted[1] << 1) | (shifted[0] >> 63);
						shifted[0] = (shifted[0] << 1) ^ mod;
					}
					else {
						shifted[1] = (shifted[1] << 1) | (shifted[0] >> 63);
						shifted[0] = shifted[0] << 1;
					}
				}
			}

			xy1 = result0;
			xy2 = result1;
		}


		block gf128Reduce(const block& x1) const
		{
#ifdef OC_ENABLE_PCLMUL
			return mm_gf128Reduce(x1);
#else
			return cc_gf128Reduce(x1);
#endif
		}


		block cc_gf128Reduce(const block& x1) const;

#ifdef OC_ENABLE_PCLMUL
		block mm_gf128Reduce(const block& x1) const
		{
			auto mul256_low = *this;
			auto mul256_high = x1;
			static const constexpr std::uint64_t mod = 0b10000111;

			/* reduce w.r.t. high half of mul256_high */
			const __m128i modulus = _mm_loadl_epi64((const __m128i*) & (mod));
			__m128i tmp = _mm_clmulepi64_si128(mul256_high, modulus, 0x01);
			mul256_low = _mm_xor_si128(mul256_low, _mm_slli_si128(tmp, 8));
			mul256_high = _mm_xor_si128(mul256_high, _mm_srli_si128(tmp, 8));

			/* reduce w.r.t. low half of mul256_high */
			tmp = _mm_clmulepi64_si128(mul256_high, modulus, 0x00);
			mul256_low = _mm_xor_si128(mul256_low, tmp);

			//std::cout << "redu " << bits(x, 128) << std::endl;
			//std::cout << "     " << bits(mul256_low, 128) << std::endl;

			return mul256_low;
		}
#endif

#ifdef OC_ENABLE_SSE2
		template<int imm8>
		OC_FORCEINLINE  block mm_clmulepi64_si128(const block b) const
		{
			return _mm_clmulepi64_si128(*this, b, imm8);
		}
#endif

		template<int imm8>
		OC_FORCEINLINE  block cc_clmulepi64_si128(const block b) const
		{

			std::array<uint64_t,2> shifted, result0;

			auto x = get<uint64_t>()[imm8 & 1];
			auto y = b.get<uint64_t>()[(imm8 >> 4) & 1];
			result0[0] = x * (y & 1);
			result0[1] = 0;

			for (int64_t j = 1; j < 64; ++j) {
				auto bit = (y >> j) & 1ull;

				shifted[0] = x << j;
				shifted[1] = x >> (64 - j);

				result0[0] ^= shifted[0] * bit;
				result0[1] ^= shifted[1] * bit;
			}

			return result0;
		}



		template<int imm8>
		OC_FORCEINLINE  block clmulepi64_si128(block b) const
		{
#ifdef OC_ENABLE_SSE2
			return mm_clmulepi64_si128<imm8>(b);
#else
			return cc_clmulepi64_si128<imm8>(b);
#endif
		}


		OC_FORCEINLINE  block cc_unpacklo_epi64(block b) const
		{
			return std::array<uint64_t, 2>{get<uint64_t>()[0], b.get<uint64_t>()[0]};
		}

#ifdef OC_ENABLE_SSE2
		OC_FORCEINLINE  block mm_unpacklo_epi64(block b) const
		{
			return _mm_unpacklo_epi64(*this, b);
		}
#endif

		OC_FORCEINLINE  block unpacklo_epi64(block b) const
		{
#ifdef OC_ENABLE_SSE2
			return mm_unpacklo_epi64(b);
#else
			return cc_unpacklo_epi64(b);
#endif
		}


		template<int imm8>
		OC_FORCEINLINE  block cc_srli_si128() const
		{
			static_assert(imm8 < 16, "");
			block r;
			memcpy(r.data(), data() + imm8, 16 - imm8);

			if (imm8)
				memset(r.data() + 16 - imm8, 0, imm8);
			return  r;
		}

#ifdef OC_ENABLE_SSE2
		template<int imm8>
		OC_FORCEINLINE  block mm_srli_si128() const
		{
			static_assert(imm8 < 16, "");
			return _mm_srli_si128(*this, imm8);
		}
#endif


		template<int imm8>
		OC_FORCEINLINE  block srli_si128() const
		{
#ifdef OC_ENABLE_SSE2
			return mm_srli_si128<imm8>();
#else
			return cc_srli_si128<imm8>();
#endif
		}




		template<int imm8>
		OC_FORCEINLINE  block cc_slli_si128() const
		{
			static_assert(imm8 < 16, "");
			block r;
			memcpy(r.data() + imm8, data(), 16 - imm8);

			if (imm8)
				memset(r.data(), 0, imm8);

			return  r;
		}

#ifdef OC_ENABLE_SSE2
		template<int imm8>
		OC_FORCEINLINE  block mm_slli_si128() const
		{
			static_assert(imm8 < 16, "");
			return _mm_slli_si128(*this, imm8);
		}
#endif


		template<int imm8>
		OC_FORCEINLINE  block slli_si128() const
		{
#ifdef OC_ENABLE_SSE2
			return mm_slli_si128<imm8>();
#else
			return cc_slli_si128<imm8>();
#endif
		}




		template<int imm8>
		OC_FORCEINLINE  block cc_shuffle_epi32() const
		{
			auto xx = get<uint32_t>();
			std::array<uint32_t,4> rr;
			rr[0] = xx[(imm8 >> 0) & 3];
			rr[1] = xx[(imm8 >> 2) & 3];
			rr[2] = xx[(imm8 >> 4) & 3];
			rr[3] = xx[(imm8 >> 6) & 3];
			return rr;
		}

#ifdef OC_ENABLE_SSE2
		template<int imm8>
		OC_FORCEINLINE  block mm_shuffle_epi32() const
		{
			return _mm_shuffle_epi32(*this, imm8);
		}
#endif

		template<int imm8>
		OC_FORCEINLINE  block shuffle_epi32() const
		{
#ifdef OC_ENABLE_SSE2
			return mm_shuffle_epi32<imm8>();
#else
			return cc_shuffle_epi32<imm8>();
#endif
		}





		template<int imm8>
		OC_FORCEINLINE  block cc_srai_epi32() const
		{
			auto r = get<int32_t>();
			if (imm8 > 31)
			{
				r[0] = r[0] >> 31;
				r[1] = r[1] >> 31;
				r[2] = r[2] >> 31;
				r[3] = r[3] >> 31;
			}
			else
			{
				r[0] >>= imm8;
				r[1] >>= imm8;
				r[2] >>= imm8;
				r[3] >>= imm8;
			}

			return  r;
		}

#ifdef OC_ENABLE_SSE2
		template<int imm8>
		OC_FORCEINLINE  block mm_srai_epi32() const
		{
			return _mm_srai_epi32(*this, imm8);
		}
#endif


		template<int imm8>
		OC_FORCEINLINE  block srai_epi32() const
		{
#ifdef OC_ENABLE_SSE2
			return mm_srai_epi32<imm8>();
#else
			return cc_srai_epi32<imm8>();
#endif
		}





		template<int imm8>
		OC_FORCEINLINE  block cc_slli_epi32() const
		{
			auto r = get<int32_t>();
			if (imm8 > 31)
			{
				r[0] = 0;
				r[1] = 0;
				r[2] = 0;
				r[3] = 0;
			}
			else
			{
				r[0] <<= imm8;
				r[1] <<= imm8;
				r[2] <<= imm8;
				r[3] <<= imm8;
			}

			return  r;
		}

#ifdef OC_ENABLE_SSE2
		template<int imm8>
		OC_FORCEINLINE  block mm_slli_epi32() const
		{
			return _mm_slli_epi32(*this, imm8);
		}
#endif


		template<int imm8>
		OC_FORCEINLINE  block slli_epi32() const
		{
#ifdef OC_ENABLE_SSE2
			return mm_slli_epi32<imm8>();
#else
			return cc_slli_epi32<imm8>();
#endif
		}

	};

#ifdef OC_ENABLE_SSE2
	template<>
	OC_FORCEINLINE  block block::allSame<uint8_t>(uint8_t val)
	{
		return _mm_set1_epi8(val);
	}

	template<>
	OC_FORCEINLINE  block block::allSame<int8_t>(int8_t val)
	{
		return _mm_set1_epi8(val);
	}

	template<>
	OC_FORCEINLINE  block block::allSame<uint16_t>(uint16_t val)
	{
		return _mm_set1_epi16(val);
	}

	template<>
	OC_FORCEINLINE  block block::allSame<int16_t>(int16_t val)
	{
		return _mm_set1_epi16(val);
	}

	template<>
	OC_FORCEINLINE  block block::allSame<uint32_t>(uint32_t val)
	{
		return _mm_set1_epi32(val);
	}

	template<>
	OC_FORCEINLINE  block block::allSame<int32_t>(int32_t val)
	{
		return _mm_set1_epi32(val);
	}

	template<>
	OC_FORCEINLINE  block block::allSame<uint64_t>(uint64_t val)
	{
		return _mm_set1_epi64x(val);
	}

	template<>
	OC_FORCEINLINE  block block::allSame<int64_t>(int64_t val)
	{
		return _mm_set1_epi64x(val);
	}
#endif

	// Specialize to send bool to all bits.
	template<>
	OC_FORCEINLINE  block block::allSame<bool>(bool val)
	{
		return block::allSame<uint64_t>(-(int64_t)val);
	}

	static_assert(sizeof(block) == 16, "expected block size");
	static_assert(std::alignment_of<block>::value == 16, "expected block alignment");
	static_assert(std::is_trivial<block>::value, "expected block trivial");
	static_assert(std::is_standard_layout<block>::value, "expected block pod");
	//#define _SILENCE_ALL_CXX20_DEPRECATION_WARNINGS
		//static_assert(std::is_pod<block>::value, "expected block pod");

	OC_FORCEINLINE  block toBlock(std::uint64_t high_u64, std::uint64_t low_u64)
	{
		return block(high_u64, low_u64);
	}
	OC_FORCEINLINE  block toBlock(std::uint64_t low_u64) { return toBlock(0, low_u64); }
	OC_FORCEINLINE  block toBlock(const std::uint8_t* data) { return toBlock(((std::uint64_t*)data)[1], ((std::uint64_t*)data)[0]); }

	OC_FORCEINLINE  block& block::cmov(const osuCrypto::block& rhs, bool cond)
	{
		return *this ^= allSame(cond) & (*this ^ rhs);
	}

	OC_FORCEINLINE  block& block::cmovBytes(const osuCrypto::block& rhs, uint8_t cond)
	{
		return *this ^= allSame(cond) & (*this ^ rhs);
	}

	OC_FORCEINLINE  void cswap(block& x, block& y, bool cond)
	{
		block diff = block::allSame(cond) & (x ^ y);
		x ^= diff;
		y ^= diff;
	}

	OC_FORCEINLINE  void cswapBytes(block& x, block& y, uint8_t cond)
	{
		block diff = block::allSame(cond) & (x ^ y);
		x ^= diff;
		y ^= diff;
	}

	extern const block ZeroBlock;
	extern const block OneBlock;
	extern const block AllOneBlock;
	extern const block CCBlock;
	extern const std::array<block, 2> zeroAndAllOne;
}

std::ostream& operator<<(std::ostream& out, const osuCrypto::block& block);
namespace osuCrypto
{
	using ::operator<<;
}

OC_FORCEINLINE  bool eq(const osuCrypto::block& lhs, const osuCrypto::block& rhs)
{
	return lhs == rhs;
}

OC_FORCEINLINE  bool neq(const osuCrypto::block& lhs, const osuCrypto::block& rhs)
{
	return lhs != rhs;
}



namespace std {

	template <>
	struct hash<osuCrypto::block>
	{
		std::size_t operator()(const osuCrypto::block& k) const;
	};

}

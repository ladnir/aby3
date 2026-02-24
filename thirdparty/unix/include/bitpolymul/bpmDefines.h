#pragma once

#include <stdint.h>
#include <cinttypes>


#include <immintrin.h>
#include <mmintrin.h>
#include <xmmintrin.h>
#ifdef _MSC_VER
#include <intrin.h>
#include <malloc.h>
#endif

//#define (x) 
namespace bpm
{

	using u64 = std::uint64_t;
	using u32 = std::uint32_t;
	using u16 = std::uint16_t;
	using u8 = std::uint8_t;
	using i64 = std::int64_t;
	using i32 = std::int32_t;
	using i16 = std::int16_t;
	using i8 = std::int8_t;


#ifdef _MSC_VER
	inline int __builtin_ctz(uint32_t x) {
		unsigned long ret;
		_BitScanForward(&ret, x);
		return (int)ret;
	}

	inline int __builtin_ctzll(unsigned long long x) {
		unsigned long ret;
		_BitScanForward64(&ret, x);
		return (int)ret;
	}



	inline int __builtin_ctzl(unsigned long x) { return sizeof(x) == 8 ? __builtin_ctzll(x) : __builtin_ctz((uint32_t)x); }
	inline int __builtin_clz(uint32_t x) { return (int)__lzcnt(x); }
	inline int __builtin_clzll(unsigned long long x) { return (int)__lzcnt64(x); }
	inline int __builtin_clzl(unsigned long x) { return sizeof(x) == 8 ? __builtin_clzll(x) : __builtin_clz((uint32_t)x); }

	inline int __builtin_ctzl(unsigned long long x) { return __builtin_ctzll(x); }
	inline int __builtin_clzl(unsigned long long x) { return __builtin_clzll(x); }

#endif


#define xor128(v1,v2) _mm_xor_si128(v1,v2)
#define xor256(v1,v2) _mm256_xor_si256(v1,v2)
#define and128(v1,v2) _mm_and_si128(v1,v2)
#define and256(v1,v2) _mm256_and_si256(v1,v2)
#define or128(v1,v2) _mm_or_si128(v1,v2)
#define or256(v1,v2) _mm256_or_si256(v1,v2)


#define cache_prefetch(d, p) _mm_prefetch((const char*)d, p)


}
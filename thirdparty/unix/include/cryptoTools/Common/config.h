#pragma once


// use the miracl library for curves
/* #undef ENABLE_MIRACL */

// use the relic library for curves
/* #undef ENABLE_RELIC */

// use the libsodium library for curves
/* #undef ENABLE_SODIUM */

// does the libsodium library support noclamp operations on Montgomery curves?
/* #undef SODIUM_MONTGOMERY */

// compile the circuit library
#define ENABLE_CIRCUITS ON

// include the span-lite
#define ENABLE_SPAN_LITE ON

// defined if we should use cpp 14 and undefined means cpp 11
/* #undef ENABLE_CPP_14 */

// Turn on Channel logging for debugging.
/* #undef ENABLE_NET_LOG */


// enable the wolf ssl socket layer.
/* #undef ENABLE_WOLFSSL */

// enable integration with boost for networking.
#define ENABLE_BOOST ON

// enable the use of intel SSE instructions.
#define ENABLE_SSE ON

// enable the use of intel AVX instructions.
#define ENABLE_AVX ON

// enable the use of the portable AES implementation.
/* #undef ENABLE_PORTABLE_AES */

#if (defined(_MSC_VER) || defined(__SSE2__)) && defined(ENABLE_SSE)
#define ENABLE_SSE_BLAKE2 ON
#define OC_ENABLE_SSE2 ON
#endif

#if (defined(_MSC_VER) || defined(__PCLMUL__)) && defined(ENABLE_SSE)
#define OC_ENABLE_PCLMUL
#endif

#if (defined(_MSC_VER) || defined(__AES__)) && defined(ENABLE_SSE)
#define OC_ENABLE_AESNI ON
#else
#define OC_ENABLE_PORTABLE_AES ON
#endif

#if (defined(_MSC_VER) || defined(__AVX2__)) && defined(ENABLE_AVX)
#define OC_ENABLE_AVX2 ON
#endif

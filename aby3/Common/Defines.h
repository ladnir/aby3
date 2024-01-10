#pragma once
#include <cryptoTools/Common/Defines.h>
#include <cryptoTools/Common/Version.h>

#if CRYPTO_TOOLS_VERSION < 10601
static_assert(0, "update cryptoTools / libOTe");
#endif

#ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0501
#endif // !_WIN32_WINNT

namespace aby3
{
    using u64 = oc::u64;
    using i64 = oc::i64;
    using u32 = oc::u32;
    using i32 = oc::i32;
    using u16 = oc::u16;
    using i16 = oc::i16;
    using u8 = oc::u8;
    using i8 = oc::i8;
    using block = oc::block;

    template<typename T> using span = oc::span<T>;
}

#define ABY3_STRINGIZE_DETAIL(x) #x
#define ABY3_STRINGIZE(x) ABY3_STRINGIZE_DETAIL(x)
#define ABY3_LOCATION __FILE__ ":" ABY3_STRINGIZE(__LINE__)
#define ABY3_RTE std::runtime_error(ABY3_LOCATION)

#ifdef _MSC_VER
#define ABY3_FUNCTION __FUNCSIG__
#elif defined(__GNUC__) || defined(__clang__)
#define ABY3_FUNCTION __PRETTY_FUNCTION__ 
#else
#define ABY3_FUNCTION __func__
#endif

#if !defined( NDEBUG ) || defined(ABY3_ENABLE_ASSERTS)
#define ABY3_ASSERT(X)       \
    do{                         \
        if(!(X))                \
        {                       \
            std::cout << "Assertion `" << ABY3_STRINGIZE(X) << "` failed.\n"   \
                << "  src location: " << ABY3_LOCATION << "\n"               \
                << "  function: " << ABY3_FUNCTION << std::endl;             \
            std::terminate();                                   \
        }                       \
    }while(0)

#define ABY3_ASSERT_MSG(X, M)    \
    do{                         \
        if(!(X))                \
        {                       \
            std::cout << "Assertion `" << ABY3_STRINGIZE(X) << "` failed.\n"   \
                << "  src location: " << ABY3_LOCATION << "\n"               \
                << "  function: " << ABY3_FUNCTION << "\n"                   \
                << "  message: " << M << std::endl;             \
            std::terminate();                                   \
        }                       \
    }while(0)



#else
#define ABY3_ASSERT(expression) ((void)0)
#define ABY3_ASSERT_MSG(expression, M) ((void)0)
#endif

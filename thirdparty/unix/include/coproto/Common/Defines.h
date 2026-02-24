#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#include "coproto/config.h"
#include <cstdint>
#include <exception>
#include <cassert>
#include <iostream>

#define COPRO_STRINGIZE_DETAIL(x) #x
#define COPRO_STRINGIZE(x) COPRO_STRINGIZE_DETAIL(x)
#define COPROTO_LOCATION __FILE__ ":" COPRO_STRINGIZE(__LINE__)
#define COPROTO_RTE std::runtime_error(COPROTO_LOCATION)

#ifdef _MSC_VER
#define COPROTO_FUNCTION __FUNCSIG__
#elif defined(__GNUC__) || defined(__clang__)
#define COPROTO_FUNCTION __PRETTY_FUNCTION__ 
#else
#define COPROTO_FUNCTION __func__
#endif

#if !defined( NDEBUG ) || defined(COPROTO_ENABLE_ASSERTS)
#define COPROTO_ASSERT(X)       \
    do{                         \
        if(!(X))                \
        {                       \
            std::cout << "Assertion `" << COPRO_STRINGIZE(X) << "` failed.\n"   \
                << "  src location: " << COPROTO_LOCATION << "\n"               \
                << "  function: " << COPROTO_FUNCTION << std::endl;             \
            std::terminate();                                   \
        }                       \
    }while(0)

#define COPROTO_ASSERT_MSG(X, M)    \
    do{                         \
        if(!(X))                \
        {                       \
            std::cout << "Assertion `" << COPRO_STRINGIZE(X) << "` failed.\n"   \
                << "  src location: " << COPROTO_LOCATION << "\n"               \
                << "  function: " << COPROTO_FUNCTION << "\n"                   \
                << "  message: " << M << std::endl;             \
            std::terminate();                                   \
        }                       \
    }while(0)



#else
#define COPROTO_ASSERT(expression) ((void)0)
#define COPROTO_ASSERT_MSG(expression, M) ((void)0)
#endif


namespace coproto
{

#ifdef ALLOC_TEST
    void regNew_(void* ptr, std::string name);
    void regDel_(void* ptr);
    std::string regStr();
#define COPROTO_REG_NEW(p, n) regNew_(p,n)
#define COPROTO_REG_DEL(p) regDel_(p)
    extern u64 mNewIdx;
#else
#define COPROTO_REG_NEW(p, n) 
#define COPROTO_REG_DEL(p)
#endif

    typedef uint64_t u64;
    typedef int64_t i64;
    typedef uint32_t u32;
    typedef int32_t i32;
    typedef uint16_t u16;
    typedef int16_t i16;
    typedef uint8_t u8;
    typedef int8_t i8;

    template<typename T>
    class ProtoV;

    namespace internal
    {
        const int inlineSize = 256;
    }


    template<typename T>
    T copy(const T& t) { return t; }
}
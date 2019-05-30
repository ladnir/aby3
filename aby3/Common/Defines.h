#pragma once
#include <cryptoTools/Common/Defines.h>
#define _WIN32_WINNT 0x0501
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
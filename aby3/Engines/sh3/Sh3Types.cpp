#include "Sh3Types.h"
namespace aby3
{
    namespace Sh3
    {


    }
}

void aby3::Sh3::details::trimImpl(oc::MatrixView<u8> a, u64 bits)
{
    if (bits > a.stride() * 8)
        throw std::runtime_error(LOCATION);

    auto mod8 = bits & 7;
    auto div8 = bits >> 3;
    u8 mask = mod8 ? ((1 << mod8) - 1) : ~0;

    for (u64 j = 0; j < a.rows(); ++j)
    {
        auto i = div8;
        a(j, i) &= mask;
        ++i;
        for (; i < a.stride(); ++i)
            a(j, i) = 0;
    }
}

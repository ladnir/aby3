#include "Sh3Types.h"

bool aby3::Sh3::details::areEqualImpl(
    const std::array<oc::MatrixView<u8>, 2>& a,
    const std::array<oc::MatrixView<u8>, 2>& b,
    u64 bitCount)
{
    auto mod8 = bitCount & 7;
    auto div8 = bitCount >> 3;
    u8 mask = mod8 ? ((1 << mod8) - 1) : ~0;

    auto stride = (bitCount + 7) / 8;
    if (stride > a[0].stride())
        throw RTE_LOC;

    for (u64 i = 0; i < a[0].rows(); ++i)
    {

        if (div8)
        {
            auto c1 = memcmp(a[0][i].data(), b[0][i].data(), div8);
            auto c2 = memcmp(a[1][i].data(), b[1][i].data(), div8);

            if(c1 || c2)
                return false;
        }

        if (mask & (
                (a[0](i, div8) ^ b[0](i, div8)) |
                (a[1](i, div8) ^ b[1](i, div8))
            ))
            return false;
    }
    return true;
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

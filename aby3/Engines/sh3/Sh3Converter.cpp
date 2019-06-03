#include "Sh3Converter.h"
#include <libOTe/Tools/Tools.h>
using namespace oc;

namespace aby3
{



    void Sh3Converter::toPackedBin(const sbMatrix & in, sPackedBin & dest)
    {
        dest.reset(in.rows(), in.bitCount());

        for (u64 i = 0; i < 2; ++i)
        {
            auto& s = in.mShares[i];
            auto& d = dest.mShares[i];

            MatrixView<u8> inView(
                (u8*)(s.data()),
                (u8*)(s.data() + s.size()),
                sizeof(i64) * s.cols());
            
            MatrixView<u8> memView(
                (u8*)(d.data()),
                (u8*)(d.data() + d.size()),
                sizeof(i64) * d.cols());

            sse_transpose(inView, memView);
        }
    }

    void Sh3Converter::toBinaryMatrix(const sPackedBin & in, sbMatrix & dest)
    {
        dest.resize(in.shareCount(), in.bitCount());

        for (u64 i = 0; i < 2; ++i)
        {
            auto& s = in.mShares[i];
            auto& d = dest.mShares[i];

            MatrixView<u8> inView(
                (u8*)(s.data()),
                (u8*)(s.data() + s.size()),
                sizeof(i64) * s.cols());

            MatrixView<u8> memView(
                (u8*)(d.data()),
                (u8*)(d.data() + d.size()),
                sizeof(i64) * d.cols());

            sse_transpose(inView, memView);
        }
    }

}
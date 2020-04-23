#pragma once
#include "aby3/Common/Defines.h"
#include "aby3/sh3/Sh3Types.h"
#include <string>
#include "cryptoTools/Crypto/PRNG.h"
#include "aby3/sh3/Sh3Runtime.h"

namespace aby3
{


    std::string prettyShare(u64 partyIdx, const si64& v);
    void rand(i64Matrix& A, oc::PRNG& prng);

    void run(Sh3Task t0, Sh3Task t1, Sh3Task t2);

    std::string bitstr(const i64Matrix& x, u64 w);


    void share(const i64Matrix& x, u64 colBitCount, sbMatrix& x0, sbMatrix& x1, sbMatrix& x2, oc::PRNG& prng);
    int memcmp(oc::MatrixView<i64> l, oc::MatrixView<i64> r);
    void reveal(i64Matrix& x, sbMatrix& x0, sbMatrix& x1, sbMatrix& x2);
    void reveal(i64Matrix& x, si64Matrix& x0, si64Matrix& x1, si64Matrix& x2);

}
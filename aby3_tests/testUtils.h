#pragma once
#include "aby3/Common/Defines.h"
#include "aby3/sh3/Sh3Types.h"
#include <string>
#include "cryptoTools/Crypto/PRNG.h"
#include "aby3/sh3/Sh3Runtime.h"
#include "cryptoTools/Circuit/BetaCircuit.h"
#include "cryptoTools/Network/IOService.h"
#include "aby3/sh3/Sh3Encryptor.h"
#include "aby3/sh3/Sh3ShareGen.h"
#include "aby3/sh3/Sh3Converter.h"
namespace aby3
{


    std::string prettyShare(u64 partyIdx, const si64& v);
    void rand(i64Matrix& A, oc::PRNG& prng);

    void run(std::function<void()> fn0,
        std::function<void()> fn1,
        std::function<void()> fn2);
    void run(Sh3Task t0, Sh3Task t1, Sh3Task t2);

    std::string bitstr(const i64Matrix& x, u64 w);


    i64Matrix pack(const i64Matrix& x);
    i64Matrix unpack(const i64Matrix& x, u64 bitCount);

    void evaluate(oc::BetaCircuit cir, std::vector<i64Matrix> input, std::vector<i64Matrix*> y, bool sparse = true);

    void share(const i64Matrix& x, u64 colBitCount, sbMatrix& x0, sbMatrix& x1, sbMatrix& x2, oc::PRNG& prng);
    void share(const i64Matrix& x, si64Matrix& x0, si64Matrix& x1, si64Matrix& x2, oc::PRNG& prng);
    int memcmp(oc::MatrixView<i64> l, oc::MatrixView<i64> r);
    void reveal(i64Matrix& x, sbMatrix& x0, sbMatrix& x1, sbMatrix& x2);
    void reveal(i64Matrix& x, si64Matrix& x0, si64Matrix& x1, si64Matrix& x2);


    std::array<CommPkg, 3> makeComms(oc::IOService& ios);
    void makeRuntimes(std::array<Sh3Runtime, 3>&  rt, oc::IOService& ios);
    void makeRuntimes(std::array<Sh3Runtime, 3>& rt, std::array<CommPkg, 3>& comm);
    void resize(span<sbMatrix> m, u64 x, u64 y);

    std::array<Sh3Encryptor, 3> makeEncryptors();
    std::array<Sh3ShareGen, 3> makeShareGens();
    std::array<Sh3Converter, 3> makeConverters(std::array<Sh3Runtime,3>& rts, std::array<Sh3ShareGen, 3>& gens);

}
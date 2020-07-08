#include "aby3/sh3/Sh3Encryptor.h"
#include "aby3/sh3/Sh3BinaryEvaluator.h"
#include "aby3/Circuit/CircuitLibrary.h"
#include "cryptoTools/Network/IOService.h"
#include "cryptoTools/Common/Log.h"
#include <random>
#include "cryptoTools/Crypto/PRNG.h"

using namespace oc;
using namespace aby3;

//
//void createBinarySharing(
//    PRNG& prng,
//    Eigen::Matrix<i64, Eigen::Dynamic, Eigen::Dynamic>& value,
//    Lynx::Engine::Matrix& s0,
//    Lynx::Engine::Matrix& s1,
//    Lynx::Engine::Matrix& s2)
//{
//    s0.mShares[0] = value;
//
//    s1.mShares[0].resizeLike(value);
//    s2.mShares[0].resizeLike(value);
//
//    //if (zeroshare) {
//    //	s1.mShares[0].setZero();
//    //	s2.mShares[0].setZero();
//    //}
//    //else 
//    {
//        prng.get(s1.mShares[0].data(), s1.mShares[0].size());
//        prng.get(s2.mShares[0].data(), s2.mShares[0].size());
//
//        for (u64 i = 0; i < s0.mShares[0].size(); ++i)
//            s0.mShares[0](i) ^= s1.mShares[0](i) ^ s2.mShares[0](i);
//    }
//
//    s0.mShares[1] = s2.mShares[0];
//    s1.mShares[1] = s0.mShares[0];
//    s2.mShares[1] = s1.mShares[0];
//}

//Lynx::Matrix toLynx(const sbMatrix& A)
//{
//    Lynx::Matrix r(A.rows(), A.i64Cols());
//    r.mShares[0] = A.mShares[0];
//    r.mShares[1] = A.mShares[1];
//    return r;
//}
//
//void compare(Sh3BinaryEvaluator& a, Lynx::BinaryEngine& b)
//{
//    if (a.mMem.bitCount() != b.mMem.bitCount())
//        throw std::runtime_error(LOCATION);
//
//    if (a.mMem.shareCount() != b.mMem.shareCount())
//        throw std::runtime_error(LOCATION);
//
//
//    if (neq(a.hashState(), b.hashState()))
//        ostreamLock(std::cout)
//            << "Ha " << a.hashState() << std::endl
//            << "Hb " << b.hashState() << std::endl;
//
//    for (u64 s = 0; s < 2; ++s)
//    {
//
//        for (u64 i = 0; i < a.mMem.bitCount(); ++i)
//        {
//            for (u64 j = 0; j < a.mMem.simdWidth(); ++j)
//            {
//                if (neq(a.mMem.mShares[s](i, j),b.mMem.mShares[s](i, j)))
//                {
//                    ostreamLock(std::cout) << " share " << s << " bit " << i << " word " << j << std::endl;
//                }
//            }
//        }
//    }
//
//    if (a.mPlainWires_DEBUG.size() != b.mPlainWires_DEBUG.size())
//        throw std::runtime_error(LOCATION);
//
//    for (u64 s = 0; s < 3; ++s)
//    {
//        for (u64 i = 0; i < a.mPlainWires_DEBUG.size(); ++i)
//        {
//
//            if (a.mPlainWires_DEBUG[i].size() != b.mPlainWires_DEBUG[i].size())
//                throw std::runtime_error(LOCATION);
//
//            for (u64 j = 0; j < a.mPlainWires_DEBUG[i].size(); ++j)
//            {
//                if (a.mPlainWires_DEBUG[i][j].mBits[s] !=
//                    b.mPlainWires_DEBUG[i][j].mBits[s])
//                {
//                    ostreamLock(std::cout) << "*share " << s << " bit " << i << " word " << j << std::endl;
//                }
//            }
//        }
//    }
//}

std::array<oc::Matrix<i64>, 3> getShares(sbMatrix& S, Sh3Runtime& rt, CommPkg& comm)
{
    std::array<oc::Matrix<i64>, 3> r;

    auto& r0 = r[rt.mPartyIdx];
    auto& r1 = r[(rt.mPartyIdx + 1) % 3];
    auto& r2 = r[(rt.mPartyIdx + 2) % 3];

    r0 = S.mShares[0];
    r1 = S.mShares[1];

    comm.mNext.asyncSend(r0.data(), r0.size());
    comm.mPrev.asyncSend(r1.data(), r1.size());

    r2.resize(S.rows(), S.i64Cols());

    comm.mNext.recv(r2.data(), r2.size());
    oc::Matrix<i64> check(r1.rows(), r1.cols());
    comm.mPrev.recv(check.data(), check.size());

    if (memcmp(check.data(), r1.data(), check.size() * sizeof(i64)) != 0)
        throw RTE_LOC;

    return r;

}

void Sh3_BinaryEngine_test(
    BetaCircuit* cir,
    std::function<i64(i64, i64)> binOp,
    bool debug,
    std::string opName,
    u64 valMask = ~0ull)
{

    IOService ios;
    Session s01(ios, "127.0.0.1", SessionMode::Server, "01");
    Session s10(ios, "127.0.0.1", SessionMode::Client, "01");
    Session s02(ios, "127.0.0.1", SessionMode::Server, "02");
    Session s20(ios, "127.0.0.1", SessionMode::Client, "02");
    Session s12(ios, "127.0.0.1", SessionMode::Server, "12");
    Session s21(ios, "127.0.0.1", SessionMode::Client, "12");

    Channel chl01 = s01.addChannel("c");
    Channel chl10 = s10.addChannel("c");
    Channel chl02 = s02.addChannel("c");
    Channel chl20 = s20.addChannel("c");
    Channel chl12 = s12.addChannel("c");
    Channel chl21 = s21.addChannel("c");


    CommPkg comms[3], debugComm[3];
    comms[0] = { chl02, chl01 };
    comms[1] = { chl10, chl12 };
    comms[2] = { chl21, chl20 };

    debugComm[0] = { comms[0].mPrev.getSession().addChannel(), comms[0].mNext.getSession().addChannel() };
    debugComm[1] = { comms[1].mPrev.getSession().addChannel(), comms[1].mNext.getSession().addChannel() };
    debugComm[2] = { comms[2].mPrev.getSession().addChannel(), comms[2].mNext.getSession().addChannel() };

    cir->levelByAndDepth();
    u64 width = 1 << 8;
    bool failed = false;
    //bool manual = false;

    enum Mode { Manual, Auto, Replicated };

    std::array < std::vector<oc::Matrix<i64>>, 3> CC;
    std::array < std::vector<oc::Matrix<i64>>, 3> CC2;
    std::array<std::atomic<int>, 3> ac;
    Sh3BinaryEvaluator evals[3];

    //block tag = oc::sysRandomSeed();

    ac[0] = 0;
    ac[1] = 0;
    ac[2] = 0;
    auto aSize = cir->mInputs[0].size();
    auto bSize = cir->mInputs[1].size();
    auto cSize = cir->mOutputs[0].size();

    auto routine = [&](int pIdx) {
        //auto i = 0;
        i64Matrix a(width, 1), b(width, 1), c(width, 1), ar(1, 1);
        i64Matrix aa(width, 1), bb(width, 1);

        PRNG prng(ZeroBlock);
        for (u64 i = 0; i < a.size(); ++i)
        {
            a(i) = prng.get<i64>() & valMask;
            b(i) = prng.get<i64>() & valMask;
        }
        ar(0) = prng.get<i64 >();

        Sh3Runtime rt(pIdx, comms[pIdx]);

        sbMatrix A(width, aSize), B(width, bSize), C(width, cSize);
        sbMatrix Ar(1, aSize);

        Sh3Encryptor enc;
        enc.init(pIdx, toBlock(pIdx), toBlock((pIdx + 1) % 3));

        auto task = rt.noDependencies();

        enc.localBinMatrix(rt.noDependencies(), a, A).get();
        enc.localBinMatrix(rt.noDependencies(), ar, Ar).get();
        enc.localBinMatrix(rt.noDependencies(), b, B).get();

        auto& eval = evals[pIdx];

#ifdef BINARY_ENGINE_DEBUG
        if (debug)
            eval.enableDebug(pIdx, debugComm[pIdx].mPrev, debugComm[pIdx].mNext);
#endif

        for (auto mode : { Manual, Auto, Replicated })
        {
            eval.init(toBlock(pIdx), toBlock((pIdx + 1) % 3));
            //if (pIdx == 0)
            //    oc::lout << "---------------------------------------" << std::endl;

            C.mShares[0](0) = 0;
            C.mShares[1](0) = 0;
            switch (mode)
            {
            case Manual:
                task.get();
                eval.setCir(cir, width);
                eval.setInput(0, A);
                eval.setInput(1, B);
                eval.asyncEvaluate(rt.noDependencies()).get();
                eval.getOutput(0, C);
                break;
            case Auto:
                task = eval.asyncEvaluate(task, cir, { &A, &B }, { &C });
                break;
            case Replicated:
                for (u64 i = 0; i < width; ++i)
                    a(i) = ar(0);

                task.get();
                eval.setCir(cir, width);
                eval.setReplicatedInput(0, Ar);
                eval.setInput(1, B);
                eval.asyncEvaluate(rt.noDependencies()).get();
                eval.getOutput(0, C);
                break;

            }
            task.get();

            auto ccc0 = C.mShares[0](0);
            CC[pIdx].push_back(C.mShares[0]);
            CC2[pIdx].push_back(C.mShares[1]);
            ++ac[pIdx];
            auto ccc1 = C.mShares[0](0);
            auto ccc2 = CC[pIdx].back()(0);

            if (pIdx)
            {

                enc.reveal(task, 0, C).get();

                //auto CC = getShares(C, rt, debugComm[pIdx]);

            }
            else
            {
                enc.reveal(task, C, c).get();

                auto ci = CC[pIdx].size() - 1;;


                for (u64 j = 0; j < width; ++j)
                {
                    if (c(j) != binOp(a(j), b(j)))
                    {
                        oc::lout << "pidx: " << rt.mPartyIdx << " mode: " << (int)mode << " debug:" << int(debug) << " failed at " << j << " " << std::hex << c(j) << " != " << std::hex << binOp(a(j), b(j))
                            << " = " << opName << "(" << std::hex << a(j) << ", " << std::hex << b(j) << ")" << std::endl << std::dec;
                        failed = true;
                    }
                }

                if (a(0) == 0x66 && failed)
                {
                    while (ac[1] < ac[0]);
                    while (ac[2] < ac[0]);

                    auto& oo = oc::lout;
                    oo << "pidx: " << rt.mPartyIdx << " check\n";
                    oo << "      a " << A.mShares[0](0) << " " << A.mShares[1](0) << std::endl;
                    oo << "      b " << B.mShares[0](0) << " " << B.mShares[1](0) << std::endl;
                    oo << "      c " << C.mShares[0](0) << " " << C.mShares[1](0) << std::endl;
                    oo << "      C " << CC[0][ci](0) << " " << CC[1][ci](0) << " " << CC[2][ci](0) << std::endl;
                    oo << "      D " << CC2[0][ci](0) << " " << CC2[1][ci](0) << " " << CC2[2][ci](0) << std::endl;
                    oo << "        " << ccc0 << " " << ccc1 << " " << ccc2 << std::endl;
                    oo << "   recv " << eval.mRecvFutr.size() << std::endl;
                    //oo << "   " << eval.mLog.str() << std::endl;

                    //oo << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << std::endl;
                    //oo << evals[1].mLog.str() << std::endl;
                    //oo << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << std::endl;
                    //oo << evals[2].mLog.str() << std::endl;
                }
            }

            //oc::lout << oc::Color::Green << eval.mLog.str() << oc::Color::Default << std::endl;
        }
    };

    auto t0 = std::thread(routine, 0);
    auto t1 = std::thread(routine, 1);
    auto t2 = std::thread(routine, 2);

    t0.join();
    t1.join();
    t2.join();

    if (failed)
        throw std::runtime_error(LOCATION);
}

void Sh3_BinaryEngine_and_test()
{

    BetaLibrary lib;
    u64 size = 8;
    u64 mask = ~((~0ull) << size);
    // and
    {
        auto cir = lib.int_int_bitwiseAnd(size, size, size);
        cir->levelByAndDepth();

        Sh3_BinaryEngine_test(cir, [](i64 a, i64 b) {return a & b; }, true, "AND", mask);
        Sh3_BinaryEngine_test(cir, [](i64 a, i64 b) {return a & b; }, false, "AND", mask);
    }

    // na_and
    {
        auto* cd = new BetaCircuit;

        BetaBundle a(size);
        BetaBundle b(size);
        BetaBundle c(size);

        cd->addInputBundle(a);
        cd->addInputBundle(b);

        cd->addOutputBundle(c);

        //int_int_bitwiseAnd_build(*cd, a, b, c);
        for (u64 j = 0; j < c.mWires.size(); ++j)
        {
            cd->addGate(
                a.mWires[j],
                b.mWires[j],
                GateType::na_And,
                c.mWires[j]);
        }


        Sh3_BinaryEngine_test(cd, [](i64 a, i64 b) {
            return ~a & b;
            }, false, "na_AND", mask);

    }

}


void Sh3_BinaryEngine_add_test()
{

    BetaLibrary lib;
    u64 size = 8;
    u64 mask = ~((~0ull) << size);
    auto cir = lib.int_int_add(size, size, size, BetaLibrary::Optimized::Depth);

    Sh3_BinaryEngine_test(cir, [mask](i64 a, i64 b) {return (a + b) & mask; }, true, "Plus", mask);
    Sh3_BinaryEngine_test(cir, [mask](i64 a, i64 b) {return (a + b) & mask; }, false, "Plus", mask);

}


void Sh3_BinaryEngine_add_msb_test()
{
    BetaLibrary lib;
    u64 size = 8;
    u64 mask = ~((~0ull) << size);
    auto cir = lib.int_int_add_msb(size);

    Sh3_BinaryEngine_test(cir, [size](i64 a, i64 b) {return ((a + b) >> (size - 1)) & 1; }, true, "msb", mask);
    Sh3_BinaryEngine_test(cir, [size](i64 a, i64 b) {return ((a + b) >> (size - 1)) & 1; }, false, "msb", mask);
}

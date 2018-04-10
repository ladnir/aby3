#include "Sh3EncryptorTests.h"
#include "aby3/Engines/sh3/Sh3Encryptor.h"
#include <cryptoTools/Network/Channel.h>
#include <cryptoTools/Network/IOService.h>
#include <cryptoTools/Crypto/PRNG.h>
#include <cryptoTools/Common/BitVector.h>

using namespace aby3;
using namespace oc;
using namespace aby3::Sh3;

i64 extract(const Sh3::sPackedBin& A, u64 share, u64 packIdx, u64 wordIdx)
{

    i64 v = 0;
    BitIterator iter((u8*)&v, 0);

    u64 bitIdx = 64 * wordIdx;
    u64 offset = packIdx / 64;
    u64 mask = 1 << (packIdx % 64);
    for (u64 i = 0; i < 64; ++i)
    {
        *iter = A.mShares[share](bitIdx, offset) & mask;
        ++bitIdx;
        ++iter;
    }

    return v;
}

void Sh3_Encryptor_Integer_test()
{

    IOService ios;
    auto chl01 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "01").addChannel();
    auto chl10 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "01").addChannel();
    auto chl02 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "02").addChannel();
    auto chl20 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "02").addChannel();
    auto chl12 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "12").addChannel();
    auto chl21 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "12").addChannel();


    int trials = 10;
    CommPkg comm[3];
    comm[0] = { chl02, chl01 };
    comm[1] = { chl10, chl12 };
    comm[2] = { chl21, chl20 };

    Sh3Encryptor enc[3];
    enc[0].init(0, toBlock(0, 0), toBlock(1, 1));
    enc[1].init(1, toBlock(1, 1), toBlock(2, 2));
    enc[2].init(2, toBlock(2, 2), toBlock(0, 0));

    bool failed = false;
    auto t0 = std::thread([&]() {
        auto i = 0;
        auto& e = enc[i];
        auto& c = comm[i];
        PRNG prng(ZeroBlock);

        i64 s = 0, x = 0;
        std::vector<i64> vals(trials);
        std::vector<si64> shrs(trials);
        std::vector<sb64> bshrs(trials);
        si64 sum{ { 0,0 } };
        sb64 XOR{ {0,0} };

        for (int i = 0; i < trials; ++i)
        {
            vals[i] = i * 325143121;
            shrs[i] = e.localInt(c, vals[i]);
            bshrs[i] = e.localBinary(c, vals[i]);
            sum = sum + shrs[i];
            XOR = XOR ^ bshrs[i];
            s += vals[i];
            x ^= vals[i];
        }

        for (int i = 0; i < trials; ++i)
        {
            std::array<i64, 2> v;
            v[0] = e.reveal(c, shrs[i]);
            v[1] = e.reveal(c, bshrs[i]);
            for (u64 j = 0; j < v.size(); ++j)
            {
                if (v[j] != vals[i]) {
                    std::cout << "localInt[" << i << ", "<<j<<"] " << v[j] << " " << vals[i] << " failed" << std::endl;
                    failed = true;
                }
            }
        }

        auto v1 = e.reveal(c, sum);
        auto v2 = e.reveal(c, XOR);
        if (v1 != s)
        {
            std::cout << "sum" << std::endl;
            failed = true;
        }

        if (v2 != x)
        {
            std::cout << "xor" << std::endl;
            failed = true;
        }


        Sh3::i64Matrix m(trials, trials), mm(trials, trials);
        for (u64 i = 0; i < m.size(); ++i)
            m(i) = i;

        Sh3::si64Matrix mShr(trials, trials);
        e.localIntMatrix(c, m, mShr);
        e.reveal(c, mShr, mm);

        if (mm != m)
        {
            std::cout << "int mtx" << std::endl;
            failed = true;
        }

        Sh3::sb64Matrix bShr(trials, trials);
        e.localBinMatrix(c, m, bShr);
        e.reveal(c, bShr, mm);

        if (mm != m)
        {
            std::cout << "bin mtx" << std::endl;
            failed = true;
        }

        //m.setZero();

        Sh3::sPackedBin pShr(trials, trials * sizeof(i64) * 8);// , pShr1(trials, trials * sizeof(i64) * 8), pShr2(trials, trials * sizeof(i64) * 8);
        e.localPackedBinary(c, m, pShr);
        e.reveal(c, pShr, mm);

        //c.mNext.recv(pShr1.mShares[0].data(), pShr1.mShares[0].size());
        //c.mNext.recv(pShr1.mShares[1].data(), pShr1.mShares[1].size());
        //c.mPrev.recv(pShr2.mShares[0].data(), pShr2.mShares[0].size());
        //c.mPrev.recv(pShr2.mShares[1].data(), pShr2.mShares[1].size());

        //pShr = pShr ^ pShr1;
        //pShr = pShr ^ pShr2;

        if (mm != m)
        {
            std::cout << "pac mtx" << std::endl;
            std::cout << m << std::endl;
            std::cout << mm << std::endl;


            //for (u64 i = 0; i < 64; ++i)
            //{
            //    auto v = extract(pShr, 0, i, 0);
            //    std::cout<< "s["<<i<<"] " << std::hex << v << std::endl;

            //}

            failed = true;
        }

    });


    auto rr = [&](int i)
    {
        auto& e = enc[i];
        auto& c = comm[i];
        std::vector<si64> shrs(trials);
        std::vector<sb64> bshrs(trials);
        si64 sum{ { 0,0 } };
        sb64 XOR{ { 0,0 } };

        for (int i = 0; i < trials; ++i)
        {
            shrs[i] = e.remoteInt(c);
            bshrs[i] = e.remoteBinary(c);
            sum = sum + shrs[i];
            XOR = XOR ^ bshrs[i];
        }

        for (int i = 0; i < trials; ++i)
        {
            e.reveal(c, shrs[i], 0);
            e.reveal(c, bshrs[i], 0);
        }
        e.reveal(c, sum, 0);
        e.reveal(c, XOR, 0);




        Sh3::si64Matrix mShr(trials, trials);
        e.remoteIntMatrix(c, mShr);
        e.reveal(c, mShr, 0);

        Sh3::sb64Matrix bShr(trials, trials);
        e.remoteBinMatrix(c, bShr);
        e.reveal(c, bShr, 0);
        
        Sh3::sPackedBin pShr(trials, trials * sizeof(i64) * 8);
        e.remotePackedBinary(c, pShr);
        e.reveal(c, pShr, 0);

        //Channel chl;
        //if (i == 1)
        //    chl = c.mPrev;
        //else
        //    chl = c.mNext;

        //chl.asyncSend(pShr.mShares[0].data(), pShr.mShares[0].size());
        //chl.asyncSend(pShr.mShares[1].data(), pShr.mShares[1].size());
    };

    auto t1 = std::thread(rr, 1);
    auto t2 = std::thread(rr, 2);

    t0.join();
    t1.join();
    t2.join();

    if (failed)
        throw std::runtime_error(LOCATION);
}

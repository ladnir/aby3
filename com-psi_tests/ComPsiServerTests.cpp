
#include "com-psi/ComPsiServer.h"
#include <cryptoTools/Network/IOService.h>
#include "ComPsiServerTests.h"
#include <unordered_set>
using namespace oc;

void ComPsi_computeKeys_test()
{

    IOService ios;
    Session s01(ios, "127.0.0.1", SessionMode::Server, "01");
    Session s10(ios, "127.0.0.1", SessionMode::Client, "01");
    Session s02(ios, "127.0.0.1", SessionMode::Server, "02");
    Session s20(ios, "127.0.0.1", SessionMode::Client, "02");
    Session s12(ios, "127.0.0.1", SessionMode::Server, "12");
    Session s21(ios, "127.0.0.1", SessionMode::Client, "12");


    ComPsiServer srvs[3];
    srvs[0].init(0, s02, s01);
    srvs[1].init(1, s10, s12);
    srvs[2].init(2, s21, s20);

    auto size =  10;
    Table a, b;
    a.mKeys.resize(size, (srvs[0].mKeyBitCount + 63) / 64);
    b.mKeys.resize(size, (srvs[0].mKeyBitCount + 63) / 64);

    for (u64 i = 0; i < size; ++i)
    {
        for (u64 j = 0; j < a.mKeys.cols(); ++j)
        {
            a.mKeys(i, j) = i >> 1;
            b.mKeys(i, j) = a.mKeys(i, j);
        }
    }

    aby3::Sh3::i64Matrix r0, r1;
    auto t0 = std::thread([&]() {
        auto i = 0;
        setThreadName("t_" + ToString(i));
        auto A = srvs[i].localInput(a);
        auto B = srvs[i].localInput(b);

        std::vector<SharedTable*> tables{ &A , &B};
        std::vector<u64> reveals{ 0, 1};
        r0 = srvs[i].computeKeys(tables, reveals);


        for (u64 i = 0; i < size - 1; i += 2)
        {
            for (u64 j = 0; j < a.mKeys.cols(); ++j)
            {
                //std::cout << (j ? ", " : ToString(i) + " : ") << r(i, j);
                if (r0(i, j) != r0(i + 1, j))
                {
                    throw std::runtime_error(LOCATION);
                }
            }
            //std::cout << std::endl;
        }

    });


    auto routine = [&](int i)
    {
        setThreadName("t_" + ToString(i));

        auto A = srvs[i].remoteInput(0, size);
        auto B = srvs[i].remoteInput(0, size);
        std::vector<SharedTable*> tables{ &A, &B };
        std::vector<u64> reveals{ 0 , 1};
        auto r = srvs[i].computeKeys(tables, reveals);

        if (i == 1)
        {
            r1 = std::move(r);
        }
    };

    auto t1 = std::thread(routine, 1);
    auto t2 = std::thread(routine, 2);

    t0.join();
    t1.join();
    t2.join();

    if (r0 != r1)
    {
        std::cout << r0 << std::endl << std::endl;
        std::cout << r1 << std::endl;

        throw std::runtime_error("");
    }

}

void ComPsi_cuckooHash_test()
{

    IOService ios;
    Session s01(ios, "127.0.0.1", SessionMode::Server, "01");
    Session s10(ios, "127.0.0.1", SessionMode::Client, "01");
    Session s02(ios, "127.0.0.1", SessionMode::Server, "02");
    Session s20(ios, "127.0.0.1", SessionMode::Client, "02");
    Session s12(ios, "127.0.0.1", SessionMode::Server, "12");
    Session s21(ios, "127.0.0.1", SessionMode::Client, "12");


    ComPsiServer srvs[3];
    srvs[0].init(0, s02, s01);
    srvs[1].init(1, s10, s12);
    srvs[2].init(2, s21, s20);


    // 80 bits;
    u32 hashSize = 80;
    u32 rows = 100;
    u32 bytes = 8;

    PRNG prng(OneBlock);
    aby3::Sh3::i64Matrix hashs(rows, (hashSize + 63) / 64);
    hashs.setZero();

    Table a;
    a.mKeys.resize(rows, (srvs[0].mKeyBitCount + 63) / 64);

    for (u64 i = 0; i < rows; ++i)
    {
        for (u64 j = 0; j < a.mKeys.cols(); ++j)
        {
            a.mKeys(i, j) = i;
        }

        prng.get((u8*)hashs.row(i).data(), hashSize / 8);
    }



    auto t0 = std::thread([&]() {
        auto i = 0;

        setThreadName("t_" + ToString(i));
        auto A = srvs[i].localInput(a);

        srvs[i].cuckooHash(A, hashs);
    });


    auto t1 = std::thread([&]() {
        auto i = 1;
        setThreadName("t_" + ToString(i));
        auto A = srvs[i].remoteInput(0, rows);
        srvs[i].cuckooHashRecv(A);
    });

    auto t2 = std::thread([&]() {
        auto i = 2;
        setThreadName("t_" + ToString(i));
        auto A = srvs[i].remoteInput(0, rows);
        srvs[i].cuckooHashSend(A);
    });


    t0.join();
    t1.join();
    t2.join();


}

void ComPsi_Intersect_test()
{

    IOService ios;
    Session s01(ios, "127.0.0.1", SessionMode::Server, "01");
    Session s10(ios, "127.0.0.1", SessionMode::Client, "01");
    Session s02(ios, "127.0.0.1", SessionMode::Server, "02");
    Session s20(ios, "127.0.0.1", SessionMode::Client, "02");
    Session s12(ios, "127.0.0.1", SessionMode::Server, "12");
    Session s21(ios, "127.0.0.1", SessionMode::Client, "12");


    ComPsiServer srvs[3];
    srvs[0].init(0, s02, s01);
    srvs[1].init(1, s10, s12);
    srvs[2].init(2, s21, s20);


    // 80 bits;
    u32 hashSize = 80;
    u32 rows = 10;

    PRNG prng(ZeroBlock);
    Table a, b;
    a.mKeys.resize(rows, (srvs[0].mKeyBitCount + 63) / 64);
    b.mKeys.resize(rows, (srvs[0].mKeyBitCount + 63) / 64);
    auto intersectionSize = (rows +1)/ 2;

    std::unordered_set<i64> map;
    map.reserve(intersectionSize);

    for (u64 i = 0; i < rows; ++i)
    {
        auto out = (i >= intersectionSize);
        for (u64 j = 0; j < a.mKeys.cols(); ++j)
        {
            a.mKeys(i, j) = i + 1;
            b.mKeys(i, j) = i + 1 + (rows * out);
        }

        std::cout << "a[" << i << "] = " << a.mKeys(i, 0) << std::endl;
        std::cout << "b[" << i << "] = " << b.mKeys(i, 0) << std::endl;

        if (!out)
            map.emplace(a.mKeys(i, 0));
    }


    bool failed = false;
    auto t0 = std::thread([&]() {
        setThreadName("t0");
        auto i = 0;
        auto A = srvs[i].localInput(a);
        auto B = srvs[i].localInput(b);

        auto C = srvs[i].intersect(A, B);

        aby3::Sh3::i64Matrix c(C.mKeys.rows(), C.mKeys.i64Cols());
        srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mKeys, c);

        if (c.rows() != intersectionSize)
        {
            failed = true;
            std::cout << "bad size, exp: " << intersectionSize << ", act: " << c.rows() << std::endl;
        }


        for (u64 i = 0; i < c.rows(); ++i)
        {
           auto iter = map.find(c(i, 0));
           if (iter == map.end())
           {
               failed = true;
               std::cout << "bad value: " << c(i, 0) << std::endl;
           }

           std::cout << "c[" << i << "] = " << c(i, 0) << std::endl;

        }


    });

    auto t1 = std::thread([&]() {
        setThreadName("t1");
        auto i = 1;
        auto A = srvs[i].remoteInput(0, rows);
        auto B = srvs[i].remoteInput(0, rows);

        auto C = srvs[i].intersect(A, B);
        aby3::Sh3::i64Matrix c(C.mKeys.rows(), C.mKeys.i64Cols());
        srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mKeys, c);
    });
    auto t2 = std::thread([&]() {
        setThreadName("t2");
        auto i = 2;
        auto A = srvs[i].remoteInput(0, rows);
        auto B = srvs[i].remoteInput(0, rows);

        auto C = srvs[i].intersect(A, B);
        aby3::Sh3::i64Matrix c(C.mKeys.rows(), C.mKeys.i64Cols());
        srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mKeys, c);
    });



    t0.join();
    t1.join();
    t2.join();

    if (failed)
        throw std::runtime_error("");

    //srvs[0].intersect(A, B);
}


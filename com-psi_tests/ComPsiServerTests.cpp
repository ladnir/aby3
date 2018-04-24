
#include "com-psi/ComPsiServer.h"
#include <cryptoTools/Network/IOService.h>
#include "ComPsiServerTests.h"

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

    auto size = 1 << 10;
    Table a;
    a.mKeys.resize(size, (srvs[0].mKeyBitCount + 63) / 64);

    for (u64 i = 0; i < size; ++i)
    {
        for (u64 j = 0; j < a.mKeys.cols(); ++j)
        {
            a.mKeys(i, j) = i >> 1;
        }
    }

    auto t0 = std::thread([&]() {
        auto i = 0;
        setThreadName("t_" + ToString(i));
        auto A = srvs[i].localInput(a);

        std::vector<SharedTable*> tables{ &A };
        std::vector<u64> reveals{ 0 };
        auto r = srvs[i].computeKeys(tables, reveals);


        for (u64 i = 0; i < size - 1; i += 2)
        {
            for (u64 j = 0; j < a.mKeys.cols(); ++j)
            {
                //std::cout << (j ? ", " : ToString(i) + " : ") << r(i, j);
                if (r(i, j) != r(i + 1, j))
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
        std::vector<SharedTable*> tables{ &A };
        std::vector<u64> reveals{ 0 };
        auto r = srvs[i].computeKeys(tables, reveals);
    };

    auto t1 = std::thread(routine, 1);
    auto t2 = std::thread(routine, 2);

    t0.join();
    t1.join();
    t2.join();
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
    u32 rows = 3;

    PRNG prng(ZeroBlock);
    Table a, b;
    a.mKeys.resize(rows, (srvs[0].mKeyBitCount + 63) / 64);
    b.mKeys.resize(rows, (srvs[0].mKeyBitCount + 63) / 64);

    for (u64 i = 0; i < rows; ++i)
    {
        for (u64 j = 0; j < a.mKeys.cols(); ++j)
        {
            a.mKeys(i, j) = i;
            b.mKeys(i, j) = i;
        }
    }



    auto t0 = std::thread([&]() {
        setThreadName("t0");
        auto i = 0;
        auto A = srvs[i].localInput(a);
        auto B = srvs[i].localInput(b);

        srvs[i].intersect(A, B);

    });
    auto t1 = std::thread([&]() {
        setThreadName("t1");
        auto i = 1;
        auto A = srvs[i].remoteInput(0, rows);
        auto B = srvs[i].remoteInput(0, rows);

        srvs[i].intersect(A, B);
    });
    auto t2 = std::thread([&]() {
        setThreadName("t2");
        auto i = 2;
        auto A = srvs[i].remoteInput(0, rows);
        auto B = srvs[i].remoteInput(0, rows);

        srvs[i].intersect(A, B);
    });



    t0.join();
    t1.join();
    t2.join();

    //srvs[0].intersect(A, B);
}


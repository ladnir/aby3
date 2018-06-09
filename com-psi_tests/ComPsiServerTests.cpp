
#include "com-psi/ComPsiServer.h"
#include <cryptoTools/Network/IOService.h>
#include "ComPsiServerTests.h"
#include <unordered_set>
using namespace oc;

namespace osuCrypto
{
    extern int ComPsiServer_ssp;
}

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

    auto size = 1046;
    auto keyBitCount = srvs[0].mKeyBitCount;
    Table a(size, { ColumnInfo{ "key", TypeID::IntID, keyBitCount } })
        , b(size, { ColumnInfo{ "key", TypeID::IntID, keyBitCount } });

    for (u64 i = 0; i < size; ++i)
    {
        for (u64 j = 0; j < a.mColumns[0].cols(); ++j)
        {
            a.mColumns[0](i, j) = i >> 1;
            b.mColumns[0](i, j) = a.mColumns[0](i, j);
        }
    }
    bool failed = false;

    aby3::Sh3::i64Matrix r0, r1;
    auto t0 = std::thread([&]() {
        auto i = 0;
        setThreadName("t_" + ToString(i));
        auto A = srvs[i].localInput(a);
        auto B = srvs[i].localInput(b);

        std::vector<SharedTable::ColRef> tables{ A["key"], B["key"] };
        std::vector<u64> reveals{ 0, 1 };
        r0 = srvs[i].computeKeys(tables, reveals);


        for (u64 i = 0; i < size - 1; i += 2)
        {
            for (u64 j = 0; j < a.mColumns[0].cols(); ++j)
            {
                //std::cout << (j ? ", " : ToString(i) + " : ") << r(i, j);
                if (r0(i, j) != r0(i + 1, j))
                {
                    std::cout << i << std::endl;
                    //throw std::runtime_error(LOCATION);
                    failed = true;
                }
            }
            //std::cout << std::endl;
        }

    });


    auto routine = [&](int i)
    {
        setThreadName("t_" + ToString(i));

        auto A = srvs[i].remoteInput(0);
        auto B = srvs[i].remoteInput(0);
        std::vector<SharedTable::ColRef> tables{ A["key"], B["key"] };
        std::vector<u64> reveals{ 0 , 1 };
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
        std::cout << std::hex << std::endl;
        std::cout << r0 << std::endl << std::endl;
        std::cout << r1 << std::endl;

        throw std::runtime_error("");
    }

    if (failed)
    {
        std::cout << "failed " << std::endl;
        throw RTE_LOC;
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
    u32 rows = 1 << 10;
    u32 bytes = 8;

    PRNG prng(OneBlock);
    aby3::Sh3::i64Matrix hashs(rows, (hashSize + 63) / 64);
    hashs.setZero();

    auto keyBitCount = srvs[0].mKeyBitCount;
    Table a(rows, { ColumnInfo{ "key", TypeID::IntID, keyBitCount } });

    for (u64 i = 0; i < rows; ++i)
    {
        for (u64 j = 0; j < a.mColumns[0].cols(); ++j)
        {
            a.mColumns[0](i, j) = i;
        }

        prng.get((u8*)hashs.row(i).data(), hashSize / 8);
    }



    auto t0 = std::thread([&]() {
        auto i = 0;

        setThreadName("t_" + ToString(i));
        auto A = srvs[i].localInput(a);
        std::vector<SharedTable::ColRef> select{ A["key"] };

        srvs[i].cuckooHash(select, hashs);
    });


    auto t1 = std::thread([&]() {
        auto i = 1;
        setThreadName("t_" + ToString(i));
        auto A = srvs[i].remoteInput(0);
        srvs[i].cuckooHashRecv(A);
    });

    auto t2 = std::thread([&]() {
        auto i = 2;
        setThreadName("t_" + ToString(i));
        auto A = srvs[i].remoteInput(0);
        auto cuckooParams = CuckooIndex<>::selectParams(rows, ComPsiServer_ssp, 0, 3);
        srvs[i].cuckooHashSend(A, cuckooParams);
    });


    t0.join();
    t1.join();
    t2.join();


}

void ComPsi_compare_test()
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
    auto byteSize = hashSize / 8;
    u32 rows =1 << 12;
    //Table b;
    //b.mColumns[0].resize(rows, (srvs[0].mKeyBitCount + 63) / 64);
    auto keyBitCount = srvs[0].mKeyBitCount;
    Table b(rows, { ColumnInfo{ "key", TypeID::IntID, keyBitCount } });
    b.mColumns[0].setZero();

    std::vector<u8> expIntersection(rows, 0);
    PRNG prng(ZeroBlock);

    for (u64 i = 0; i < rows; ++i)
    {
        expIntersection[i] = prng.get<bool>();
        prng.get((u8*)b.mColumns[0].row(i).data(), byteSize);
    }

    bool failed = false;

    auto routine = [&](int i)
    {
        SharedTable B;

        if (i == 0)
            B = srvs[i].localInput(b);
        else
            B = srvs[i].remoteInput(0);


        aby3::Sh3::PackedBin plainFlags(B.rows(), 1);
        aby3::Sh3::sPackedBin intersectionFlags(B.rows(), 1);

        if (i < 2)
        {
            std::array<MatrixView<u8>, 3> selects;
            std::array<Matrix<u8>, 3> selects2;

            selects2[0].resize(rows, byteSize);
            selects2[1].resize(rows, byteSize);
            selects2[2].resize(rows, byteSize);

            if (i == 0)
            {

                for (u64 j = 0; j < rows; ++j)
                {
                    if (expIntersection[j])
                    {
                        u8 idx = prng.get<u8>() % 3;
                        memcpy(selects2[idx][j].data(), b.mColumns[0].row(j).data(), byteSize);
                    }
                }
            }

            selects[0] = selects2[0];
            selects[1] = selects2[1];
            selects[2] = selects2[2];


            srvs[i].compare(B, selects, intersectionFlags);
            srvs[i].mEnc.revealAll(srvs[i].mRt.noDependencies(), intersectionFlags, plainFlags).get();
            BitIterator iter((u8*)plainFlags.mData.data(), 0);
            for (u64 j = 0; j < rows; ++j)
            {
                if (*iter++ != expIntersection[j])
                {
                    failed = true;
                }
            }
        }
        else
        {
            srvs[i].compare(B, intersectionFlags);
            srvs[i].mEnc.revealAll(srvs[i].mRt.noDependencies(), intersectionFlags, plainFlags).get();
        }
    };


    auto t0 = std::thread(routine, 0);
    auto t1 = std::thread(routine, 1);

    routine(2);
    t0.join();
    t1.join();

    if (failed)
        throw RTE_LOC;
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


    u32 rows = 1 << 14;

    PRNG prng(ZeroBlock);

    auto keyBitCount = srvs[0].mKeyBitCount;
    Table a(rows, { ColumnInfo{ "key", TypeID::IntID, keyBitCount } })
        , b(rows, { ColumnInfo{ "key", TypeID::IntID, keyBitCount } });
    auto intersectionSize = (rows + 1) / 2;

    std::unordered_set<i64> map;
    map.reserve(intersectionSize);

    for (u64 i = 0; i < rows; ++i)
    {
        auto out = (i >= intersectionSize);
        for (u64 j = 0; j < a.mColumns[0].cols(); ++j)
        {
            a.mColumns[0](i, j) = i + 1;
            b.mColumns[0](i, j) = i + 1 + (rows * out);
        }

        //std::cout << "a[" << i << "] = " << a.mColumns[0](i, 0) << std::endl;
        //std::cout << "b[" << i << "] = " << b.mColumns[0](i, 0) << std::endl;

        if (!out)
            map.emplace(a.mColumns[0](i, 0));
    }


    bool failed = false;
    auto t0 = std::thread([&]() {
        setThreadName("t0");
        auto i = 0;
        auto A = srvs[i].localInput(a);
        auto B = srvs[i].localInput(b);

        auto C = srvs[i].intersect(A, B);

        aby3::Sh3::i64Matrix c(C.mColumns[0].rows(), C.mColumns[0].i64Cols());
        srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[0], c);

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
                std::cout << "bad value in intersection: " << c(i, 0) << " @ " << i << std::endl;
            }
            else
            {
                map.erase(iter);
            }

            //std::cout << "c[" << i << "] = " << c(i, 0) << std::endl;
        }

        for (auto& v : map)
        {
            std::cout << "missing idx " << v << std::endl;
        }
    });

    auto t1 = std::thread([&]() {
        setThreadName("t1");
        auto i = 1;
        auto A = srvs[i].remoteInput(0);
        auto B = srvs[i].remoteInput(0);

        auto C = srvs[i].intersect(A, B);
        aby3::Sh3::i64Matrix c(C.mColumns[0].rows(), C.mColumns[0].i64Cols());
        srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[0], c);
    });
    auto t2 = std::thread([&]() {
        setThreadName("t2");
        auto i = 2;
        auto A = srvs[i].remoteInput(0);
        auto B = srvs[i].remoteInput(0);

        auto C = srvs[i].intersect(A, B);
        aby3::Sh3::i64Matrix c(C.mColumns[0].rows(), C.mColumns[0].i64Cols());
        srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[0], c);
    });



    t0.join();
    t1.join();
    t2.join();

    if (failed)
        throw std::runtime_error("");

    //srvs[0].intersect(A, B);
}


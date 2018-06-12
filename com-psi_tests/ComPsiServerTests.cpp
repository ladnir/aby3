
#include "com-psi/ComPsiServer.h"
#include <cryptoTools/Network/IOService.h>
#include "ComPsiServerTests.h"
#include <unordered_set>
#include <iomanip>
using namespace oc;

namespace osuCrypto
{
    extern int ComPsiServer_ssp;
}


std::string hexString(u8* ptr, u32 size)
{
    std::stringstream ss;
    for (u64 i = 0; i < size; ++i)
    {
        ss << std::hex << std::setw(2) << std::setfill('0') << u32(ptr[i]);
    }

    return ss.str();
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
        for (u64 j = 0; j < a.mColumns[0].mData.cols(); ++j)
        {
            a.mColumns[0].mData(i, j) = i >> 1;
            b.mColumns[0].mData(i, j) = a.mColumns[0].mData(i, j);
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
            for (u64 j = 0; j < a.mColumns[0].mData.cols(); ++j)
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
    auto keyBitCount = srvs[0].mKeyBitCount;
    u32 rows = 1 << 10;
    u32 bytes = 8;

    PRNG prng(OneBlock);
    aby3::Sh3::i64Matrix hashs(rows, (keyBitCount + 63) / 64);
    hashs.setZero();

    Table a(rows, {
        ColumnInfo{ "key", TypeID::IntID, keyBitCount } ,
        ColumnInfo{ "data", TypeID::IntID, 64 }
        });

    for (u64 i = 0; i < rows; ++i)
    {
        a.mColumns[0].mData(i, 0) = i;
        a.mColumns[1].mData(i, 0) = i;

        prng.get((u8*)hashs.row(i).data(), keyBitCount / 8);
    }

    oc::Matrix<u8> m1, m2;

    auto t0 = std::thread([&]() {
        auto i = 0;

        setThreadName("t_" + ToString(i));
        auto A = srvs[i].localInput(a);
        std::vector<SharedTable::ColRef> select{ A["key"], A["data"] };

        m1 = srvs[i].cuckooHash(select, hashs);
    });


    auto t1 = std::thread([&]() {
        auto i = 1;
        setThreadName("t_" + ToString(i));
        auto A = srvs[i].remoteInput(0);
        std::vector<SharedTable::ColRef> select{ A["key"] , A["data"] };
        m2 = srvs[i].cuckooHashRecv(select);
    });

    auto t2 = std::thread([&]() {
        auto i = 2;
        setThreadName("t_" + ToString(i));
        auto A = srvs[i].remoteInput(0);
        std::vector<SharedTable::ColRef> select{ A["key"] , A["data"] };
        auto cuckooParams = CuckooIndex<>::selectParams(rows, ComPsiServer_ssp, 0, 3);
        srvs[i].cuckooHashSend(select, cuckooParams);
    });

    t0.join();
    t1.join();
    t2.join();

    for (u64 i = 0; i < m1.size(); ++i)
        m1(i) ^= m2(i);

    if (m1.cols() != 18)
        throw RTE_LOC;

    std::vector<u8> temp(m1.cols());
    span<block> view((block*)hashs.data(), hashs.rows());
    for (u64 i = 0; i < view.size(); ++i)
    {
        auto j = srvs[0].mCuckoo.find(view[i]);

        if (!j)
            throw RTE_LOC;

        ((u64*)&(temp[0]))[0] = i;
        ((u64*)&(temp[10]))[0] = i;

        if (memcmp(m1[j.mCuckooPositon].data(), temp.data(), 18) != 0)
        {
            std::cout << i << "  "
                << hexString((u8*)m1[j.mCuckooPositon].data(), 18) << " vs \n"
                << hexString((u8*)temp.data(), 18) << std::endl;
            throw RTE_LOC;
        }
    }


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
    auto keyBitCount = srvs[0].mKeyBitCount;
    u32 rows =  1 << 12;
    //Table b;
    //b.mColumns[0].resize(rows, (srvs[0].mKeyBitCount + 63) / 64);
    Table b(rows, {
        ColumnInfo{ "key", TypeID::IntID, keyBitCount }
        ,ColumnInfo{ "data", TypeID::IntID, 64 }
        });


    b.mColumns[0].mData.setZero();

    std::vector<u8> expIntersection(rows, 0);
    PRNG prng(OneBlock);

    for (u64 i = 0; i < rows; ++i)
    {
        expIntersection[i] = prng.get<bool>();
        u8* d = (u8*)b.mColumns[0].mData.row(i).data();
        auto size = b.mColumns[0].mData.cols();
        prng.get(d, size);
    }

    bool failed = false;

    auto routine = [&](int i)
    {
        SharedTable B, C;

        if (i == 0)
            B = srvs[i].localInput(b);
        else
            B = srvs[i].remoteInput(0);

        C.mColumns.resize(2);
        C.mColumns[0].mShares   = B.mColumns[0].mShares;
        C.mColumns[0].mName     = B.mColumns[0].mName;
        C.mColumns[0].mType     = B.mColumns[0].mType;
        C.mColumns[0].mBitCount = B.mColumns[0].mBitCount;

        C.mColumns[1].mShares   = B.mColumns[1].mShares;
        C.mColumns[1].mName     = B.mColumns[1].mName;
        C.mColumns[1].mType     = B.mColumns[1].mType;
        C.mColumns[1].mBitCount = B.mColumns[1].mBitCount;

        C.mColumns[0].mShares[0].setZero();
        C.mColumns[0].mShares[1].setZero();
        C.mColumns[1].mShares[0].setZero();
        C.mColumns[1].mShares[1].setZero();

        aby3::Sh3::PackedBin plainFlags(B.rows(), 1);
        aby3::Sh3::sPackedBin intersectionFlags(B.rows(), 1);

        if (i < 2)
        {

            std::vector<SharedTable::ColRef> outCols{ C["key"], C["data"] }; //};

            auto leftJoin = B["key"];
            auto rightJoin = B["key"];

            auto selectBytes = (leftJoin.mCol.getBitCount() + 7) / 8;
            for (auto& c : outCols)
                selectBytes += (c.mCol.getBitCount() + 7) / 8;

            std::array<MatrixView<u8>, 3> selects;
            std::array<Matrix<u8>, 3> selects2;

            selects2[0].resize(rows, selectBytes);
            selects2[1].resize(rows, selectBytes);
            selects2[2].resize(rows, selectBytes);

            if (i == 0)
            {

                for (u64 j = 0; j < rows; ++j)
                {
                    if (expIntersection[j])
                    {
                        u8 idx = prng.get<u8>() % 3;

                        auto dest = selects2[idx][j].data();
                        auto src = b.mColumns[0].mData.row(j).data();
                        auto size = (b.mColumns[0].getBitCount() + 7) / 8;

                        if (selectBytes != 2 * size + ((C["data"].mCol.getBitCount() + 7) / 8))
                            throw RTE_LOC;

                        memcpy(dest, src, size);
                        dest += size;
                        memcpy(dest, src, size);
                        dest += size;
                        *(u64*)dest = j;
                    }
                }
            }

            selects[0] = selects2[0];
            selects[1] = selects2[1];
            selects[2] = selects2[2];


            srvs[i].compare(leftJoin, rightJoin, selects, outCols, intersectionFlags);
            srvs[i].mEnc.revealAll(srvs[i].mRt.noDependencies(), intersectionFlags, plainFlags).get();
            BitIterator iter((u8*)plainFlags.mData.data(), 0);

            auto stride = C.mColumns[0].i64Cols();
            aby3::Sh3::i64Matrix c0(C.rows(), stride), c1(C.rows(), 1);
            srvs[i].mEnc.revealAll(srvs[i].mRt.noDependencies(), C.mColumns[0], c0).get();
            srvs[i].mEnc.revealAll(srvs[i].mRt.noDependencies(), C.mColumns[1], c1).get();

            //ostreamLock o(std::cout);
            for (u64 j = 0; j < rows; ++j)
            {

                //o << "out[" << j << "] = " 
                //    << hexString((u8*)c0.row(j).data(), c0.cols() * sizeof(u64)) << " "
                //    << hexString((u8*)c1.row(j).data(), c1.cols() * sizeof(u64)) << " " << int(*iter) << std::endl;;

                if (*iter != expIntersection[j])
                {
                    failed = true;

                }
                else
                {
                    for (u64 k = 0; k < stride; ++k)
                    {
                        if (c0(j, k) != *iter * b.mColumns[0].mData(j, k))
                            failed = true;
                    }


                    if (c1(j, 0) != *iter * j)
                        failed = true;
                }

                ++iter;
            }
        }
        else
        {
            std::vector<SharedTable::ColRef> outCols{ C["key"], C["data"] };

            auto leftJoin = B["key"];
            auto rightJoin = B["key"];

            srvs[i].compare(leftJoin, rightJoin, outCols, intersectionFlags);
            srvs[i].mEnc.revealAll(srvs[i].mRt.noDependencies(), intersectionFlags, plainFlags).get();

            aby3::Sh3::i64Matrix c(C.rows(), C.mColumns[0].i64Cols()), c1(C.rows(), 1);
            srvs[i].mEnc.revealAll(srvs[i].mRt.noDependencies(), C.mColumns[0], c).get();
            srvs[i].mEnc.revealAll(srvs[i].mRt.noDependencies(), C.mColumns[1], c1).get();

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
        , b(rows, {
        ColumnInfo{ "key", TypeID::IntID, keyBitCount },
        ColumnInfo{ "data", TypeID::IntID, 64}
    });

    auto intersectionSize = (rows + 1) / 2;

    std::unordered_set<i64> map;
    map.reserve(intersectionSize);

    for (u64 i = 0; i < rows; ++i)
    {
        auto out = (i >= intersectionSize);
        for (u64 j = 0; j < a.mColumns[0].mData.cols(); ++j)
        {
            a.mColumns[0].mData(i, j) = i + 1;
            b.mColumns[0].mData(i, j) = i + 1 + (rows * out);
        }

        b.mColumns[1].mData(i, 0) = 2 * i;

        //std::cout << "a[" << i << "] = " << a.mColumns[0](i, 0) << std::endl;
        //std::cout << "b[" << i << "] = " << b.mColumns[0](i, 0) << std::endl;

        if (!out)
            map.emplace(a.mColumns[0].mData(i, 0));
    }


    bool failed = false;
    auto t0 = std::thread([&]() {
        setThreadName("t0");
        auto i = 0;
        auto A = srvs[i].localInput(a);
        auto B = srvs[i].localInput(b);

        auto C = srvs[i].join(
            /* where  */ A["key"],/* = */ B["key"],
            /* select */ { A["key"], B["data"] });

        aby3::Sh3::i64Matrix c(C.mColumns[0].rows(), C.mColumns[0].i64Cols());
        aby3::Sh3::i64Matrix c2(C.mColumns[0].rows(), C.mColumns[1].i64Cols());
        srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[0], c);
        srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[1], c2);

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
                std::cout << "bad key in intersection: " << c(i, 0) << " @ " << i << std::endl;
            }
            else
            {
                if (c2(i, 0) != 2 * i)
                {
                    failed = true;
                    std::cout << "bad label in intersection: " << c2(i, 0) << " @ " << i << std::endl;
                }

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

        auto C = srvs[i].join(A["key"], B["key"], { A["key"], B["data"] });
        aby3::Sh3::i64Matrix c(C.mColumns[0].rows(), C.mColumns[0].i64Cols());
        aby3::Sh3::i64Matrix c2(C.mColumns[0].rows(), C.mColumns[1].i64Cols());
        srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[0], c);
        srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[1], c2);
    });
    auto t2 = std::thread([&]() {
        setThreadName("t2");
        auto i = 2;
        auto A = srvs[i].remoteInput(0);
        auto B = srvs[i].remoteInput(0);

        auto C = srvs[i].join(A["key"], B["key"], { A["key"], B["data"] });
        aby3::Sh3::i64Matrix c(C.mColumns[0].rows(), C.mColumns[0].i64Cols());
        aby3::Sh3::i64Matrix c2(C.mColumns[0].rows(), C.mColumns[1].i64Cols());
        srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[0], c);
        srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[1], c2);
    });



    t0.join();
    t1.join();
    t2.join();

    if (failed)
        throw std::runtime_error("");

    //srvs[0].intersect(A, B);
}


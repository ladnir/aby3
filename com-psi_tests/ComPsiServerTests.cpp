
#include "com-psi/ComPsiServer.h"
#include <cryptoTools/Network/IOService.h>
#include "ComPsiServerTests.h"
#include <unordered_map>
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
    auto cuckooParams = CuckooIndex<>::selectParams(rows, ComPsiServer_ssp, 0, 3);

    for (u64 i = 0; i < rows; ++i)
    {
        //a.mColumns[0].mData(i, 0) = i;
        //a.mColumns[1].mData(i, 0) = i;
        u8* d0 = (u8*)a.mColumns[0].mData.row(i).data();
        u8* d1 = (u8*)a.mColumns[1].mData.row(i).data();
        prng.get(d0, keyBitCount / 8);
        prng.get(d1, 8);
        prng.get((u8*)hashs.row(i).data(), keyBitCount / 8);
    }

    oc::Matrix<u8> m1, m2;

    auto t0 = std::thread([&]() {
        auto i = 0;

        setThreadName("t_" + ToString(i));
        auto A = srvs[i].localInput(a);
        std::vector<SharedTable::ColRef> select{ A["key"], A["data"] };

        m1 = srvs[i].cuckooHash(select, cuckooParams, hashs);
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
        srvs[i].cuckooHashSend(select, cuckooParams);
    });

    t0.join();
    t1.join();
    t2.join();

    for (u64 i = 0; i < m1.size(); ++i)
        m1(i) ^= m2(i);

    if (m1.cols() != 18)
        throw RTE_LOC;
    bool failed = false;

    std::vector<u8> temp(m1.cols());
    span<block> view((block*)hashs.data(), hashs.rows());
    for (u64 i = 0; i < view.size(); ++i)
    {
        auto j = srvs[0].mCuckoo.find(view[i]);

        if (!j)
            throw RTE_LOC;

        auto s0 = keyBitCount / 8;
        auto s1 = 8;

        memcpy(temp.data(), a.mColumns[0].mData.row(i).data(), s0);
        memcpy(temp.data() + s0, a.mColumns[1].mData.row(i).data(), s1);

        if (memcmp(m1[j.mCuckooPositon].data(), temp.data(), 18) != 0)
        {
            std::cout << i << "\n  "
                << hexString((u8*)m1[j.mCuckooPositon].data(), 18) << " vs \n  "
                << hexString((u8*)temp.data(), 18) << std::endl;
            failed = true;
        }
    }

    if(failed)
        throw RTE_LOC;


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

        if (i < 2)
        {

            std::vector<SharedTable::ColRef> outCols{ C["key"], C["data"] }; //};

            auto leftJoin = B["key"];
            auto rightJoin = B["key"];

            auto selectBytes = (leftJoin.mCol.getBitCount() + 7) / 8;
            for (auto& c : outCols)
                selectBytes += (c.mCol.getBitCount() + 7) / 8;

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


            auto intersectionFlags = srvs[i].compare(leftJoin, rightJoin, selects2, outCols);
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

            auto intersectionFlags = srvs[i].compare(leftJoin, rightJoin, {}, outCols);
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
void ComPsi_Intersect_test(u32 rows, u32 rows2);

void ComPsi_Intersect_test()
{
    ComPsi_Intersect_test(1 << 12, 1 << 12);
}


void ComPsi_Intersect_sl_test()
{
    ComPsi_Intersect_test(1 << 5, 1 << 12);
}

void ComPsi_Intersect_ls_test()
{
    ComPsi_Intersect_test(1 << 12, 1 << 5);
}


void ComPsi_Intersect_test(u32 rows, u32 rows2)
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



    PRNG prng(ZeroBlock);

    auto keyBitCount = srvs[0].mKeyBitCount;
    Table a(rows, {
        ColumnInfo{ "key", TypeID::IntID, keyBitCount },
        ColumnInfo{ "data3", TypeID::IntID, 32 }
        }),
        b(rows2, {
        ColumnInfo{ "key", TypeID::IntID, keyBitCount },
        ColumnInfo{ "data", TypeID::IntID, 64 },
        ColumnInfo{ "data2", TypeID::IntID, 128 }
    });


    auto intersectionSize = (std::min(rows, rows2) + 1) / 2;

    std::unordered_map<i64, i64> map;
    map.reserve(intersectionSize);


    // initialize a
    for (u64 i = 0; i < rows; ++i)
    {
        for (u64 j = 0; j < a.mColumns[0].mData.cols(); ++j)
            a.mColumns[0].mData(i, j) = i + 1;

        a.mColumns[1].mData(i) = prng.get<u32>();

        auto out = (i >= intersectionSize);
        if (!out)
            map.emplace(a.mColumns[0].mData(i, 0), i);
    }

    // initialize b
    for (u64 i = 0; i < rows2; ++i)
    {
        auto out = (i >= intersectionSize);
        for (u64 j = 0; j < b.mColumns[0].mData.cols(); ++j)
            b.mColumns[0].mData(i, j) = i + 1 + (rows * out);
    }
    i64* bb1 = b.mColumns[1].mData.data();
    i64* bb2 = b.mColumns[2].mData.data();
    prng.get(bb1, b.mColumns[1].mData.size());
    prng.get(bb2, b.mColumns[2].mData.size());

    bool failed = false;
    auto t0 = std::thread([&]() {
        setThreadName("t0");
        auto i = 0;
        auto A = srvs[i].localInput(a);
        auto B = srvs[i].localInput(b);

        auto C = srvs[i].join(
            /* where  */ A["key"],/* = */ B["key"],
            /* select */ { A["key"], B["data"], B["data2"], A["data3"] });

        aby3::Sh3::i64Matrix keys(C.mColumns[0].rows(), C.mColumns[0].i64Cols());
        aby3::Sh3::i64Matrix data(C.mColumns[1].rows(), C.mColumns[1].i64Cols());
        aby3::Sh3::i64Matrix data2(C.mColumns[2].rows(), C.mColumns[2].i64Cols());
        aby3::Sh3::i64Matrix data3(C.mColumns[3].rows(), C.mColumns[3].i64Cols());
        srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[0], keys);
        srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[1], data);
        srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[2], data2);
        srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[3], data3);

        if (keys.rows() != intersectionSize)
        {
            failed = true;
            std::cout << "bad size, exp: " << intersectionSize << ", act: " << keys.rows() << std::endl;
        }


        for (u64 i = 0; i < keys.rows(); ++i)
        {
            auto iter = map.find(keys(i, 0));
            if (iter == map.end())
            {
                failed = true;
                std::cout << "bad key in intersection: " << keys(i, 0) << " @ " << i << std::endl;
            }
            else
            {
                auto idx = iter->second;
                if (data(i, 0) != b.mColumns[1].mData(idx,0))
                {
                    failed = true;
                    std::cout << "bad data in intersection: " << data(i, 0) << " @ " << i << std::endl;
                }

                if (data2(i, 0) != b.mColumns[2].mData(idx, 0) ||
                    data2(i, 1) != b.mColumns[2].mData(idx, 1))
                {
                    failed = true;
                    std::cout << "bad data2 in intersection: " << data2(i, 0) << " " << data2(i, 1) << " @ " << i << std::endl;
                }

                if (data3(i, 0) != a.mColumns[1].mData(idx, 0))
                {
                    failed = true;
                    std::cout << "bad data3 in intersection: " << std::hex<<data3(i, 0) <<" vs "<< std::hex << a.mColumns[1].mData(idx, 0) << " @ " << std::dec << i << std::endl;
                }


                map.erase(iter);
            }

            //std::cout << "keys[" << i << "] = " << keys(i, 0) << std::endl;
        }

        for (auto& v : map)
        {
            std::cout << "missing idx " << v.second << std::endl;
            failed = true;
        }
    });

    auto r = [&](int i) {
        setThreadName("t" + ToString(i));
        auto A = srvs[i].remoteInput(0);
        auto B = srvs[i].remoteInput(0);

        auto C = srvs[i].join(A["key"], B["key"], { A["key"], B["data"], B["data2"], A["data3"] });
        aby3::Sh3::i64Matrix keys(C.mColumns[0].rows(), C.mColumns[0].i64Cols());
        aby3::Sh3::i64Matrix data(C.mColumns[1].rows(), C.mColumns[1].i64Cols());
        aby3::Sh3::i64Matrix data2(C.mColumns[2].rows(), C.mColumns[2].i64Cols());
        aby3::Sh3::i64Matrix data3(C.mColumns[3].rows(), C.mColumns[3].i64Cols());
        srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[0], keys);
        srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[1], data);
        srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[2], data2);
        srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[3], data3);
    };

    auto t1 = std::thread(r, 1);
    auto t2 = std::thread(r, 2);


    t0.join();
    t1.join();
    t2.join();

    if (failed)
        throw std::runtime_error("");

    //srvs[0].intersect(A, B);
}



void ComPsi_leftUnion_test()
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

    auto left = 1 << 4;
    auto mid = 1 << 5;
    auto right = 1 << 7;

    PRNG prng(ZeroBlock);

    auto keyBitCount = srvs[0].mKeyBitCount;
    Table 
        a(left + mid, {
            ColumnInfo{ "key", TypeID::IntID, keyBitCount }
            ,ColumnInfo{ "data", TypeID::IntID, 64 }
            }),
        b(right + mid, {
            ColumnInfo{ "key", TypeID::IntID, keyBitCount }
            ,ColumnInfo{ "data", TypeID::IntID, 64 }
            //,ColumnInfo{ "data2", TypeID::IntID, 128 }
            });


    std::unordered_map<i64, i64> map;
    map.reserve(mid);
    auto total = left + mid + right;

    auto th0 = right + mid;
    // initialize a
    for (u64 i = 0; i < total; ++i)
    {

        auto v = i + 1;
        if (i < th0)
        {
            for (u64 j = 0; j < a.mColumns[0].mData.cols(); ++j)
            {
                b.mColumns[0].mData(i, j) = v;
                b.mColumns[1].mData(i, 0) = v * 2;
            }
        }

        if (i >= right)
        {
            for (u64 j = 0; j < b.mColumns[0].mData.cols(); ++j)
            {
                a.mColumns[0].mData(i - right, j) = v;
                a.mColumns[1].mData(i - right, 0) = v * 3;
            }
        }

        map.emplace(v, i);
    }

    //// initialize b
    //for (u64 i = 0; i < rows2; ++i)
    //{
    //    auto out = (i >= intersectionSize);
    //    for (u64 j = 0; j < b.mColumns[0].mData.cols(); ++j)
    //        b.mColumns[0].mData(i, j) = i + 1 + (rows * out);
    //}
    //i64* bb1 = b.mColumns[1].mData.data();
    //i64* bb2 = b.mColumns[2].mData.data();
    //prng.get(bb1, b.mColumns[1].mData.size());
    //prng.get(bb2, b.mColumns[2].mData.size());

    bool failed = false;
    auto t0 = std::thread([&]() {
        setThreadName("t0");
        auto i = 0;
        auto A = srvs[i].localInput(a);
        auto B = srvs[i].localInput(b);

        auto C = srvs[i].rightUnion(
            /* where  */ A["key"],/* = */ B["key"],
            /* select */ { A["key"], A["data"] },
            /* and    */ { B["key"], B["data"] });

        aby3::Sh3::i64Matrix keys(C.mColumns[0].rows(), C.mColumns[0].i64Cols());
        aby3::Sh3::i64Matrix data(C.mColumns[1].rows(), C.mColumns[1].i64Cols());
        //aby3::Sh3::i64Matrix data2(C.mColumns[2].rows(), C.mColumns[2].i64Cols());
        //aby3::Sh3::i64Matrix data3(C.mColumns[3].rows(), C.mColumns[3].i64Cols());
        srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[0], keys);
        srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[1], data);
        //srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[2], data2);
        //srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[3], data3);

        if (keys.rows() != total)
        {
            failed = true;
            std::cout << "bad size, exp: " << total << ", act: " << keys.rows() << std::endl;
        }

        //for (u64 i = 0; i < keys.rows(); ++i)
        //    std::cout << "u[" << i << "] = " << keys(i, 0) << std::endl;

        for (u64 i = 0; i < keys.rows(); ++i)
        {
            auto iter = map.find(keys(i, 0));
            if (iter == map.end())
            {
                failed = true;
                std::cout << "bad key in union: " << keys(i, 0) << " @ " << i << std::endl;
            }
            else
            {
    
                auto idx = iter->second;
                auto exp = idx < th0 ?  b.mColumns[1].mData(idx, 0) : a.mColumns[1].mData(idx - right, 0);

                if (data(i, 0) != exp)
                {
                    failed = true;
                    std::cout << "bad data in union: " << data(i, 0) <<" != " << exp << " @ " << i << std::endl;
                }
                map.erase(iter);

            //    if (data2(i, 0) != b.mColumns[2].mData(idx, 0) ||
            //        data2(i, 1) != b.mColumns[2].mData(idx, 1))
            //    {
            //        failed = true;
            //        std::cout << "bad data2 in intersection: " << data2(i, 0) << " " << data2(i, 1) << " @ " << i << std::endl;
            //    }

            //    if (data3(i, 0) != a.mColumns[1].mData(idx, 0))
            //    {
            //        failed = true;
            //        std::cout << "bad data3 in intersection: " << std::hex << data3(i, 0) << " vs " << std::hex << a.mColumns[1].mData(idx, 0) << " @ " << std::dec << i << std::endl;
            //    }

            }

            //std::cout << "keys[" << i << "] = " << keys(i, 0) << std::endl;
        }

        for (auto& v : map)
        {
            std::cout << "missing idx " << v.second << std::endl;
            failed = true;
        }
    });

    auto r = [&](int i) {
        setThreadName("t" + ToString(i));
        auto A = srvs[i].remoteInput(0);
        auto B = srvs[i].remoteInput(0);

        auto C = srvs[i].rightUnion(A["key"], B["key"], { A["key"], A["data"] }, { B["key"], B["data"] });
        aby3::Sh3::i64Matrix keys(C.mColumns[0].rows(), C.mColumns[0].i64Cols());
        aby3::Sh3::i64Matrix data(C.mColumns[1].rows(), C.mColumns[1].i64Cols());
        //aby3::Sh3::i64Matrix data2(C.mColumns[2].rows(), C.mColumns[2].i64Cols());
        //aby3::Sh3::i64Matrix data3(C.mColumns[3].rows(), C.mColumns[3].i64Cols());
        srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[0], keys);
        srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[1], data);
        //srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[2], data2);
        //srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[3], data3);
    };

    auto t1 = std::thread(r, 1);
    auto t2 = std::thread(r, 2);


    t0.join();
    t1.join();
    t2.join();

    if (failed)
        throw std::runtime_error("");

    //srvs[0].intersect(A, B);
}


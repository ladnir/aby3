#include "Sh3ConverterTests.h"
#include "aby3/sh3/Sh3Converter.h"
#include "aby3/sh3/Sh3Encryptor.h"
#include <cryptoTools/Crypto/PRNG.h>
#include <cryptoTools/Common/BitIterator.h>
#include <cryptoTools/Common/Log.h>
using namespace aby3;
using namespace oc;

std::array<u8, 2> getBitShare(u64 row, u64 bitIdx, const sbMatrix& x)
{
    std::array<u8, 2> ret;

    for (u64 i = 0; i < 2; ++i)
    {
        auto word = bitIdx / 64;
        auto offset = bitIdx % 64;

        ret[i] = (x.mShares[i](row, word) >> offset) & 1;
    }

    return ret;
}

std::array<u8, 2> getBitShare(u64 row, u64 bitIdx, const sPackedBin& x)
{

    std::array<u8, 2> ret;

    for (u64 i = 0; i < 2; ++i)
    {
        auto word = row / 64;
        auto offset = row % 64;

        ret[i] = (x.mShares[i](bitIdx, word) >> offset) & 1;
    }

    return ret;
}
//u64 getMask(u64 bits)
//{
//    if (bits == 0)
//        return ~0ull;
//
//    return (1ull << bits) -1;
//}

//bool eq(const sPackedBin& a, const sPackedBin& b)
//{
//    return a.shareCount() == b.shareCount()
//        && a.bitCount() == b.bitCount()
//        && a.mShares == b.mShares;
//}
//
////
//
//void trim(sPackedBin& a)
//{
//    auto shares = a.mShareCount;
//    auto packLast = (shares + 63) / 64 - 1;
//    auto packMask = getMask(shares % 64);
//    for (u64 i = 0; i < a.bitCount(); ++i)
//    {
//        a.mShares[0](i, packLast) &= packMask;
//        a.mShares[1](i, packLast) &= packMask;
//    }
//}
//
//
//void trim(sbMatrix& a)
//{
//    auto bits = a.bitCount();
//    auto mtxLast = (bits + 63) / 64 - 1;
//    auto mtxMask = getMask(bits % 64);
//    for (u64 i = 0; i < a.rows(); ++i)
//    {
//        a.mShares[0](i, mtxLast) &= mtxMask;
//        a.mShares[1](i, mtxLast) &= mtxMask;
//    }
//}

void printBits(eMatrix<i64>& a, u64 bits)
{
    std::string b(bits, '0');
    //b.back() = '\n';
    auto total = a.cols() * 64;
    std::string c(total - bits, '0');
    std::cout << "    ";
    for (u64 j = 0; j < bits; ++j)
        std::cout << (j % 10);
    std::cout << '\n';

    for (u64 i = 0; i < (u64)a.rows(); ++i)
    {
        auto& r = a(i, 0);
        auto base = (u8*)&r;
        BitIterator iter(base, 0);

        for (u64 j = 0; j < bits; ++j)
        {
            b[j] = '0' + *iter;
            ++iter;
        }
        for (u64 j = 0; j < c.size(); ++j)
        {
            c[j] = '0' + *iter;
            ++iter;
        }

        std::cout << (i % 10) << " | " << b << " | " << c;// << '\n';
        //for (u64 j = 0; j < a.cols(); ++j)
        //    std::cout << std::hex << a(i, j) << ' ';

        std::cout << '\n';
    }

    std::cout << std::flush;
}

void pattern(oc::Matrix<i64>& a)
{
    auto bits = a.cols() * 64;
    for (u64 i = 0; i < a.rows(); ++i)
    {
        u64 k = i;
        BitIterator iter((u8*)&a(i, 0), 0);

        for (u64 j = 0; j < bits; ++j, ++k)
        {
            *iter = (k & 4);

            ++iter;
        }
    }
}


void corner(oc::Matrix<i64>& a, u64 bits)
{
    a.setZero();

    //a(0, 0) = -1;
    //a(3, 0) = -1;

    //printBits(a, bits);
    for (u64 i = bits / 2; i < a.rows(); ++i)
    {
        BitIterator iter((u8*)&a(i, 0), 0);
        //a(i, 0) = -1;


        for (u64 j = bits / 2; j < bits; ++j)
        {
            *iter = 1;
            //printBits(a, bits);

            ++iter;
        }
        //printBits(a, bits);

    }
}


void Sh3_convert_b64Matrix_PackedBin_test()
{
    auto trials = 10;
    auto mod = 256;

    for (u64 t = 0; t < (u64)trials; ++t)
    {
        PRNG prng(ZeroBlock);
        auto shares = prng.get<u64>() % mod + 1;
        auto bits = prng.get<u64>() % mod + 1;
        Sh3Encryptor enc;

        sbMatrix mtx(shares, bits), mtxDest;
        sPackedBin pack(shares, bits), packDest;

        enc.init(0, ZeroBlock, OneBlock);
        enc.rand(mtx);
        enc.rand(pack);

        //pattern(mtx.mShares[0]);
        corner(mtx.mShares[0], bits);
        mtx.trim();

        //trim(pack);

        Sh3Converter convt;
        packDest.mShares[0].setZero();
        packDest.mShares[1].setZero();
        mtxDest.mShares[0].setZero();
        mtxDest.mShares[1].setZero();
        convt.toPackedBin(mtx, packDest);

        //std::cout << std::endl;
        //printBits(mtx.mShares[0], mtx.bitCount());
        //std::cout << std::endl;
        //printBits(packDest.mShares[0], mtx.bitCount());
        //std::cout << std::endl;

        for (u64 r = 0; r < pack.shareCount(); ++r)
        {
            for (u64 b = 0; b < pack.bitCount(); ++b)
            {
                if (getBitShare(r, b, mtx) != getBitShare(r, b, packDest))
                {

                    throw std::runtime_error("");
                }
            }
        }

        convt.toBinaryMatrix(packDest, mtxDest);
        //trim(mtxDest);

        if (mtx != mtxDest)
            throw std::runtime_error(LOCATION);

        mtxDest.trim();

        //if (mtx != mtxDest)
        //{
        //    //std::cout << mtx.mShares[0] << std::endl;
        //    //std::cout << mtxDest.mShares[0] << std::endl;
        //    throw std::runtime_error(LOCATION);
        //}


        convt.toBinaryMatrix(pack, mtxDest);

        for (u64 r = 0; r < pack.shareCount(); ++r)
        {
            for (u64 b = 0; b < pack.bitCount(); ++b)
            {
                if (getBitShare(r, b, pack) != getBitShare(r, b, mtxDest))
                    throw std::runtime_error("");
            }
        }

        convt.toPackedBin(mtxDest, packDest);

        if (pack != packDest)
        {

            for (u64 i = 0; i < pack.mShares[0].rows(); ++i)
            {
                for (auto s : { 0,1 })
                {
                    for (u64 j = 0; j < pack.mShares[0].cols(); ++j)
                    {
                        std::cout << pack.mShares[s](i, j) << " ";
                    }
                    std::cout << "   |   ";

                }

                std::cout << "   #   ";
                for (auto s : { 0,1 })
                {

                    for (u64 j = 0; j < pack.mShares[0].cols(); ++j)
                    {
                        if (pack.mShares[s](i, j) != pack.mShares[s](i, j)) std::cout << Color::Red;
                        std::cout << pack.mShares[s](i, j) << " " << ColorDefault;

                    }
                    std::cout << "   |   ";
                }

                std::cout << std::endl;
            }

            throw std::runtime_error(LOCATION);
        }
    }


}


void Sh3_trim_test()
{

    auto trials = 100;
    auto mod = 256;

    for (u64 t = 0; t < (u64)trials; ++t)
    {
        PRNG prng(ZeroBlock);
        auto shares = prng.get<u64>() % mod + 1;
        auto bits = prng.get<u64>() % mod + 1;

        sbMatrix mtx(shares, bits), mtxDest;
        sPackedBin pack(shares, bits), packDest;

        memset(mtx.mShares[0].data(), ~0, mtx.mShares[0].size() * sizeof(i64));
        memset(mtx.mShares[1].data(), ~0, mtx.mShares[1].size() * sizeof(i64));
        memset(pack.mShares[0].data(), ~0, pack.mShares[0].size() * sizeof(i64));
        memset(pack.mShares[1].data(), ~0, pack.mShares[1].size() * sizeof(i64));

        mtx.trim();
        pack.trim();

        auto end = oc::roundUpTo(shares, 64);
        for (u64 i = 0; i < shares; ++i)
        {
            BitIterator iter0((u8*)pack.mShares[0][(i)].data(), 0);
            BitIterator iter1((u8*)pack.mShares[1][(i)].data(), 0);

            if (false)
            {
                std::cout << '\n';
                auto iter = iter0;
                for (u64 j = 0; j < end; ++j)
                {
                    if (j % 8 == 0)
                        std::cout << ' ';
                    if (j == shares)
                        std::cout << " - ";
                    std::cout << *iter;

                    ++iter;
                }
            }

            for (u64 j = 0; j < end; ++j)
            {
                auto exp = j < shares;
                if ((bool)*iter0 != exp)
                {
                    std::cout << "\n " << std::string(j, ' ') << '^' << std::endl;;
                    pack.trim();
                    throw std::runtime_error(LOCATION);
                }
                if ((bool)*iter1 != exp)
                {
                    pack.trim();
                    throw std::runtime_error(LOCATION);
                }

                ++iter0;
                ++iter1;
            }

        }
    }


}

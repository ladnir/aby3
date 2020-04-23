#include "testUtils.h"
#include <sstream>
#include <iomanip>
#include "cryptoTools/Common/BitIterator.h"
#include "cryptoTools/Common/Log.h"

namespace aby3
{

    std::string prettyShare(u64 partyIdx, const si64& v)
    {
        std::array<u64, 3> shares;
        shares[partyIdx] = v[0];
        shares[(partyIdx + 2) % 3] = v[1];
        shares[(partyIdx + 1) % 3] = -1;

        std::stringstream ss;
        ss << "(";
        if (shares[0] == -1) ss << "               _ ";
        else ss << std::hex << std::setw(16) << std::setfill('0') << shares[0] << " ";
        if (shares[1] == -1) ss << "               _ ";
        else ss << std::hex << std::setw(16) << std::setfill('0') << shares[1] << " ";
        if (shares[2] == -1) ss << "               _)";
        else ss << std::hex << std::setw(16) << std::setfill('0') << shares[2] << ")";

        return ss.str();
    }
    void rand(i64Matrix& A, oc::PRNG& prng)
    {
        prng.get(A.data(), A.size());
    }


    void run(Sh3Task t0, Sh3Task t1, Sh3Task t2)
    {
        std::thread thrd1 = std::thread([t1]() mutable {
            oc::setThreadName("p" +std::to_string(t1.getRuntime().mPartyIdx));
            t1.getRuntime().runUntilTaskCompletes(t1);
            });
        std::thread thrd2 = std::thread([t2]() mutable {
            oc::setThreadName("p" + std::to_string(t2.getRuntime().mPartyIdx));
            t2.getRuntime().runUntilTaskCompletes(t2);
            });

        t0.getRuntime().runUntilTaskCompletes(t0);

        thrd1.join();
        thrd2.join();
    }

    std::string bitstr(const i64Matrix& x, u64 w)
    {
        std::stringstream ss;
        for (u64 i = 0; i < x.rows(); ++i)
        {
            oc::BitIterator iter((u8*)x.row(i).data(), 0);
            for (u64 j = 0; j < w; ++j)
            {
                ss << (int)*iter++ << " ";
            }
            ss << "\n";
        }

        return ss.str();
    }

    void share(const i64Matrix& x, si64Matrix& x0, si64Matrix& x1, si64Matrix& x2, oc::PRNG& prng)
    {
        x0.resize(x.rows(), x.cols());
        x1.resize(x.rows(), x.cols());
        x2.resize(x.rows(), x.cols());

        prng.get(x0.mShares[0].data(), x0.mShares[0].size());
        prng.get(x1.mShares[0].data(), x1.mShares[0].size());

        for (u64 i = 0; i < x2.mShares[0].size(); ++i)
        {
            x2.mShares[0](i) = x(i) - x0.mShares[0](i) - x1.mShares[0](i);
        }

        x0.mShares[1] = x2.mShares[0];
        x1.mShares[1] = x0.mShares[0];
        x2.mShares[1] = x1.mShares[0];
    }

    void share(const i64Matrix& x, u64 colBitCount, sbMatrix& x0, sbMatrix& x1, sbMatrix& x2, oc::PRNG& prng)
    {
        x0.resize(x.rows(), colBitCount);
        x1.resize(x.rows(), colBitCount);
        x2.resize(x.rows(), colBitCount);

        prng.get(x0.mShares[0].data(), x0.mShares[0].size());
        prng.get(x1.mShares[0].data(), x1.mShares[0].size());

        for (u64 i = 0; i < x2.mShares[0].size(); ++i)
        {
            x2.mShares[0](i) = x(i) ^ x0.mShares[0](i) ^ x1.mShares[0](i);
        }

        x0.mShares[1] = x2.mShares[0];
        x1.mShares[1] = x0.mShares[0];
        x2.mShares[1] = x1.mShares[0];
    }

    int memcmp(eMatrix<i64> l, eMatrix<i64> r)
    {
        if (l.rows() != r.rows() || l.cols() != r.cols())
            return -1;

        return std::memcmp(l.data(), r.data(), r.size());
    }
    int memcmp(oc::MatrixView<i64> l, oc::MatrixView<i64> r)
    {
        if (l.rows() != r.rows() || l.cols() != r.cols())
            return -1;

        return std::memcmp(l.data(), r.data(), r.size());
    }

    void reveal(i64Matrix& x, sbMatrix& x0, sbMatrix& x1, sbMatrix& x2)
    {

        if (memcmp(x0.mShares[0], x1.mShares[1]) != 0)
            throw std::runtime_error(LOCATION);
        if (memcmp(x1.mShares[0], x2.mShares[1]) != 0)
            throw std::runtime_error(LOCATION);
        if (memcmp(x2.mShares[0], x0.mShares[1]) != 0)
            throw std::runtime_error(LOCATION);

        x.resize(x0.mShares[0].rows(), x0.mShares[0].cols());
        for (u64 i = 0; i < x0.mShares[0].size(); ++i)
            x(i) = x0.mShares[0](i)
            ^ x1.mShares[0](i)
            ^ x2.mShares[0](i);

        if (x0.mBitCount % 64)
        {
            i64 mask = (1ll << (x0.mBitCount % 64)) - 1;
            for (u64 i = 0; i < x.rows(); ++i)
                x(i, x.cols() - 1) &= mask;
        }

    }

    void reveal(i64Matrix& x, si64Matrix& x0, si64Matrix& x1, si64Matrix& x2)
    {
        if (memcmp(x0.mShares[0], x1.mShares[1]) != 0)
        {
            std::cout << "share 0 doesnt match\n" << x0.mShares[0] << "\n" << x1.mShares[1] << std::endl;
            throw std::runtime_error(LOCATION);
        }
        if (memcmp(x1.mShares[0], x2.mShares[1]) != 0)
        {
            std::cout << "share 1 doesnt match\n" << x1.mShares[0] << "\n" << x2.mShares[1] << std::endl;
            throw std::runtime_error(LOCATION);
        }
        if (memcmp(x2.mShares[0], x0.mShares[1]) != 0)
        {
            std::cout << "share 2 doesnt match\n" << x2.mShares[0] << "\n" << x0.mShares[1] << std::endl;
            throw std::runtime_error(LOCATION);
        }

        x.resize(x0.mShares[0].rows(), x0.mShares[0].cols());
        for (u64 i = 0; i < x0.mShares[0].size(); ++i)
            x(i) = x0.mShares[0](i)
            + x1.mShares[0](i)
            + x2.mShares[0](i);
    }


}
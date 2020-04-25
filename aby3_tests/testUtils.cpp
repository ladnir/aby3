#include "testUtils.h"
#include <sstream>
#include <iomanip>
#include "cryptoTools/Common/BitIterator.h"
#include "cryptoTools/Common/Log.h"
#include "cryptoTools/Network/Session.h"

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
            oc::setThreadName("p" + std::to_string(t1.getRuntime().mPartyIdx));
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

    i64Matrix pack(const i64Matrix& x)
    {
        i64Matrix ret(x.rows(), oc::roundUpTo(x.cols(), 64) / 64);

        for (u64 i = 0; i < x.rows(); i++)
        {
            auto iter = oc::BitIterator((u8*)ret.row(i).data(), 0);
            for (u64 j = 0; j < x.cols(); j++)
            {
                if (x(i, j) != 0 && x(i, j) != 1)
                    throw std::runtime_error("non binary value. " LOCATION);

                *iter++ = x(i, j);
            }
        }
        return ret;
    }

    i64Matrix unpack(const i64Matrix& x, u64 bitCount)
    {
        if (bitCount > x.cols() * 64)
            throw std::runtime_error(LOCATION);

        i64Matrix ret(x.rows(), bitCount);

        for (u64 i = 0; i < x.rows(); i++)
        {
            auto iter = oc::BitIterator((u8*)x.row(i).data(), 0);
            for (u64 j = 0; j < bitCount; j++)
            {
                ret(i, j) = *iter++;
            }
        }

        return ret;
    }

    void evaluate(oc::BetaCircuit cir, std::vector<i64Matrix> input, std::vector<i64Matrix*> y, bool sparse)
    {
        u64 n = input[0].rows();
        if (y.size() != cir.mOutputs.size())
            throw std::runtime_error(LOCATION);
        if (input.size() != cir.mInputs.size())
            throw std::runtime_error(LOCATION);

        for (u64 i = 0; i < n; ++i)
        {
            std::vector<oc::BitVector> in(input.size());
            std::vector<oc::BitVector> ou(y.size());

            for (u64 j = 0; j < input.size(); j++)
            {
                if (sparse)
                {
                    if (input[j].cols() != cir.mInputs[j].size())
                        throw std::runtime_error(LOCATION);
                    if (input[j].rows() != n)
                        throw std::runtime_error(LOCATION);

                    in[j].resize(input[j].cols());
                    for (u64 k = 0; k < in[j].size(); k++)
                        in[j][k] = input[j](i, k);
                }
                else
                {
                    in[j].resize(cir.mInputs[j].size());

                    if (input[j].cols() != oc::roundUpTo(in[j].sizeBytes(), sizeof(i64)))
                        throw std::runtime_error("LOCATION");

                    memcpy(in[j].data(), &input[j](i, 0), in[j].sizeBytes());
                }
            }

            for (u64 j = 0; j < y.size(); j++)
                ou[j].resize(cir.mOutputs[j].size());

            cir.evaluate(in, ou);

            for (u64 j = 0; j < y.size(); j++)
            {
                if (sparse)
                {
                    y[j]->resize(n, cir.mOutputs[j].size());

                    for (u64 k = 0; k < ou[j].size(); k++)
                        (*y[j])(i, k) = ou[j][k];
                }
                else
                {
                    y[j]->resize(n, oc::roundUpTo(cir.mOutputs[j].size(), 64) / 64);
                    memcpy(y[j]->row(j).data(), ou[j].data(), ou[j].sizeBytes());
                }
            }
        }

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

    std::array<CommPkg, 3> makeComms(oc::IOService& ios)
    {
        using namespace oc;
        auto chl01 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "01").addChannel();
        auto chl10 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "01").addChannel();
        auto chl02 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "02").addChannel();
        auto chl20 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "02").addChannel();
        auto chl12 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "12").addChannel();
        auto chl21 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "12").addChannel();
        std::array<CommPkg, 3> comms;
        comms[0] = { chl02, chl01 };
        comms[1] = { chl10, chl12 };
        comms[2] = { chl21, chl20 };

        return comms;
    }

    void makeRuntimes(std::array<Sh3Runtime, 3>& rt, oc::IOService& ios)
    {
        auto comms = makeComms(ios);
        makeRuntimes(rt, comms);
    }


    void makeRuntimes(std::array<Sh3Runtime, 3>& rts, std::array<CommPkg, 3>& comms)
    {
        rts[0].init(0, comms[0]);
        rts[1].init(1, comms[1]);
        rts[2].init(2, comms[2]);
    }
    std::array<Sh3Encryptor, 3> makeEncryptors()
    {
        std::array<Sh3Encryptor, 3> encs;
        encs[0].init(0, oc::toBlock(0ull), oc::toBlock(1));
        encs[1].init(1, oc::toBlock(1), oc::toBlock(2));
        encs[2].init(2, oc::toBlock(2), oc::toBlock(0ull));
        return encs;
    }
    std::array<Sh3ShareGen, 3> makeShareGens()
    {
        std::array<Sh3ShareGen, 3> gens;
        gens[0].init(oc::toBlock(0ull), oc::toBlock(1));
        gens[1].init(oc::toBlock(1), oc::toBlock(2));
        gens[2].init(oc::toBlock(2), oc::toBlock(0ull));
        return gens;
    }
    std::array<Sh3Converter, 3> makeConverters(std::array<Sh3Runtime, 3>& rts, std::array<Sh3ShareGen, 3>& gens)
    {
        std::array<Sh3Converter, 3> conv;
        conv[0].init(rts[0], gens[0]);
        conv[1].init(rts[1], gens[1]);
        conv[2].init(rts[2], gens[2]);
        return conv;
    }
}
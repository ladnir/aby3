#include "Sh3Converter.h"
#include <libOTe/Tools/Tools.h>
#include "Sh3BinaryEvaluator.h"
#include "cryptoTools/Common/Log.h"

using namespace oc;

namespace aby3
{



    void Sh3Converter::toPackedBin(const sbMatrix& in, sPackedBin& dest)
    {
        dest.reset(in.rows(), in.bitCount());

        for (u64 i = 0; i < 2; ++i)
        {
            auto& s = in.mShares[i];
            auto& d = dest.mShares[i];

            MatrixView<u8> inView(
                (u8*)(s.data()),
                (u8*)(s.data() + s.size()),
                sizeof(i64) * s.cols());

            MatrixView<u8> memView(
                (u8*)(d.data()),
                (u8*)(d.data() + d.size()),
                sizeof(i64) * d.cols());

            sse_transpose(inView, memView);
        }
    }

    void Sh3Converter::toBinaryMatrix(const sPackedBin& in, sbMatrix& dest)
    {
        dest.resize(in.shareCount(), in.bitCount());

        for (u64 i = 0; i < 2; ++i)
        {
            auto& s = in.mShares[i];
            auto& d = dest.mShares[i];

            MatrixView<u8> inView(
                (u8*)(s.data()),
                (u8*)(s.data() + s.size()),
                sizeof(i64) * s.cols());

            MatrixView<u8> memView(
                (u8*)(d.data()),
                (u8*)(d.data() + d.size()),
                sizeof(i64) * d.cols());

            sse_transpose(inView, memView);
        }
    }

    Sh3Task Sh3Converter::toBinaryMatrix(Sh3Task dep, const si64Matrix& in, sbMatrix& dest)
    {
        struct State
        {
            sbMatrix x0, x1;
        };

        auto state = std::make_shared<State>();

        return dep.then([&, state](CommPkg comm, Sh3Task self) {

            switch (dep.getRuntime().mPartyIdx)
            {
            case 0:
            {

                if (dest.rows() == 0)
                    dest.resize(in.rows(), 64 * in.cols());

                // x0 = in.mShares[0] + in.mShare[1];
                //    = in.0 + in.2 
                sbMatrix& x0 = state->x0;
                x0.resize(in.rows(), dest.bitCount());

                // x1 = 0
                //    = in.1
                sbMatrix& x1 = state->x1;
                x1.resize(in.rows(), dest.bitCount());

                for (u64 i = 0; i < x0.mShares[0].size(); ++i)
                {
                    x0.mShares[1](i) = mRandGen->mPrevCommon.get<i64>();
                    x0.mShares[0](i) = in.mShares[0](i) + in.mShares[1](i) ^ x0.mShares[1](i);
                }

                if (dest.bitCount() % 64)
                {
                    u64 back = x0.mShares[0].cols() - 1;
                    i64 mask = (1ull << (dest.bitCount() % 64)) - 1;
                    for (u64 i = 0; i < x0.mShares[0].rows(); ++i)
                    {
                        x0.mShares[0](i, back) &= mask;
                        x0.mShares[1](i, back) &= mask;
                    }
                }


                comm.mNext.asyncSend(x0.mShares[0].data(), x0.mShares[1].size());

                mCir = getArithToBinCircuit(64, dest.bitCount());
                mBin.asyncEvaluate(self, &mCir, { &x0,&x1 }, { &dest }).then([state](Sh3Task) {});

                break;
            }
            case 1:
            {

                if (dest.rows() == 0)
                    dest.resize(in.rows(), 64 * in.cols());

                // x0 = in.mShares[0] + in.mShare[1];
                //    = in.0 + in.2 
                sbMatrix& x0 = state->x0;
                x0.resize(in.rows(), dest.bitCount());

                // x1 = 0
                //    = in.1
                sbMatrix& x1 = state->x1;
                x1.resize(in.rows(), dest.bitCount());


                x0.mShares[0].setZero();

                for (u64 i = 0; i < x0.mShares[0].size(); ++i)
                {
                    x1.mShares[0](i) = in.mShares[0](i);
                }

                if (dest.bitCount() % 64)
                {
                    u64 back = x1.mShares[0].cols() - 1;
                    i64 mask = (1ull << (dest.bitCount() % 64)) - 1;
                    for (u64 i = 0; i < x0.mShares[0].rows(); ++i)
                    {
                        x1.mShares[0](i, back) &= mask;
                    }
                }

                auto f = comm.mPrev.asyncRecv(x0.mShares[1].data(), x0.mShares[1].size());



                mCir = getArithToBinCircuit(64, dest.bitCount());


                self.then([f = std::move(f)](CommPkg& _, Sh3Task __) mutable { f.get(); });
                mBin.asyncEvaluate(self, &mCir, { &x0,&x1 }, { &dest }).then([state](Sh3Task) {});
                break;
            }
            case 2:
            {
                if (dest.rows() == 0)
                    dest.resize(in.rows(), 64 * in.cols());

                // x0 = in.mShares[0] + in.mShare[1];
                //    = in.0 + in.2 
                sbMatrix& x0 = state->x0;
                x0.resize(in.rows(), dest.bitCount());

                // x1 = 0
                //    = in.1
                sbMatrix& x1 = state->x1;
                x1.resize(in.rows(), dest.bitCount());



                x0.mShares[1].setZero();

                for (u64 i = 0; i < x0.mShares[0].size(); ++i)
                {
                    x0.mShares[0](i) = mRandGen->mNextCommon.get<i64>();
                    x1.mShares[1](i) = in.mShares[1](i);
                }

                if (dest.bitCount() % 64)
                {
                    u64 back = x1.mShares[0].cols() - 1;
                    i64 mask = (1ull << (dest.bitCount() % 64)) - 1;
                    for (u64 i = 0; i < x0.mShares[0].rows(); ++i)
                    {
                        x0.mShares[0](i, back) &= mask;
                        x1.mShares[1](i, back) &= mask;
                    }
                }

                mCir = getArithToBinCircuit(64, dest.bitCount());
                mBin.asyncEvaluate(self, &mCir, { &x0,&x1 }, { &dest }).then([state](Sh3Task) {});
                break;
            }
            default:
                throw std::runtime_error("logic error. " LOCATION);
                break;
            }
            }
        ).getClosure();

    }

    Sh3Task Sh3Converter::bitInjection(Sh3Task dep, const sbMatrix& in, si64Matrix& dest)
    {
        struct State
        {
            std::vector<std::array<i64, 2>> msgs;
            std::vector<i64> mc;
            std::array < std::future<void>, 2> f;
            oc::BitVector choices;
            State(u64 n) :msgs(n), mc(n), choices(n) {}
        };

        return dep.then([&](CommPkg& comm, Sh3Task self) {

            dest.resize(in.rows(), in.bitCount());

            switch (self.getRuntime().mPartyIdx)
            {
            case 0:
            {
                // receiver 0
                auto state = std::make_shared<State>(in.rows() * in.bitCount());
                for (u64 i = 0, k = 0; i < in.rows(); ++i)
                {
                    auto iter = oc::BitIterator((u8*)&in.mShares[0](i, 0), 0);
                    for (u64 j = 0; j < in.bitCount(); ++j, ++k)
                        state->choices[k] = *iter++;
                }
                state->f[0] = comm.mPrev.asyncRecv(state->msgs);
                state->f[1] = comm.mNext.asyncRecv(state->mc);

                self.then([&, state](CommPkg& comm, Sh3Task self) {
                    state->f[0].get();
                    state->f[1].get();

                    for (u64 i = 0; i < state->msgs.size(); ++i)
                    {
                        dest.mShares[0](i) = state->msgs[i][state->choices[i]] ^ state->mc[i];
                    }
                    });

                // help 1
                mOT02.help(comm.mNext, state->choices);

                if (mRandGen == nullptr)
                    throw std::runtime_error("init was not called. " LOCATION);

                mRandGen->mPrevCommon.get(dest.mShares[1].data(), dest.mShares[1].size());

                break;
            }
            case 1:
            {
                // helper 0
                auto state = std::make_shared<State>(in.rows() * in.bitCount());
                for (u64 i = 0, k = 0; i < in.rows(); ++i)
                {
                    auto iter = oc::BitIterator((u8*)&in.mShares[1](i, 0), 0);
                    for (u64 j = 0; j < in.bitCount(); ++j, ++k)
                        state->choices[k] = *iter++;
                }
                mOT12.help(comm.mPrev, state->choices);
                mRandGen->mNextCommon.get(dest.mShares[0].data(), dest.mShares[0].size());

                // receiver 1
                state->f[0] = comm.mNext.asyncRecv(state->msgs);
                state->f[1] = comm.mPrev.asyncRecv(state->mc);

                self.then([&, state](CommPkg& comm, Sh3Task self) {
                    state->f[0].get();
                    state->f[1].get();

                    for (u64 i = 0; i < state->msgs.size(); ++i)
                    {
                        dest.mShares[1](i) = state->msgs[i][state->choices[i]] ^ state->mc[i];
                    }

                    });

                break;
            }
            case 2:
            {
                // sender 
                if (!mRandGen)
                    throw std::runtime_error("init was not called. " LOCATION);

                mRandGen->mNextCommon.get(dest.mShares[0].data(), dest.mShares[0].size());
                mRandGen->mPrevCommon.get(dest.mShares[1].data(), dest.mShares[1].size());

                std::vector<std::array<i64, 2>> m(in.rows() * in.bitCount());// , w(in.rows()* in.bitCount());
                for (u64 i = 0, k = 0; i < in.rows(); ++i)
                {
                    auto iter0 = oc::BitIterator((u8*)&in.mShares[0](i, 0), 0);
                    auto iter1 = oc::BitIterator((u8*)&in.mShares[1](i, 0), 0);

                    for (u64 j = 0; j < in.bitCount(); ++j, ++k)
                    {

                        u8 b = *iter0++ ^ *iter1++;
                        m[k][0] = -dest.mShares[0](k) - dest.mShares[1](k);
                        m[k][1] = m[k][0];
                        m[k][b ^ 1] += 1;
                    }
                }

                // sender 0
                mOT12.send(comm.mNext, m);
                // sender 1
                mOT02.send(comm.mPrev, m);

                break;
            }
            default:
                throw std::runtime_error("logic error");
            }

            }).getClosure();
    }

    oc::BetaCircuit Sh3Converter::getArithToBinCircuit(u64 base, u64 bitCount)
    {
        oc::BetaLibrary lib;
        oc::BetaCircuit cir;

        auto numWords = (base + bitCount - 1) / base;

        oc::BetaBundle in0(bitCount), in1(bitCount), out(bitCount);

        cir.addInputBundle(in0);
        cir.addInputBundle(in1);
        cir.addOutputBundle(out);

        std::vector<oc::BetaBundle> words0(numWords), words1(numWords), oWords(numWords);

        for (u64 i = 0; i < numWords; ++i)
        {
            auto begin = i * base;
            auto end = std::min<u64>(begin + base, bitCount);
            words0[i].mWires.insert(words0[i].mWires.end(),
                in0.mWires.begin() + begin,
                in0.mWires.begin() + end);
            words1[i].mWires.insert(words1[i].mWires.end(),
                in1.mWires.begin() + begin,
                in1.mWires.begin() + end);
            oWords[i].mWires.insert(oWords[i].mWires.end(),
                out.mWires.begin() + begin,
                out.mWires.begin() + end);

            lib.int_int_add_build_do(cir, words0[i], words1[i], oWords[i]);
        }

        return cir;
    }

    //Sh3Task Sh3Converter::toPackedBin(Sh3Task dep, const si64Matrix& in, sPackedBin& dest)
    //{
    //    return dep.then([&](CommPkg & comm, Sh3Task self) {
    //        struct State {
    //            Sh3BinaryEvaluator mEval;
    //            //oc::BetaCircuit mCircuit;
    //        };

    //        auto state = std::make_unique<State>();
    //        state->mEval.setCir(mLib.convert_arith_to_bin(in.cols(), 64), in.rows());

    //        throw RTE_LOC;
    //        //state->mEval.setInput
    //        //for(u64 i =0; i < in.)

    //    });
    //}

}
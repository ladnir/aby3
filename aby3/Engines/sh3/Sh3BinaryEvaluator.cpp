#include "Sh3BinaryEvaluator.h"
#include <cryptoTools/Common/Matrix.h>
#include <cryptoTools/Common/Log.h>
#include <libOTe/Tools/Tools.h>
#include <cryptoTools/Crypto/sha1.h>
#include "Sh3Converter.h"

namespace aby3
{
    using namespace oc;

    void Sh3BinaryEvaluator::setCir(BetaCircuit * cir, u64 width)
    {


        if (cir->mLevelCounts.size() == 0)
            cir->levelByAndDepth();

        //if (cir->mInputs.size() != 2) throw std::runtime_error(LOCATION);

        mCir = cir;
        //auto bits = sizeof(i64) * 8;
        //auto simdWidth = (width + bits - 1) / bits;

        // each row of mem corresponds to a wire. Each column of mem corresponds to 64 SIMD bits
        mMem.resize(width, mCir->mWireCount);
        //mMem[0].resize(cir->mWireCount, simdWidth);
        //mMem[1].resize(cir->mWireCount, simdWidth);
        //mMem[0].setZero();
        //mMem[1].setZero();


#ifdef BINARY_ENGINE_DEBUG
        if (mDebug)
        {
            mPlainWires_DEBUG.resize(width);
            for (auto& m : mPlainWires_DEBUG)
            {
                m.resize(cir->mWireCount);
                memset(m.data(), 0, m.size() * sizeof(DEBUG_Triple));
            }
        }
#endif
    }

    void Sh3BinaryEvaluator::setReplicatedInput(u64 idx, const Sh3::sbMatrix & in)
    {
        mLevel = 0;
        auto& inWires = mCir->mInputs[idx].mWires;
        auto simdWidth = mMem.simdWidth();
        auto bitCount = in.bitCount();

        if (mCir == nullptr)
            throw std::runtime_error(LOCATION);
        if (idx >= mCir->mInputs.size())
            throw std::invalid_argument("input index out of bounds");
        if (bitCount != inWires.size())
            throw std::invalid_argument("input data wrong size");
        if (in.rows() != 1)
            throw std::invalid_argument("incorrect number of simd rows");


        std::array<char, 2> vals{ 0,~0 };

        for (u64 i = 0; i < 2; ++i)
        {
            auto& shares = mMem.mShares[i];
            BitIterator iter((u8*)in.mShares[0].data(), 0);

            for (u64 j = 0; j < inWires.size(); ++j, ++iter)
            {
                if (shares.rows() <= inWires[j])
                    throw std::runtime_error(LOCATION);

                char v = vals[*iter];
                memset(shares.data() + simdWidth * inWires[j], v, simdWidth * sizeof(block));
            }
        }


#ifdef BINARY_ENGINE_DEBUG
        throw std::runtime_error("");
        if (mDebug)
        {
            for (u64 i = 0; i < inWires.size(); ++i)
            {
                validateWire(inWires[i]);
            }
        }
#endif
    }

    void Sh3BinaryEvaluator::setInput(u64 idx, const Sh3::sbMatrix  & in)
    {
        mLevel = 0;
        auto& inWires = mCir->mInputs[idx].mWires;
        auto simdWidth = mMem.simdWidth();

        auto bitCount = in.bitCount();
        auto inRows = in.rows();

        if (mCir == nullptr)
            throw std::runtime_error(LOCATION);
        if (idx >= mCir->mInputs.size())
            throw std::invalid_argument("input index out of bounds");
        if (bitCount != inWires.size())
            throw std::invalid_argument("input data wrong size");
        if (inRows != mMem.shareCount())
            throw std::invalid_argument("incorrect number of rows");

        for (u64 i = 0; i < inWires.size() - 1; ++i)
        {
            if (inWires[i] + 1 != inWires[i + 1])
                throw std::runtime_error("expecting contiguous input wires. " LOCATION);
        }

        for (u64 i = 0; i < 2; ++i)
        {
            auto& shares = mMem.mShares[i];

            MatrixView<u8> inView(
                (u8*)(in.mShares[i].data()),
                (u8*)(in.mShares[i].data() + in.mShares[i].size()),
                sizeof(i64) * in.mShares[i].cols());

            if (inWires.back() > shares.rows())
                throw std::runtime_error(LOCATION);

            MatrixView<u8> memView(
                (u8*)(shares.data() + simdWidth * inWires.front()),
                (u8*)(shares.data() + simdWidth * (inWires.back() + 1)),
                sizeof(block) * simdWidth);

            if (memView.data() + memView.size() > (u8*)(shares.data() + shares.size()))
                throw std::runtime_error(LOCATION);

            sse_transpose(inView, memView);
            //std::cout << " in* " << std::endl;
            //for (u64 r = 0; r < inView.bounds()[0]; ++r)
            //{
            //	BitVector bv(inView[r].data(), inView[r].size() * 8);
            //	std::cout << bv;
            //	//for (u64 c = 0; c < inView.bounds()[1]; ++c)
            //	//{
            //	//	std::cout << std::hex << (u64)inView(r, c) << " ";
            //	//}
            //	std::cout << std::endl;
            //}
            //std::cout << std::endl;
            //
            //
            //std::cout << " out*" << std::endl;
            //for (u64 r = 0; r < memView.bounds()[0]; ++r)
            //{
            //	for (u64 c = 0; c < memView.bounds()[1]; ++c)
            //	{
            //		std::cout << std::hex << (u64)memView(r, c) << " ";
            //	}
            //	std::cout << std::endl;
            //}
            //std::cout << std::endl;
        }

#ifdef BINARY_ENGINE_DEBUG

        if (mDebug)
        {
            Sh3Converter convt;
            Sh3::sPackedBin pack;
            convt.toPackedBin(in, pack);
            pack.trim();
            mMem.trim();
            for (u64 i = 0; i < inWires.size(); ++i)
            {
                for (u64 j = 0; j < 2; ++j)
                {
                    auto pPtr = pack.mShares[j][i].data();
                    auto mPtr = mMem.mShares[j][inWires[i]].data();
                    if (memcmp(pPtr, mPtr, pack.mShares[j].cols() * sizeof(i64)))
                    {

                        BitVector a((u8*)pPtr, pack.shareCount());
                        BitVector b((u8*)mPtr, mMem.shareCount());

                        auto leftover = ((pack.shareCount() + 63) / 64) * 64 - pack.shareCount();
                        BitVector aa, bb;

                        aa.append((u8*)pPtr, leftover, pack.shareCount());
                        bb.append((u8*)mPtr, leftover, pack.shareCount());
                        ostreamLock(std::cout)
                            << j << std::endl << std::hex
                            //<< pack.mShares[j].row(i) << std::endl
                            //<< mMem.mShares[j].row(inWires[i]) << std::endl
                            << a << " " << aa << std::endl
                            << b << " " << bb << std::endl;
                        throw std::runtime_error("");
                    }
                }
            }

            for (u64 r = 0; r < mPlainWires_DEBUG.size(); ++r)
            {



                auto prevIdx = (mDebugPartyIdx + 2) % 3;
                auto& m = mPlainWires_DEBUG[r];
                auto bv0 = BitVector((u8*)in.mShares[0][r].data(), inWires.size());
                auto bv1 = BitVector((u8*)in.mShares[1][r].data(), inWires.size());

                for (u64 i = 0; i < inWires.size(); ++i)
                {
                    m[inWires[i]].mBits[mDebugPartyIdx] = bv0[i];
                    m[inWires[i]].mBits[prevIdx] = bv1[i];
                    m[inWires[i]].mIsSet = true;
                    //if (inWires[i] < 10)
                    //	ostreamLock(std::cout) << mPartyIdx << " w[" << inWires[i] << "] = "
                    //	<< (int)m[inWires[i]].mBits[mPartyIdx] << std::endl;
                }
            }
        }
        //if (mDebugPartyIdx == 0)
        //    for (u64 i = 0; i < inWires.size(); ++i)
        //    {
        //        validateWire(inWires[i]);
        //    }
        //Matrix nn(in);
        //getOutput(inWires, nn);

        //if (nn.mShares[0] != in.mShares[0])
        //{
        //	std::cout << "exp " << in.mShares[0] << std::endl;
        //	std::cout << "act " << nn.mShares[0] << std::endl;
        //	throw std::runtime_error(LOCATION);
        //}
        //if (nn.mShares[1] != in.mShares[1]) throw std::runtime_error(LOCATION);
#endif

    }

    void Sh3BinaryEvaluator::setInput(u64 idx, const Sh3::sPackedBin & in)
    {
        //auto simdWidth = mMem.simdWidth();

        if (mCir == nullptr)
            throw std::runtime_error(LOCATION);
        if (idx >= mCir->mInputs.size())
            throw std::invalid_argument("input index out of bounds");
        if (in.shareCount() != mMem.shareCount())
            throw std::runtime_error(LOCATION);
        if (in.bitCount() != mCir->mInputs[idx].mWires.size())
            throw std::runtime_error(LOCATION);

        auto& inWires = mCir->mInputs[idx];
        for (u64 i = 0; i < 2; ++i)
        {
            auto& share = mMem.mShares[i];
            for (u64 j = 0; j < inWires.size(); ++j)
            {
                memcpy(
                    share.data() + mMem.simdWidth() * inWires[j],
                    in.mShares[i].data() + in.simdWidth() * j,
                    in.simdWidth() * sizeof(i64));
            }
        }

#ifdef BINARY_ENGINE_DEBUG
        if (mDebug)
        {
            throw std::runtime_error("");
            for (u64 i = 0; i < inWires.size(); ++i)
            {
                auto shareCount = mPlainWires_DEBUG.size();

                auto prevIdx = (mDebugPartyIdx + 2) % 3;
                //auto& m = mPlainWires_DEBUG[r];
                auto bv0 = BitVector((u8*)in.mShares[0][i].data(), shareCount);
                auto bv1 = BitVector((u8*)in.mShares[1][i].data(), shareCount);

                for (u64 r = 0; r < shareCount; ++r)
                {
                    auto& triple = mPlainWires_DEBUG[r][inWires[i]];

                    triple.mBits[mDebugPartyIdx] = bv0[r];
                    triple.mBits[prevIdx] = bv1[r];
                    triple.mIsSet = true;
                }

                validateWire(inWires[i]);
            }
        }
#endif
    }

#ifdef BINARY_ENGINE_DEBUG
    void Sh3BinaryEvaluator::validateMemory()
    {
        for (u64 i = 0; i < mCir->mWireCount; ++i)
        {
            validateWire(i);
        }
    }


    void Sh3BinaryEvaluator::validateWire(u64 wireIdx)
    {
        if (mDebug && mPlainWires_DEBUG[0][wireIdx].mIsSet)
        {
            auto shareCount = mPlainWires_DEBUG.size();
            auto prevIdx = (mDebugPartyIdx + 2) % 3;

            for (u64 r = 0; r < shareCount; ++r)
            {
                auto& triple = mPlainWires_DEBUG[r][wireIdx];

                auto bit0 = extractBitShare(r, wireIdx, 0);
                auto bit1 = extractBitShare(r, wireIdx, 1);
                if (triple.mBits[mDebugPartyIdx] != bit0)
                {
                    std::cout << "party " << mDebugPartyIdx << " wire " << wireIdx << " row " << r << " s " << 0 << " ~~"
                        << " exp:" << int(triple.mBits[mDebugPartyIdx])
                        << " act:" << int(bit0) << std::endl;

                    throw std::runtime_error(LOCATION);

                }
                if (triple.mBits[prevIdx] != bit1)
                {

                    std::cout << "party " << mDebugPartyIdx << " wire " << wireIdx << " row " << r << " s " << 1 << " ~~"
                        << " exp:" << int(triple.mBits[prevIdx])
                        << " act:" << int(bit1) << std::endl;
                    throw std::runtime_error(LOCATION);
                }
            }
        }
    }
    void Sh3BinaryEvaluator::distributeInputs()
    {

        if (mDebug)
        {
            SHA1 sha(sizeof(block));

            auto prevIdx = (mDebugPartyIdx + 2) % 3;
            auto nextIdx = (mDebugPartyIdx + 1) % 3;
            for (u64 r = 0; r < mPlainWires_DEBUG.size(); ++r)
            {
                auto& m = mPlainWires_DEBUG[r];

                std::vector<u8> s0(m.size()), s1;
                for (u64 i = 0; i < s0.size(); ++i)
                {
                    s0[i] = m[i].mBits[mDebugPartyIdx];
                }
                mDebugPrev.asyncSendCopy(s0);
                mDebugNext.asyncSendCopy(s0);
                mDebugPrev.recv(s0);
                mDebugNext.recv(s1);
                if (s0.size() != m.size())
                    throw std::runtime_error(LOCATION);

                for (u64 i = 0; i < m.size(); ++i)
                {
                    if (m[i].mBits[prevIdx] != s0[i])
                        throw std::runtime_error(LOCATION);

                    m[i].mBits[nextIdx] = s1[i];
                }


                sha.Update(m.data(), m.size());

            }

            block b;
            sha.Final(b);
            //ostreamLock(std::cout) << "b" << mDebugPartyIdx << " " << b << std::endl;
        }
    }
#endif


    Sh3Task Sh3BinaryEvaluator::asyncEvaluate(
        Sh3Task dep,
        oc::BetaCircuit * cir,
        std::vector<const Sh3::sbMatrix*> inputs,
        std::vector<Sh3::sbMatrix*> outputs)
    {
        if (cir->mInputs.size() != inputs.size())
            throw std::runtime_error(LOCATION);
        if (cir->mOutputs.size() != outputs.size())
            throw std::runtime_error(LOCATION);

        return dep.then([this, cir, inputs = std::move(inputs)](Sh3::CommPkg& comm, Sh3Task& self)
        {
            auto width = inputs[0]->rows();
            setCir(cir, width);

            for (u64 i = 0; i < inputs.size(); ++i)
            {
                if (inputs[i]->rows() != width)
                    throw std::runtime_error(LOCATION);

                setInput(i, *inputs[i]);
            }

#ifdef BINARY_ENGINE_DEBUG
            validateMemory();
            distributeInputs();
#endif

            roundCallback(comm, self);

        }).then([this, outputs = std::move(outputs)](Sh3Task& self)
        {
            for (u64 i = 0; i < outputs.size(); ++i)
            {
                getOutput(i, *outputs[i]);
            }
        });
    }

    Sh3Task Sh3BinaryEvaluator::asyncEvaluate(Sh3Task dependency)
    {
#ifdef BINARY_ENGINE_DEBUG
        validateMemory();
        distributeInputs();
#endif

        return dependency.then([this](Sh3::CommPkg& comm, Sh3Task& self)
        {
            roundCallback(comm, self);
        });
    }

    u8 Sh3BinaryEvaluator::extractBitShare(u64 rowIdx, u64 wireIdx, u64 shareIdx)
    {
        if (rowIdx >= mMem.shareCount())
            throw std::runtime_error("");

        auto row = (i64*)mMem.mShares[shareIdx][wireIdx].data();
        auto k = rowIdx / (sizeof(i64) * 8);
        auto j = rowIdx % (sizeof(i64) * 8);

        return (row[k] >> j) & 1;
    }

    void Sh3BinaryEvaluator::roundCallback(Sh3::CommPkg& comm, Sh3Task task)
    {

        auto shareCountDiv8 = (mMem.shareCount() + 7) / 8;
        auto simdWidth128 = mMem.simdWidth();

        if (mLevel > mCir->mLevelCounts.size())
        {
            throw std::runtime_error("evaluateRound() was called but no rounds remain... " LOCATION);
        }
        if (mCir->mLevelCounts.size() == 0 && mCir->mNonlinearGateCount)
            throw std::runtime_error("the level by and gate function must be called first." LOCATION);

        if (mLevel)
        {
            if (mRecvLocs.size())
            {
                mRecvFutr.get();


                auto iter = mRecvData.begin();

                for (u64 j = 0; j < mRecvLocs.size(); ++j)
                {
                    auto out = mRecvLocs[j];
                    memcpy(&mMem.mShares[1](out), &*iter, shareCountDiv8);
                    iter += shareCountDiv8;

                    //{
                    //    auto tt = iter - shareCountDiv8;
                    //    //for(u64 i = 0; i < )
                    //    ostreamLock o(std::cout);
                    //    o << ((mDebugPartyIdx+2)%3) << " " << out << " - ";
                    //    while (tt != iter)
                    //    {
                    //        o << std::hex << int(*tt++) << " ";
                    //    }
                    //    o << std::dec << std::endl;
                    //}

                    //validateWire(out);
                }

            }
        }
        else
        {
            mGateIter = mCir->mGates.data();
            auto seed0 = toBlock((task.getRuntime().mPartyIdx));
            auto seed1 = toBlock((task.getRuntime().mPartyIdx + 2) % 3);
            mShareGen.init(seed0, seed1, simdWidth128);
        }

        i64 ccWord;
        memset(&ccWord, 0xCC, sizeof(i64));

        const bool debug = mDebug;
        auto& shares = mMem.mShares;
        //ostreamLock(std::cout) << "P" << mDebugPartyIdx << " l" << mLevel << " : " << hashState() << std::endl;

        if (mLevel < mCir->mLevelCounts.size())
        {
            auto andGateCount = mCir->mLevelAndCounts[mLevel];
            auto gateCount = mCir->mLevelCounts[mLevel];

            mRecvLocs.resize(andGateCount);
            auto updateIter = mRecvLocs.data();
            std::vector<u8> mSendData(andGateCount * shareCountDiv8);
            auto sendIter = mSendData.begin();


            for (u64 j = 0; j < gateCount; ++j, ++mGateIter)
            {
                const auto& gate = *mGateIter;
                const auto& type = gate.mType;
                auto in0 = gate.mInput[0] * simdWidth128;
                auto in1 = gate.mInput[1] * simdWidth128;
                auto out = gate.mOutput * simdWidth128;
                auto s0_Out = &shares[0](out);
                auto s1_Out = &shares[1](out);
                auto s0_in0 = &shares[0](in0);
                auto s0_in1 = &shares[0](in1);
                auto s1_in0 = &shares[1](in0);
                auto s1_in1 = &shares[1](in1);

                std::array<block*, 2> z{ nullptr, nullptr };


                switch (gate.mType)
                {
                case GateType::Xor:
                    for (u64 k = 0; k < simdWidth128; ++k)
                    {
                        s0_Out[k] = s0_in0[k] ^ s0_in1[k];
                        s1_Out[k] = s1_in0[k] ^ s1_in1[k];
                    }
                    break;
                case GateType::And:
                    *updateIter++ = out;
                    z = getShares();
                    for (u64 k = 0; k < simdWidth128; ++k)
                    {

                        s0_Out[k]
                            = (s0_in0[k] & s0_in1[k])
                            ^ (s0_in0[k] & s1_in1[k])
                            ^ (s1_in0[k] & s0_in1[k])
                            ^ z[0][k]
                            ^ z[1][k];

#ifndef NDEBUG
                        if (eq(s1_in0[k], CCBlock) || eq(s1_in1[k], CCBlock))
                            throw std::runtime_error(LOCATION);
                        s1_Out[k] = CCBlock;
#endif

                    }
                    memcpy(&*sendIter, s0_Out, shareCountDiv8);
                    sendIter += shareCountDiv8;

                    break;
                case GateType::Nor:
                    *updateIter++ = out;
                    z = getShares();
                    for (u64 k = 0; k < simdWidth128; ++k)
                    {
                        auto mem00 = s0_in0[k] ^ AllOneBlock;
                        auto mem01 = s0_in1[k] ^ AllOneBlock;
                        auto mem10 = s1_in0[k] ^ AllOneBlock;
                        auto mem11 = s1_in1[k] ^ AllOneBlock;

                        TODO("add the randomization back");
                        s0_Out[k]
                            = (mem00 & mem01)
                            ^ (mem00 & mem11)
                            ^ (mem10 & mem01)
                            ^ z[0][k]
                            ^ z[1][k];

#ifndef NDEBUG
                        if (eq(s1_in0[k], CCBlock) || eq(s1_in1[k], CCBlock))
                            throw std::runtime_error(LOCATION);
                        s1_Out[k] = CCBlock;
#endif
                    }

                    memcpy(&*sendIter, s0_Out, shareCountDiv8);
                    sendIter += shareCountDiv8;
                    break;
                case GateType::Nxor:
                    for (u64 k = 0; k < simdWidth128; ++k)
                    {
                        s0_Out[k] = (s0_in0[k] ^ s0_in1[k]) ^ AllOneBlock;
                        s1_Out[k] = (s1_in0[k] ^ s1_in1[k]) ^ AllOneBlock;

                    }
                    break;
                case GateType::a:
                    for (u64 k = 0; k < simdWidth128; ++k)
                    {
#ifndef NDEBUG
                        if (eq(s1_in0[k], CCBlock))
                            throw std::runtime_error(LOCATION);
#endif

                        s0_Out[k] = s0_in0[k];
                        s1_Out[k] = s1_in0[k];

                    }
                    break;
                case GateType::na_And:
                    z = getShares();

                    *updateIter++ = out;
                    for (u64 k = 0; k < simdWidth128; ++k)
                    {

                        s0_Out[k]
                            = ((AllOneBlock ^ s0_in0[k]) & s0_in1[k])
                            ^ ((AllOneBlock ^ s0_in0[k]) & s1_in1[k])
                            ^ ((AllOneBlock ^ s1_in0[k]) & s0_in1[k])
                            ^ z[0][k]
                            ^ z[1][k];

#ifndef NDEBUG
                        if (eq(s1_in0[k], CCBlock) || eq(s1_in1[k], CCBlock))
                            throw std::runtime_error(LOCATION);
                        s1_Out[k] = CCBlock;
#endif
                    }

                    memcpy(&*sendIter, s0_Out, shareCountDiv8);
                    sendIter += shareCountDiv8;
                    break;
                case GateType::Zero:
                case GateType::nb_And:
                case GateType::nb:
                case GateType::na:
                case GateType::Nand:
                case GateType::nb_Or:
                case GateType::b:
                case GateType::na_Or:
                case GateType::Or:
                case GateType::One:
                default:

                    throw std::runtime_error("BinaryEngine unsupported GateType " LOCATION);
                    break;
                }



#ifdef BINARY_ENGINE_DEBUG
                if (debug)
                {
                    auto gIdx = mGateIter - mCir->mGates.data();
                    auto gIn0 = gate.mInput[0];
                    auto gIn1 = gate.mInput[1];
                    auto gOut = gate.mOutput;

                    //if (gate.mType == GateType::And)
                    //{
                    //    auto iter = sendIter - shareCountDiv8;
                    //    //for(u64 i = 0; i < )
                    //    ostreamLock o(std::cout);
                    //    o << mDebugPartyIdx<< " " << gOut << " - ";
                    //    while (iter != sendIter)
                    //    {
                    //        o << std::hex << int(*iter++) << " ";
                    //    }
                    //    o << std::dec << std::endl;
                    //}

                    auto prevIdx = (mDebugPartyIdx + 2) % 3;
                    for (u64 r = 0; r < mPlainWires_DEBUG.size(); ++r)
                    {
                        auto bit0 = extractBitShare(r, gOut, 0);

                        auto& m = mPlainWires_DEBUG[r];

                        m[gOut].assign(m[gIn0], m[gIn1], gate.mType);

                        if (bit0 != m[gOut].mBits[mDebugPartyIdx]) 
                        {
                            ostreamLock(std::cout)
                                << "\n g" << gIdx << " act: _  exp: " << (int)m[gate.mOutput].val() << std::endl
                                << m[gIn0].val() << " " << gateToString(type) << " " << m[gIn1].val() << " -> " << m[gOut].val() << std::endl
                                << gIn0 << " " << gateToString(type) << " " << gIn1 << " -> " << int(gOut) << std::endl;
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                            throw std::runtime_error(LOCATION);
                        }
                    }

                }
#endif
            }

            if (sendIter != mSendData.end()) throw std::runtime_error(LOCATION);

            mRecvData.resize(roundUpTo(mSendData.size(), 2));

            if (mSendData.size())
            {
                comm.mNext.asyncSend(std::move(mSendData));

                mRecvFutr = comm.mPrev.asyncRecv(mRecvData);
            }
        }

        mLevel++;
        if (mGateIter > mCir->mGates.data() + mCir->mGates.size()) throw std::runtime_error(LOCATION);


        if (hasMoreRounds())
        {
            task.nextRound([this](Sh3::CommPkg& comm, Sh3Task& task)
            {
                roundCallback(comm, task);
            }
            );
        }
    }

    void Sh3BinaryEvaluator::getOutput(u64 i, Sh3::sbMatrix & out)
    {
        if (mCir->mOutputs.size() <= i) throw std::runtime_error(LOCATION);

        const auto& outWires = mCir->mOutputs[i].mWires;

        getOutput(outWires, out);
    }

    void Sh3BinaryEvaluator::getOutput(u64 idx, Sh3::sPackedBin & out)
    {
        const auto& outWires = mCir->mOutputs[idx].mWires;

        out.resize(mMem.shareCount(), outWires.size());

        auto simdWidth128 = mMem.simdWidth();

        for (u64 j = 0; j < 2; ++j)
        {
            auto prevIdx = (mDebugPartyIdx + 2) % 3;
            auto jj = !j ? mDebugPartyIdx : prevIdx;
            auto dest = out.mShares[j].data();

            for (u64 wireIdx = 0; wireIdx < outWires.size(); ++wireIdx)
            {
                //if (mCir->isInvert(outWires[wireIdx]))
                //	throw std::runtime_error(LOCATION);
                auto wire = outWires[wireIdx];

                auto md = mMem.mShares[j].data();
                auto ms = mMem.mShares[j].size();
                auto src = md + wire * simdWidth128;
                auto size = out.simdWidth() * sizeof(i64);

                //if (src + simdWidth > md + ms)
                //    throw std::runtime_error(LOCATION);
                //if (dest + simdWidth > out..data() + outMem.size())
                //    throw std::runtime_error(LOCATION);


                memcpy(dest, src, size);

                // check if we need to ivert these output wires.
                if (mCir->isInvert(wire))
                {
                    for (u64 k = 0; k < out.simdWidth(); ++k)
                    {
                        dest[k] = ~dest[k];
                    }
                }

                dest += out.simdWidth();

#ifdef BINARY_ENGINE_DEBUG
                for (u64 r = 0; r < mPlainWires_DEBUG.size(); ++r)
                {
                    auto m = mPlainWires_DEBUG[r];
                    //auto k = r / (sizeof(i64) * 8);
                    //auto l = r % (sizeof(i64) * 8);
                    //i64 plain = (((u64*)src[k] >> l) & 1;
                    auto bit0 = extractBitShare(r, wire, j);

                    if (m[wire].mBits[jj] != bit0)
                        throw std::runtime_error(LOCATION);
                }
#endif
            }
        }
    }

    void Sh3BinaryEvaluator::getOutput(const std::vector<BetaWire>& outWires, Sh3::sbMatrix & out)
    {

        using Word = i64;
        if (outWires.size() != out.bitCount()) throw std::runtime_error(LOCATION);
        //auto outCols = roundUpTo(outWires.size(), 8);

        auto simdWidth128 = mMem.simdWidth();
        //auto simdWidth64 = (mMem.shareCount() + 63) / 64;
        Eigen::Matrix<block, Eigen::Dynamic, Eigen::Dynamic> temp;
        temp.resize(outWires.size(), simdWidth128);

        for (u64 j = 0; j < 2; ++j)
        {
            auto dest = temp.data();

            for (u64 i = 0; i < outWires.size(); ++i)
            {
                //if (mCir->isInvert(outWires[i]))
                //	throw std::runtime_error(LOCATION);

                auto md = mMem.mShares[j].data();
                auto ms = mMem.mShares[j].size();
                auto src = md + outWires[i] * simdWidth128;
                auto size = simdWidth128 * sizeof(block);

                if (src + simdWidth128 > md + ms)
                    throw std::runtime_error(LOCATION);
                if (dest + simdWidth128 > temp.data() + temp.size())
                    throw std::runtime_error(LOCATION);


                memcpy(dest, src, size);

                // check if we need to ivert these output wires.
                if (mCir->isInvert(outWires[i]))
                {
                    for (u64 k = 0; k < simdWidth128; ++k)
                    {
                        dest[k] = dest[k] ^ AllOneBlock;
                    }
                }

                dest += simdWidth128;

#ifdef BINARY_ENGINE_DEBUG
                if (mDebug)
                {
                    auto prev = (mDebugPartyIdx + 2) % 3;
                    auto jj = !j ? mDebugPartyIdx : prev;
                    for (u64 r = 0; r < mPlainWires_DEBUG.size(); ++r)
                    {
                        auto m = mPlainWires_DEBUG[r];
                        //auto k = r / (sizeof(Word) * 8);
                        //auto l = r % (sizeof(Word) * 8);
                        //Word plain = (src[k] >> l) & 1;

                        auto bit0 = extractBitShare(r, outWires[i], j);
                        //if (j == 1 && mDebugPartyIdx == 1 && i == 0 && r == 0)
                        //{
                        //    ostreamLock o(std::cout);
                        //    o << "hashState " << hashState() << std::endl;

                        //    for (u64 hh = 0; hh < simdWidth; ++hh)
                        //    {
                        //        o << " SS[" << hh << "] " << src[hh] << std::endl;
                        //    }

                        //    o << "ss " << int(m[outWires[i]].mBits[jj]) << " != " << plain << " = (" << src[k] <<" >> " << l<< ") & 1  : " << k << std::endl;
                        //}
                        if (m[outWires[i]].mBits[jj] != bit0)
                            throw std::runtime_error(LOCATION);
                    }
                }
#endif
            }
            MatrixView<u8> in((u8*)temp.data(), (u8*)(temp.data() + temp.size()), simdWidth128 * sizeof(block));
            MatrixView<u8> oout((u8*)out.mShares[j].data(), (u8*)(out.mShares[j].data() + out.mShares[j].size()), sizeof(Word) * out.mShares[j].cols());
            //memset(oout.data(), 0, oout.size());
            //out.mShares[j].setZero();
            memset(out.mShares[j].data(), 0, out.mShares[j].size() * sizeof(i64));
            sse_transpose(in, oout);

#ifdef BINARY_ENGINE_DEBUG
            if (mDebug)
            {

                auto prev = (mDebugPartyIdx + 2) % 3;
                auto jj = !j ? mDebugPartyIdx : prev;

                for (u64 i = 0; i < outWires.size(); ++i)
                {
                    for (u64 r = 0; r < mPlainWires_DEBUG.size(); ++r)
                    {
                        auto m = mPlainWires_DEBUG[r];
                        auto k = i / (sizeof(Word) * 8);
                        auto l = i % (sizeof(Word) * 8);
                        auto outWord = out.mShares[j](r, k);
                        auto plain = (outWord >> l) & 1;

                        auto inv = mCir->isInvert(outWires[i]) & 1;
                        auto xx = m[outWires[i]].mBits[jj] ^ inv;
                        if (xx != plain)
                            throw std::runtime_error(LOCATION);
                    }
                }
                auto mod = (outWires.size() % 64);
                u64 mask = ~(mod ? (Word(1) << mod) - 1 : -1);
                for (u64 i = 0; i < out.rows(); ++i)
                {
                    auto cols = out.mShares[j].cols();
                    if (out.mShares[j][i][cols - 1] & mask)
                        throw std::runtime_error(LOCATION);
                }
            }
#endif
        }
    }

    std::array<block*, 2> Sh3BinaryEvaluator::getShares()
    {

        mShareGen.refillBuffer();
        auto* z0 = mShareGen.mShareBuff[0].data();
        auto* z1 = mShareGen.mShareBuff[1].data();

#ifdef BINARY_ENGINE_DEBUG
        if (mDebug)
        {
            memset(mShareGen.mShareBuff[0].data(), 0, mShareGen.mShareBuff[0].size() * sizeof(block));
            memset(mShareGen.mShareBuff[1].data(), 0, mShareGen.mShareBuff[0].size() * sizeof(block));
        }
#endif
        return { z0, z1 };
    }


#ifdef BINARY_ENGINE_DEBUG

    void Sh3BinaryEvaluator::DEBUG_Triple::assign(
        const DEBUG_Triple & in0,
        const DEBUG_Triple & in1,
        GateType type)
    {
        mIsSet = true;
        auto vIn0 = int(in0.val());
        auto vIn1 = int(in1.val());

        if (vIn0 > 1) throw std::runtime_error(LOCATION);
        if (vIn1 > 1) throw std::runtime_error(LOCATION);

        u8 plain;
        if (type == GateType::Xor)
        {
            plain = vIn0 ^ vIn1;
            std::array<u8, 3> t;

            t[0] = in0.mBits[0] ^ in1.mBits[0];
            t[1] = in0.mBits[1] ^ in1.mBits[1];
            t[2] = in0.mBits[2] ^ in1.mBits[2];

            mBits = t;
        }
        else if (type == GateType::Nxor)
        {
            plain = vIn0 ^ vIn1 ^ 1;

            std::array<u8, 3> t;

            t[0] = in0.mBits[0] ^ in1.mBits[0] ^ 1;
            t[1] = in0.mBits[1] ^ in1.mBits[1] ^ 1;
            t[2] = in0.mBits[2] ^ in1.mBits[2] ^ 1;

            mBits = t;
        }
        else if (type == GateType::And)
        {
            plain = vIn0 & vIn1;
            std::array<u8, 3> t;
            for (u64 b = 0; b < 3; ++b)
            {

                auto bb = b ? (b - 1) : 2;

                if (bb != (b + 2) % 3) throw std::runtime_error(LOCATION);

                auto in00 = in0.mBits[b];
                auto in01 = in0.mBits[bb];
                auto in10 = in1.mBits[b];
                auto in11 = in1.mBits[bb];

                t[b]
                    = in00 & in10
                    ^ in00 & in11
                    ^ in01 & in10;
            }

            mBits = t;
        }

        else if (type == GateType::na_And)
        {
            plain = (1 ^ vIn0) & vIn1;
            std::array<u8, 3> t;
            for (u64 b = 0; b < 3; ++b)
            {

                auto bb = b ? (b - 1) : 2;

                if (bb != (b + 2) % 3) throw std::runtime_error(LOCATION);

                auto in00 = 1 ^ in0.mBits[b];
                auto in01 = 1 ^ in0.mBits[bb];
                auto in10 = in1.mBits[b];
                auto in11 = in1.mBits[bb];

                t[b]
                    = in00 & in10
                    ^ in00 & in11
                    ^ in01 & in10;
            }

            mBits = t;
        }
        else if (type == GateType::Nor)
        {
            plain = (vIn0 | vIn1) ^ 1;


            std::array<u8, 3> t;
            for (u64 b = 0; b < 3; ++b)
            {
                auto bb = b ? (b - 1) : 2;
                auto in00 = 1 ^ in0.mBits[b];
                auto in01 = 1 ^ in0.mBits[bb];
                auto in10 = 1 ^ in1.mBits[b];
                auto in11 = 1 ^ in1.mBits[bb];

                t[b]
                    = in00 & in10
                    ^ in00 & in11
                    ^ in01 & in10;
            }

            mBits = t;
        }
        else if (type == GateType::a)
        {
            plain = vIn0;
            mBits = in0.mBits;
        }
        else
            throw std::runtime_error(LOCATION);

        if (plain != (mBits[0] ^ mBits[1] ^ mBits[2]))
        {
            //ostreamLock(std::cout) /*<< " g" << (mGateIter - mCir->mGates.data())
            //<< " act: " << plain << "   exp: " << (int)m[gate.mOutput] << std::endl*/

            //	<< vIn0 << " " << gateToString(type) << " " << vIn1 << " -> " << int(m[gOut]) << std::endl
            //	<< gIn0 << " " << gateToString(type) << " " << gIn1 << " -> " << int(gOut) << std::endl;
            //std::this_thread::sleep_for(std::chrono::milliseconds(100));
            throw std::runtime_error(LOCATION);
        }
    }
#endif
}
//
//auto simdWidth = mMem.simdWidth();
//
//if (mLevel > mCir->mLevelCounts.size())
//throw std::runtime_error("evaluateRound() was called but no rounds remain... " LOCATION);
//
//if (mCir->mLevelCounts.size() == 0 && mCir->mNonXorGateCount)
//throw std::runtime_error("the level by and gate function must be called first." LOCATION);
//
//if (mLevel == 0) {
//    mGateIter = mCir->mGates.data();
//    mRecvData.resize(0);
//}
//
//if (mRecvData.size())
//{
//    mRecvFutr.get();
//    auto iter = mRecvData.begin();
//    for (u64 j = 0; j < mRecvLocs.size(); ++j)
//    {
//        auto out = mRecvLocs[j];
//        memcpy(&mMem.mShares[1](out), &*iter, simdWidth * sizeof(i64));
//        iter += simdWidth;
//    }
//}
//
//if (mLevel < mCir->mLevelCounts.size())
//{
//    i64 ccWord;
//    memset(&ccWord, 0xCC, sizeof(i64));
//
//    auto& shares = mMem.mShares;
//    auto andGateCount = mCir->mLevelAndCounts[mLevel];
//    auto gateCount = mCir->mLevelCounts[mLevel];
//    mRecvLocs.resize(andGateCount);
//    std::vector<i64> sendData(andGateCount * simdWidth);
//
//    auto recvLocIter = mRecvLocs.data();
//    auto sendIter = sendData.begin();
//
//
//    for (u64 j = 0; j < gateCount; ++j, ++mGateIter)
//    {
//        const auto& gate = *mGateIter;
//        const auto& type = gate.mType;
//        auto in0 = gate.mInput[0] * simdWidth;
//        auto in1 = gate.mInput[1] * simdWidth;
//        auto out = gate.mOutput * simdWidth;
//
//
//        switch (gate.mType)
//        {
//        case GateType::Xor:
//            for (u64 k = 0; k < simdWidth; ++k)
//            {
//                shares[0](out) = shares[0](in0) ^ shares[0](in1);
//                shares[1](out) = shares[1](in0) ^ shares[1](in1);
//
//                ++in0;
//                ++in1;
//                ++out;
//            }
//            break;
//        case GateType::And:
//            *recvLocIter++ = out;
//            for (u64 k = 0; k < simdWidth; ++k)
//            {
//
//                TODO("add the randomization back");
//                *sendIter = shares[0](out)
//                    = (shares[0](in0) & shares[0](in1))
//                    ^ (shares[0](in0) & shares[1](in1))
//                    ^ (shares[1](in0) & shares[0](in1))
//                    /*^ getBinaryShare()*/;
//
//#ifndef NDEBUG
//                if (shares[1](in0) == ccWord || shares[1](in1) == ccWord)
//                    throw std::runtime_error(LOCATION);
//                shares[1](out) = ccWord;
//#endif
//                ++sendIter;
//                ++in0;
//                ++in1;
//                ++out;
//            }
//            break;
//        case GateType::Nor:
//            *recvLocIter++ = out;
//            for (u64 k = 0; k < simdWidth; ++k)
//            {
//                auto mem00 = shares[0](in0) ^ -1;
//                auto mem01 = shares[0](in1) ^ -1;
//                auto mem10 = shares[1](in0) ^ -1;
//                auto mem11 = shares[1](in1) ^ -1;
//
//                TODO("add the randomization back");
//                *sendIter = shares[0](out)
//                    = (mem00 & mem01)
//                    ^ (mem00 & mem11)
//                    ^ (mem10 & mem01)
//                    /*^ getBinaryShare()*/;
//
//#ifndef NDEBUG
//                if (shares[1](in0) == ccWord || shares[1](in1) == ccWord)
//                    throw std::runtime_error(LOCATION);
//                shares[1](out) = ccWord;
//#endif
//                ++sendIter;
//                ++in0;
//                ++in1;
//                ++out;
//            }
//            break;
//        case GateType::Nxor:
//            for (u64 k = 0; k < simdWidth; ++k)
//            {
//                shares[0](out) = ~(shares[0](in0) ^ shares[0](in1));
//                shares[1](out) = ~(shares[1](in0) ^ shares[1](in1));
//
//                ++in0;
//                ++in1;
//                ++out;
//            }
//            break;
//        case GateType::a:
//            //memcpy(&shares[0](out), &shares[0](in0), simdWidth * sizeof(Word));
//            //memcpy(&shares[1](out), &shares[1](in0), simdWidth * sizeof(Word));
//            for (u64 k = 0; k < simdWidth; ++k)
//            {
//                if (shares[1](in0) == ccWord)
//                    throw std::runtime_error(LOCATION);
//
//                shares[0](out) = shares[0](in0);
//                shares[1](out) = shares[1](in0);
//
//                ++in0;
//                ++in1;
//                ++out;
//            }
//            break;
//        case GateType::na_And:
//            *recvLocIter++ = out;
//            for (u64 k = 0; k < simdWidth; ++k)
//            {
//
//                TODO("add the randomization back");
//                *sendIter = shares[0](out)
//                    = ((~shares[0](in0)) & shares[0](in1))
//                    ^ ((~shares[0](in0)) & shares[1](in1))
//                    ^ ((~shares[1](in0)) & shares[0](in1))
//                    /*^ getBinaryShare()*/;
//
//#ifndef NDEBUG
//                if (shares[1](in0) == ccWord || shares[1](in1) == ccWord)
//                    throw std::runtime_error(LOCATION);
//                shares[1](out) = ccWord;
//#endif
//                ++sendIter;
//                ++in0;
//                ++in1;
//                ++out;
//            }
//            break;
//        case GateType::Zero:
//        case GateType::nb_And:
//        case GateType::nb:
//        case GateType::na:
//        case GateType::Nand:
//        case GateType::nb_Or:
//        case GateType::b:
//        case GateType::na_Or:
//        case GateType::Or:
//        case GateType::One:
//        default:
//
//            throw std::runtime_error("Sh3BinaryEvaluator unsupported GateType " LOCATION);
//            break;
//        }
//
//
//
//#ifdef BINARY_ENGINE_DEBUG
//        if (mDebug)
//        {
//
//            out = gate.mOutput * simdWidth;
//
//            mDebugNext.asyncSendCopy(&shares[0](out), simdWidth);
//            mDebugPrev.asyncSendCopy(&shares[0](out), simdWidth);
//
//            std::vector<i64> s0(simdWidth), s1(simdWidth);
//            mDebugPrev.recv(s0);
//            mDebugNext.recv(s1);
//
//            for (u64 k = 0; k < simdWidth; ++k)
//                s0[k] ^= s1[k] ^ shares[0](out + k);
//
//            for (u64 r = 0; r < mPlainWires_DEBUG.size(); ++r)
//            {
//                auto k = r / (sizeof(i64) * 8);
//                auto j = r % (sizeof(i64) * 8);
//                i64 plain = (s0[k] >> j) & 1;
//                i64 zeroShare = (shares[0](out + k) >> j) & 1;
//
//                auto gIn0 = gate.mInput[0];
//                auto gIn1 = gate.mInput[1];
//                auto gOut = gate.mOutput;
//
//                auto& m = mPlainWires_DEBUG[r];
//                auto gIdx = mGateIter - mCir->mGates.data();
//
//                m[gOut].assign(m[gIn0], m[gIn1], gate.mType);
//
//                if (zeroShare != m[gOut].mBits[mDebugPartyIdx])
//                    throw std::runtime_error(LOCATION);
//
//
//                if (plain != m[gOut].val())
//                {
//                    ostreamLock(std::cout)
//                        << "\n g" << gIdx << " act: " << plain << "   exp: " << (int)m[gate.mOutput].val() << std::endl
//                        << m[gIn0].val() << " " << gateToString(type) << " " << m[gIn1].val() << " -> " << m[gOut].val() << std::endl
//                        << gIn0 << " " << gateToString(type) << " " << gIn1 << " -> " << int(gOut) << std::endl;
//                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
//                    throw std::runtime_error(LOCATION);
//                }
//
//            }
//        }
//#endif
//    }
//
//    if (sendIter != sendData.end()) throw std::runtime_error(LOCATION);
//
//    if (sendData.size())
//    {
//        comm.mNext.asyncSend(std::move(sendData));
//
//        mRecvData.resize(mRecvLocs.size() * simdWidth);
//        mRecvFutr = comm.mPrev.asyncRecv(mRecvData.data(), mRecvData.size());
//
//    }
//}
//
//mLevel++;
//if (mGateIter > mCir->mGates.data() + mCir->mGates.size())
//throw std::runtime_error(LOCATION);
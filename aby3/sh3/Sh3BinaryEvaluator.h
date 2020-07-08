#pragma once
#include "Sh3Types.h"

#include <cryptoTools/Circuit/BetaCircuit.h>
#include "Sh3Runtime.h"
#include <cryptoTools/Crypto/RandomOracle.h>
#include <cryptoTools/Crypto/PRNG.h>
#include "Sh3ShareGen.h"
#include <boost/align/aligned_allocator.hpp>
#include <vector>

namespace aby3
{

    class Sh3BinaryEvaluator
    {
    public:
#if defined(__AVX2__) || defined(_MSC_VER)
        using block_type = __m256i;
#else
        using block_type = block;
#endif

#define BINARY_ENGINE_DEBUG

#ifdef BINARY_ENGINE_DEBUG
    private:
        bool mDebug = false;
    public:

        void enableDebug(u64 partyIdx, oc::Channel debugPrev, oc::Channel debugNext)
        {
            mDebug = true;
            mDebugPartyIdx = partyIdx;

            mDebugPrev = debugPrev;
            mDebugNext = debugNext;
        }

        struct DEBUG_Triple
        {
            std::array<u8, 3> mBits;
            bool mIsSet;
            u16 val()const { return mBits[0] ^ mBits[1] ^ mBits[2]; }

            void assign(const DEBUG_Triple& in0, const DEBUG_Triple& in1, oc::GateType type);
        };
        std::vector<std::vector<DEBUG_Triple>> mPlainWires_DEBUG;
        u64 mDebugPartyIdx = -1;
        oc::Channel mDebugPrev, mDebugNext;

        u8 extractBitShare(u64 rowIdx, u64 wireIdx, u64 shareIdx);

        void validateMemory();
        void validateWire(u64 wireIdx);
        void distributeInputs();

        oc::block hashDebugState();
        std::stringstream mLog;
#endif

        oc::BetaCircuit* mCir;
        std::vector<oc::BetaGate>::iterator mGateIter;
        u64 mLevel;
        std::vector<u32> mRecvLocs;
        std::vector<u8> mRecvData;
        std::array<std::vector<u8>, 2> mSendBuffs;

        std::vector<std::future<void>> mRecvFutr;
        sPackedBinBase<block_type> mMem;
        //std::array<std::vector<block>, 2>  mZeroShares;

        void setCir(oc::BetaCircuit* cir, u64 width);

        void setReplicatedInput(u64 i, const sbMatrix& in);
        void setInput(u64 i, const sbMatrix& in);
        void setInput(u64 i, const sPackedBin& in);

        Sh3Task asyncEvaluate(Sh3Task dependency, oc::BetaCircuit* cir, std::vector<const sbMatrix*> inputs, std::vector<sbMatrix*> outputs);
        Sh3Task asyncEvaluate(Sh3Task dependency);


        void roundCallback(CommPkg& comms, Sh3Task task);

        void getOutput(u64 i, sPackedBin& out);
        void getOutput(const std::vector<oc::BetaWire>& wires, sPackedBin& out);

        void getOutput(u64 i, sbMatrix& out);
        void getOutput(const std::vector<oc::BetaWire>& wires, sbMatrix& out);

        bool hasMoreRounds() const {
            return mLevel <= mCir->mLevelCounts.size();
        }


        oc::block hashState()
        {
            oc::RandomOracle h(sizeof(block));
            h.Update(mMem.mShares[0].data(), mMem.mShares[0].size());
            h.Update(mMem.mShares[1].data(), mMem.mShares[1].size());

            block b;
            h.Final(b);
            return b;
        }

        //std::unique_ptr<block_type[]> mShareBacking;
        std::vector<block_type, boost::alignment::aligned_allocator<block_type>> mShareBuff;
        //span<block_type> mShareBuff;
        u64 mShareIdx;
        std::array<oc::AES, 2> mShareAES;

        block_type* getShares();
        Sh3ShareGen mShareGen;
        block_type mCheckBlock;

        oc::PRNG mPrng;
        //std::array<oc::PRNG, 2> mGens;
    };

}
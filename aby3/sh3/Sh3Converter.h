#pragma once

#include "Sh3Types.h"
#include "Sh3Runtime.h"
#include "aby3/Circuit/CircuitLibrary.h"
#include "cryptoTools/Crypto/PRNG.h"
#include "aby3/OT/SharedOT.h"
#include "aby3/sh3/Sh3ShareGen.h"
#include "aby3/sh3/Sh3BinaryEvaluator.h"

namespace aby3
{


    class Sh3Converter
    {
    public:
        CircuitLibrary mLib;
        Sh3ShareGen *mRandGen = nullptr;
        Sh3BinaryEvaluator mBin;

        SharedOT mOT12, mOT02;
        oc::BetaCircuit mCir;


        void init(Sh3Runtime& rt, Sh3ShareGen& gen)
        {
            mRandGen = &gen;
            mOT12.mIdx = rt.mPartyIdx;
            mOT02.mIdx = rt.mPartyIdx;

            if (rt.mPartyIdx == 0)
                mOT02.setSeed(mRandGen->mPrevCommon.get());
            if (rt.mPartyIdx == 1)
                mOT12.setSeed(mRandGen->mNextCommon.get());
            if (rt.mPartyIdx == 2)
            {
                mOT12.setSeed(mRandGen->mPrevCommon.get());
                mOT02.setSeed(mRandGen->mNextCommon.get());
            }
        }


        void toPackedBin(const sbMatrix& in, sPackedBin& dest);

        void toBinaryMatrix(const sPackedBin& in, sbMatrix& dest);


        //Sh3Task toPackedBin(Sh3Task dep, const si64Matrix& in, sPackedBin& dest);
        Sh3Task toBinaryMatrix(Sh3Task dep, const si64Matrix& in, sbMatrix& dest);

        Sh3Task bitInjection(Sh3Task dep, const sbMatrix& in, si64Matrix& dest);
        //Sh3Task toSi64Matrix(Sh3Task dep, const sPackedBin& in, si64Matrix& dest);


        oc::BetaCircuit getArithToBinCircuit(u64 base, u64 bitCount);
    };
}
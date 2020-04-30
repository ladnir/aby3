#pragma once
#include "aby3/sh3/Sh3Runtime.h"
#include "cryptoTools/Circuit/BetaLibrary.h"
#include "aby3/sh3/Sh3BinaryEvaluator.h"
#include "aby3/sh3/Sh3Converter.h"

namespace aby3
{
    class TreeBase
    {
    public:
        //enum class Comparitor
        //{
        //    Eq,
        //    Lt
        //};
        Sh3Task compare(
            Sh3Task dep, 
            const sbMatrix& x, 
            const sbMatrix& y,
            u64 nodesPerTree,
            sbMatrix& cmp);


        Sh3Task vote(Sh3Task dep, const sbMatrix& pred, sbMatrix& out);
        void initVotingCircuit(u64 n, u64 bitCount);

        oc::BetaCircuit mReduceCir, mVoteCircuit;
        oc::BetaLibrary mLib;
        Sh3BinaryEvaluator mBin;
        Sh3Converter mConv;
        //si64Matrix mSums;
        bool mUnitTest = false;

    };
}
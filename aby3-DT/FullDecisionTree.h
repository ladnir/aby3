#pragma once
#include "cryptoTools/Common/Defines.h"
#include "cryptoTools/Common/CLP.h"
#include "aby3/sh3/Sh3BinaryEvaluator.h"
#include "aby3/sh3/Sh3Converter.h"
#include "cryptoTools/Circuit/BetaLibrary.h"

namespace aby3
{
    class FullDecisionTree
    {
    public:
        enum class Comparitor
        {
            Eq,
            Lt
        };
        sbMatrix run(u64 depth, u64 featureCount, Sh3Runtime& rt);



        Sh3Task innerProd(Sh3Task dep, sbMatrix& x, sbMatrix& y, sbMatrix& z);
        Sh3Task compare(Sh3Task dep, sbMatrix& x, sbMatrix& y, sbMatrix& cmp, Comparitor type);
        
        Sh3Task reduce(Sh3Task dep, sbMatrix& cmp, sbMatrix& labels, sbMatrix& pred);
        Sh3Task vote(Sh3Task dep, sbMatrix& pred, sbMatrix& out);

        void initReduceCircuit(u64 labelBitCount);
        void initVotingCircuit(u64 n, u64 bitCount);

        u64 mDepth;
        oc::BetaCircuit mReduceCir, mVoteCircuit;
        oc::BetaLibrary mLib;
        Sh3BinaryEvaluator mBin;
        Sh3Converter mConv;
    };


    namespace tests
    {
        void FullTree_innerProd_test(const osuCrypto::CLP&);

    }

}
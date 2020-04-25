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
        void init(u64 depth, u64 featureCount, Sh3Runtime& rt, Sh3ShareGen& gen, bool unitTest = false);
        sbMatrix run(Sh3Runtime& rt);

        CommPkg mDebug;
        bool mUnitTest = false;

        Sh3Task innerProd(Sh3Task dep, sbMatrix& x, sbMatrix& y, sbMatrix& z);
        Sh3Task compare(Sh3Task dep, sbMatrix& x, sbMatrix& y, sbMatrix& cmp, Comparitor type);
        
        Sh3Task reduce(Sh3Task dep, sbMatrix& cmp, sbMatrix& labels, u64 labelBitCount, sbMatrix& pred);
        Sh3Task vote(Sh3Task dep, sbMatrix& pred, sbMatrix& out);

        void initReduceCircuit(u64 labelBitCount);
        void initVotingCircuit(u64 n, u64 bitCount);

        u64 mDepth = 0, mFeatureCount = 0;
        oc::BetaCircuit mReduceCir, mVoteCircuit;
        oc::BetaLibrary mLib;
        Sh3BinaryEvaluator mBin;
        Sh3Converter mConv;

        si64Matrix mSums;
        //sbMatrix mActive, mOutLabels;
    };


    namespace tests
    {
        void FullTree_innerProd_test(const osuCrypto::CLP&);
        void FullTree_compare_test(const osuCrypto::CLP&);
        void FullTree_reduce_test(const osuCrypto::CLP&);
        void FullTree_vote_test(const osuCrypto::CLP&);
        void FullTree_endToEnd_test(const osuCrypto::CLP&);

    }

}
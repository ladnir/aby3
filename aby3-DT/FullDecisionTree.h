#pragma once
#include "cryptoTools/Common/Defines.h"
#include "cryptoTools/Common/CLP.h"
#include "aby3/sh3/Sh3Converter.h"
#include "Common.h"
namespace aby3
{
    class FullDecisionTree : public TreeBase
    {
    public:
        //enum class Comparitor
        //{
        //    Eq,
        //    Lt
        //};
        void init(
            u64 depth,
            u64 numTrees,
            u64 featureCount,
            u64 featureBitCount,
            u64 nodeBitCount,
            u64 numLabels, 
            Sh3Runtime& rt, Sh3ShareGen& gen, bool unitTest = false);

        Sh3Task evaluate(Sh3Task dep,
            const sbMatrix& nodes,
            const sbMatrix& features,
            const sbMatrix& mapping,
            const sbMatrix& labels,
            sbMatrix& pred);

        CommPkg mDebug;

        Sh3Task innerProd(Sh3Task dep, const sbMatrix& x, const sbMatrix& y, sbMatrix& z);

        Sh3Task reduce(Sh3Task dep, const sbMatrix& cmp, const sbMatrix& labels, u64 labelBitCount, sbMatrix& pred);

        void initReduceCircuit(u64 labelBitCount);

        u64 mDepth = 0, 
            mNumTrees = 0,
            mFeatureCount = 0,
            mFeatureBitCount = 0,
            mNodeBitCount = 0,
            mNumLabels = 0;

        oc::BetaCircuit mReduceCir;

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
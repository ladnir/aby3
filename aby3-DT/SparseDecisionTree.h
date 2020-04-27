#pragma once
#include "aby3/Common/Defines.h"
#include "aby3/sh3/Sh3Runtime.h"
#include "aby3/sh3/Sh3BinaryEvaluator.h"
#include "aby3/sh3/Sh3ShareGen.h"
#include "cryptoTools/Crypto/PRNG.h"
#include "cryptoTools/Common/CLP.h"

namespace aby3
{

    //struct SparseTree
    //{
    //    u64 mDepth;
    //};


    class SparseDecisionForest
    {

    public:

        void init(
            u64 depth,
            u64 numTrees,
            u64 featureCount,
            u64 featureBitCount,
            u64 nodeBitCount,
            u64 numLabels,
            Sh3Runtime& rt, Sh3ShareGen& gen);

        Sh3BinaryEvaluator mBin;
        Sh3ShareGen mGen;
        oc::PRNG mPrng;
        //sbMatrix mNodeIdxs, mNodeLR, mNodeThreshold, mNodeIsLeaf, mNodeLabel;
        std::vector<u32> mTreeStartIdx;
        sbMatrix mNodes, mNodeNames, mFeatureNames;
        u64 mBlockSize = -1, mROffset = -1, mLOffset = -1, mFeatureOffset = -1, mFeatureCount = -1;
        std::vector<sbMatrix> mFreatureKey, mNodeKey;

        oc::BetaCircuit mLowMCCir;

        Sh3Task shuffleNodes(Sh3Task dep);

        void sampleKeys();

        Sh3Task computeNodeNames(Sh3Task dep);

        Sh3Task computeFeatureNames(Sh3Task dep);


        Sh3Task traversTree(Sh3Task dep);
    };



    namespace tests
    {
        void Sparse_shuffle_test(const oc::CLP& cmd);
        void Sparse_NodeNames_test(const oc::CLP& cmd);
        void Sparse_FeatureNames_test(const oc::CLP& cmd);
    }
}
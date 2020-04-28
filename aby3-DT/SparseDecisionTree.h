#pragma once
#include "aby3/Common/Defines.h"
#include "aby3/sh3/Sh3Runtime.h"
#include "aby3/sh3/Sh3Encryptor.h"
#include "aby3/sh3/Sh3BinaryEvaluator.h"
#include "aby3/sh3/Sh3ShareGen.h"
#include "cryptoTools/Crypto/PRNG.h"
#include "cryptoTools/Common/CLP.h"
#include "Common.h"
#include "cryptoTools/Common/Timer.h"
namespace aby3
{

    //struct SparseTree
    //{
    //    u64 mDepth;
    //};


    class SparseDecisionForest : public TreeBase, public oc::TimerAdapter
    {

    public:

        void init(
            u64 depth,
            std::vector<u32> mTreeStartIdx,
            sbMatrix& nodes,
            u64 featureBitCount,
            u64 numLabels,
            Sh3Runtime& rt, 
            Sh3ShareGen&& gen);

        Sh3Task evaluate(Sh3Task dep, sbMatrix& features);

        Comparitor mComparitor;
        Sh3Encryptor mEnc;
        Sh3BinaryEvaluator mBin2;
        Sh3ShareGen mGen;
        oc::PRNG mPrng;
        //sbMatrix mNodeIdxs, mNodeLR, mNodeThreshold, mNodeIsLeaf, mNodeLabel;
        std::vector<u32> mTreeStartIdx;
        i64Matrix mNodeIdxs, mFeatureIdxs;
        sbMatrix mNodes, mNodeNames, mFeatureNames, mFeatures, mCurLabels, mFinalLabel;
        static const u64 mBlockSize = 80;
        static const u64
            mROffset = mBlockSize / 8 * 1,
            mLOffset = mBlockSize / 8 * 2,
            mIsDummyOffset = mBlockSize / 8 * 3,
            mThresholdOffset = mBlockSize / 8 * 3 + 1;

        u64 mNumTrees = -1,
            mFeatureOffset = -1,
            mLabelOffset = -1,
            mFeatureCount = -1,
            mDepth = -1,
            mFeatureBitCount = -1,
            mNumLabels = -1, mDIdx, mCmpDone = 0;

        std::vector<sbMatrix> mFreatureKey, mNodeKey;

        oc::BetaCircuit mLowMCCir, mCmpCir;
        void initCmpCir();


        Sh3Task shuffleNodes(Sh3Task dep);
        Sh3Task shuffle(Sh3Task dep, sbMatrix& vals);

        void sampleKeys();
        i64 featureMap(i64) { return 0; };
        i64 nodeMap(i64) { return 0; };

        Sh3Task computeNodeNames(Sh3Task dep);

        Sh3Task computeFeatureNames(Sh3Task dep);




        Sh3Task traversTree(Sh3Task dep);
        Sh3Task getFeatures(Sh3Task dep);
        Sh3Task compare(Sh3Task dep);
        Sh3Task updateLabel(Sh3Task dep);
        Sh3Task vote(Sh3Task dep);

        //Sh3Task nextRound(Sh3Task dep);

        sbMatrix mTempNextFeatures;
        sbMatrix mTempNodeIdxs;

        std::vector<Sh3Task> mTasks;

    };



    namespace tests
    {
        void Sparse_shuffle_test(const oc::CLP& cmd);
        void Sparse_NodeNames_test(const oc::CLP& cmd);
        void Sparse_traversTree_test(const oc::CLP& cmd);


    }
}
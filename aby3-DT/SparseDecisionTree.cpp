#include "SparseDecisionTree.h"
#include "aby3-DB/OblvPermutation.h"
#include "cryptoTools/Network/IOService.h"
#include "aby3_tests/testUtils.h"
#include "aby3-DB/LowMC.h"

namespace aby3
{




    void SparseDecisionForest::init(
        u64 depth,
        std::vector<u32> treeStartIdx,
        sbMatrix& nodes,
        u64 featureBitCount,
        u64 numLabels,
        Sh3Runtime& rt,
        Sh3ShareGen&& gen)
    {
        mDepth = depth;
        mTreeStartIdx = std::move(treeStartIdx);
        mNumTrees = mTreeStartIdx.size() - 1;
        mNodes = nodes;
        mFeatureBitCount = featureBitCount;
        mNumLabels = numLabels;
        mLabelOffset = mThresholdOffset + (mFeatureBitCount + 7) / 8;

        mGen = std::move(gen);
        mPrng.SetSeed(mGen.mPrivPrng.get());

        mComparitor = Comparitor::Eq;
    }

    Sh3Task SparseDecisionForest::evaluate(Sh3Task dep, sbMatrix& features)
    {
        mFeatures = features;
        
        setTimePoint("start");
        //sampleKeys();
        //setTimePoint("sampleKeys");



        computeNodeNames(dep.getRuntime());
        computeFeatureNames(dep.getRuntime()).get();
        //setTimePoint("computeFeatureNames");
        setTimePoint("compute Names");
        dep.getRuntime().cancelTasks();
        

        shuffleNodes(dep.getRuntime()).get();
        dep.getRuntime().cancelTasks();

        setTimePoint("suffle");
        

        return traversTree(dep.getRuntime());
    }


    Sh3Task SparseDecisionForest::traversTree(Sh3Task dep)
    {
        mNodeIdxs.resize(mTreeStartIdx.size() - 1, (mBlockSize + 63) / 64);
        for (u64 i = 0; i < mNodeIdxs.rows(); ++i)
            mNodeIdxs(i, 0) = mTreeStartIdx[i];

        for (u8 i = 0; i < mDepth; i++)
        {
            //if(dep.getRuntime().mPartyIdx == 0)
            //    dep.getRuntime().mPrint = true;

            char c;
            dep.getRuntime().mComm.mNext.send(c);
            dep.getRuntime().mComm.mPrev.send(c);
            dep.getRuntime().mComm.mNext.recv(c);
            dep.getRuntime().mComm.mPrev.recv(c);

            getFeatures(dep.getRuntime()).get();
            //setTimePoint("getFeatures " + std::to_string(i));

            dep.getRuntime().cancelTasks();

            updateLabel(dep.getRuntime());
            compare(dep.getRuntime()).get();

            dep.getRuntime().cancelTasks();
            //setTimePoint("compare " +std::to_string(i));

        }
        getFeatures(dep.getRuntime()).get();
        dep.getRuntime().cancelTasks();
        updateLabel(dep.getRuntime()).get();
        dep.getRuntime().cancelTasks();
        //setTimePoint("getFeatures & update ");

        mConv.init(dep.getRuntime(), mGen);
        vote(dep.getRuntime()).get();
        dep.getRuntime().cancelTasks();

        setTimePoint("travers, done ");

        return dep;
    }
    Sh3Task SparseDecisionForest::shuffleNodes(Sh3Task dep)
    {
        std::array<oc::MatrixView<u8>, 2> views;
        views[0] = oc::MatrixView<u8>((u8*)mNodes.mShares[0].data(), mNodes.rows(), mNodes.i64Cols() * sizeof(u64));
        views[1] = oc::MatrixView<u8>((u8*)mNodes.mShares[1].data(), mNodes.rows(), mNodes.i64Cols() * sizeof(u64));
        return shuffle(dep, views);
    }

    Sh3Task SparseDecisionForest::shuffle(Sh3Task dep, std::array<oc::MatrixView<u8>,2> vals)
    {
        return dep.then([this, vals](CommPkg& comm, Sh3Task self) {
            oc::OblvPermutation op;
            switch (self.getRuntime().mPartyIdx)
            {
            case 0:
            {
                for (u64 i = 0; i < mTreeStartIdx.size() - 1; ++i)
                {
                    auto s = mTreeStartIdx[i] + 1;
                    auto e = mTreeStartIdx[i + 1];
                    auto n = e - s;
                    std::vector<u32> perm(n);

                    for (u64 j = 0; j < n; j++)
                        perm[j] = j;

                    for (u64 j = 0; j < n; j++)
                    {
                        auto idx = mPrng.get<u32>() % (n - j);
                        std::swap(perm[j], perm[idx]);
                    }


                    auto ptr = &vals[0](s, 0);
                    oc::MatrixView<u8> share0((u8*)ptr, n, vals[0].cols());

                    op.program(comm.mPrev, comm.mNext, perm, mPrng, share0, "", oc::OutputType::Overwrite);
                    op.program(comm.mNext, comm.mPrev, perm, mPrng, share0, "", oc::OutputType::Additive);
                    comm.mNext.asyncSend(std::move(share0));

                    ptr = &vals[1](s, 0);
                    oc::MatrixView<u8> share1((u8*)ptr, n, vals[0].cols());
                    auto f1 = comm.mPrev.asyncRecv(share1);

                    self.then([f1 = std::move(f1)](CommPkg& comm, Sh3Task _)mutable{
                        f1.get();
                    });

                }

                break;
            }
            case 1:
            case 2:
            {
                for (u64 i = 0; i < mTreeStartIdx.size() - 1; ++i)
                {
                    auto s = mTreeStartIdx[i] + 1;
                    auto e = mTreeStartIdx[i + 1];
                    auto n = e - s;
                    std::vector<u32> perm(n);

                    for (u64 j = 0; j < n; j++)
                        perm[j] = j;

                    auto ptr = &vals[0](s, 0);
                    oc::MatrixView<u8> share0((u8*)ptr, n, vals[0].cols());


                    oc::PRNG* prng = nullptr;
                    oc::Channel* prog, * recv;
                    if (self.getRuntime().mPartyIdx == 1)
                    {
                        auto s0 = vals[0].data();
                        auto s1 = vals[1].data();
                        for (u64 j = s; j < e; j++)
                        {
                            s0[j] ^= s1[j];
                        }
                        prng = &mGen.mNextCommon;
                        prog = &comm.mPrev;
                        recv = &comm.mNext;
                    }
                    else
                    {
                        prng = &mGen.mPrevCommon;
                        prog = &comm.mNext;
                        recv = &comm.mPrev;
                    }

                    for (u64 j = 0; j < n; j++)
                    {
                        auto idx = prng->get<u32>() % (n - j);
                        std::swap(perm[j], perm[idx]);

                        auto pp = share0[j].data();
                        std::swap_ranges(pp, pp + share0.cols(), share0[idx].data());
                    }

                    op.send(*prog, *recv, share0, "");
                    auto f0 = op.asyncRecv(*prog, *recv, share0, n, "");
                    comm.mNext.asyncSend(share0);

                    ptr = &vals[1](s, 0);
                    oc::MatrixView<u8> share1((u8*)ptr, n, vals[0].cols());
                    auto f1 = comm.mPrev.asyncRecv(share1);

                    self.then([
                        f0 = std::move(f0),
                            f1 = std::move(f1)](CommPkg& comm, Sh3Task _)mutable{
                            f0.get();
                            f1.get();
                        });
                }
                break;
            }
            default:
                throw RTE_LOC;
            }
            }).getClosure();
    }

    //void SparseDecisionForest::sampleKeys()
    //{

    //    //const auto rounds = 13;
    //    //const auto sboxCount = 14;
    //    //const auto dataComplex = 30;
    //    //const auto keySize = 128;

    //    //if (mLowMCCir.mGates.size() == 0)
    //    //{
    //    //    std::stringstream filename;
    //    //    filename << "./.lowMCCircuit"
    //    //        << "_b" << mBlockSize
    //    //        << "_r" << rounds
    //    //        << "_d" << dataComplex
    //    //        << "_k" << keySize
    //    //        << ".bin";

    //    //    std::ifstream in;
    //    //    in.open(filename.str(), std::ios::in | std::ios::binary);

    //    //    if (in.is_open() == false)
    //    //    {
    //    //        oc::LowMC2<sboxCount, mBlockSize, keySize, rounds> cipher1(false, 1);
    //    //        cipher1.to_enc_circuit(mLowMCCir);

    //    //        std::ofstream out;
    //    //        out.open(filename.str(), std::ios::trunc | std::ios::out | std::ios::binary);
    //    //        mLowMCCir.levelByAndDepth();

    //    //        mLowMCCir.writeBin(out);
    //    //    }
    //    //    else
    //    //    {
    //    //        mLowMCCir.readBin(in);
    //    //    }
    //    //}

    //    //mFreatureKey.resize(rounds + 1);
    //    //mNodeKey.resize(rounds + 1);
    //    //for (auto& f : mFreatureKey)
    //    //{
    //    //    f.resize(1, mBlockSize);
    //    //    mGen.getRandBinaryShare(f);
    //    //}
    //    //for (auto& f : mNodeKey)
    //    //{
    //    //    f.resize(1, mBlockSize);
    //    //    mGen.getRandBinaryShare(f);
    //    //}

    //}

    Sh3Task SparseDecisionForest::computeNodeNames(Sh3Task dep)
    {
        mNodeNames.resize(mNodes.rows() * 2, mBlockSize/8);
        return shuffle(dep, { mNodeNames, mNodeNames });

        //return dep.then([this](Sh3Task self) {
        //    auto bs = mBlockSize / 8;
        //    for (u64 b = 0; b < 2; ++b)
        //    {
        //        auto sPtr = mNodes.mShares[b].data();
        //        auto sStep = mNodes.i64Cols();
        //        auto dPtr = mNodeNames.mShares[b].data();
        //        auto dStep = mNodeNames.i64Cols();
        //        for (u64 j = 0; j < mNodes.rows(); j++)
        //        {

        //            auto r = ((u8*)sPtr) + mROffset;
        //            auto l = ((u8*)sPtr) + mLOffset;

        //            std::memcpy(mNodeNames.mShares[b][2 * j + 0].data(), r, bs); dPtr += dStep;
        //            std::memcpy(mNodeNames.mShares[b][2 * j + 1].data(), l, bs); dPtr += dStep;

        //            sPtr += sStep;
        //        }
        //    }

        //    mBin.setCir(&mLowMCCir, mNodeNames.rows());

        //    mBin.setInput(0, mNodeNames);
        //    for (u64 i = 0; i < mNodeKey.size(); ++i)
        //        mBin.setReplicatedInput(i + 1, mNodeKey[i]);

        //    mBin.asyncEvaluate(self).then([this](Sh3Task self) {
        //        mBin.getOutput(0, mNodeNames);
        //        });
        //    }).getClosure();
    }

    Sh3Task SparseDecisionForest::computeFeatureNames(Sh3Task dep)
    {
        return dep.then([this](CommPkg& comm, Sh3Task self) {

            mFeatureNames.resize(mNodes.rows(), mBlockSize / 8);

            switch (self.getRuntime().mPartyIdx)
            {
            case 0:
            {
                oc::OblvSwitchNet::Program prog;
                prog.init(mNodes.rows(), mNodes.rows());
                for (u64 i = 0; i < mNodes.rows(); i++)
                {
                    prog.addSwitch(i, i);
                }
                prog.finalize();

                mSNet.program(comm.mPrev, comm.mNext, prog, mPrng, mFeatureNames);
                break;
            }
            case 1:
            {
                mSNet.sendRecv(comm.mPrev, comm.mNext, mFeatureNames, mFeatureNames);
                break;
            }
            case 2:
            {
                mSNet.help(comm.mNext, comm.mPrev, mPrng, mNodes.rows(), mNodes.rows(), mFeatureNames.cols());
                break;
            }
            default:
                break;
            }
            //if (mFeatureNames.rows() != mFeatureCount)
            //    throw RTE_LOC;

            //auto bs = mBlockSize / 8;
            //for (u64 b = 0; b < 2; ++b)
            //{
            //    auto sPtr = mNodes.mShares[b].data() + mFeatureOffset;
            //    auto sStep = mNodes.i64Cols();
            //    auto dPtr = mFeatureNames.mShares[b].data();
            //    auto dStep = mFeatureNames.i64Cols();
            //    for (u64 j = 0; j < mNodes.rows(); j++)
            //    {
            //        std::memcpy(dPtr, sPtr, bs);

            //        dPtr += dStep;
            //        sPtr += sStep;
            //    }
            //}

            //mBin2.setCir(&mLowMCCir, mFeatureNames.rows());

            //mBin2.setInput(0, mFeatureNames);
            //for (u64 i = 0; i < mNodeKey.size(); ++i)
            //    mBin2.setReplicatedInput(i + 1, mNodeKey[i]);

            //mBin2.asyncEvaluate(self).then([this](Sh3Task self) {
            //    mBin2.getOutput(0, mFeatureNames);
            //    });
            }).getClosure();
    }



        //return dep.then([this](Sh3Task dep) {



        //    }).getClosure();
    //void extract(const sbMatrix& src, sbMatrix& dest, span<i64> locs, u64 size, u64 offset = 0)
    //{

    //}
    void extract(const oc::Matrix<u8>& src, sbMatrix& dest, i64Matrix locs, u64 size, u64 offset = 0)
    {
        //auto l = span<i64>(locs.data(), locs.size());
        //extract(src, dest, l, size, offset);

        if (dest.rows() != locs.rows())
            throw RTE_LOC;
        if (oc::roundUpTo(dest.bitCount(), 8) / 8 != size)
            throw RTE_LOC;
        if (src.cols() < size + offset)
            throw RTE_LOC;

        for (u64 b = 0; b < 2; b++)
        {
            for (u64 i = 0; i < locs.rows(); ++i)
            {
                auto destPtr = dest.mShares[b][i].data();
                auto srcPtr = ((u8*)src[locs(i, 0)].data()) + offset;
                std::memcpy(destPtr, srcPtr, size);
            }
        }
    }

    void extract(const sbMatrix& src, sbMatrix& dest, i64Matrix locs, u64 size, u64 offset = 0)
    {
        //auto l = span<i64>(locs.data(), locs.size());
        //extract(src, dest, l, size, offset);

        if (dest.rows() != locs.rows())
            throw RTE_LOC;
        if (oc::roundUpTo(dest.bitCount(), 8) / 8 != size)
            throw RTE_LOC;
        if (oc::roundUpTo(src.bitCount(), 8) / 8 < size + offset)
            throw RTE_LOC;

        for (u64 b = 0; b < 2; b++)
        {
            for (u64 i = 0; i < locs.rows(); ++i)
            {
                auto destPtr = dest.mShares[b][i].data();
                auto srcPtr = ((u8*)src.mShares[b][locs(i,0)].data()) + offset;
                std::memcpy(destPtr, srcPtr, size);
            }
        }
    }

    Sh3Task SparseDecisionForest::getFeatures(Sh3Task dep)
    {
        dep =  dep.then([this](Sh3Task self) {
            mFeatureIdxs.resize(mTreeStartIdx.size() - 1, (mBlockSize+63)/64);
            mTempNextFeatures.resize(mNumTrees, mBlockSize);

            auto bs = mBlockSize / 8;
            extract(mFeatureNames, mTempNextFeatures, mNodeIdxs, bs);

            }, "getFeatures");

        dep = mEnc.revealAll(dep, mTempNextFeatures, mFeatureIdxs);
        
        return dep.then([this](Sh3Task) {
            for (u64 i = 0; i < mNumTrees; i++)
                mFeatureIdxs(i, 0) = featureMap(mFeatureIdxs(i, 0));
            }, "revealFeatures");
    }

    Sh3Task SparseDecisionForest::compare(Sh3Task dep)
    {
        return dep.then([this](Sh3Task self) {

            sbMatrix curFeatures(mNumTrees, mFeatureBitCount);
            sbMatrix curNodes(mNumTrees, mBlockSize);
            sbMatrix curLeft(mNumTrees, mBlockSize);
            sbMatrix curRight(mNumTrees, mBlockSize);
            auto fSize = (mFeatureBitCount + 7) / 8;
            auto nSize = (mBlockSize + 7) / 8;

            extract(mFeatures, curFeatures, mFeatureIdxs, fSize);
            extract(mNodes, curNodes, mNodeIdxs, nSize, mThresholdOffset);
            extract(mNodes, curLeft, mNodeIdxs, nSize, mLOffset);
            extract(mNodes, curRight, mNodeIdxs, nSize, mROffset);

            if (mCmpCir.mGates.size() == 0)
                initCmpCir();

            mBin2.setCir(&mCmpCir, mNumTrees);
            mBin2.setInput(0, curFeatures);
            mBin2.setInput(1, curNodes);
            mBin2.setInput(2, curLeft);
            mBin2.setInput(3, curRight);
            mBin2.asyncEvaluate(self).then([this](Sh3Task self) {
                mTempNodeIdxs.resize(mNumTrees, mBlockSize);
                mBin2.getOutput(0, mTempNodeIdxs);
                mEnc.revealAll(self, mTempNodeIdxs, mNodeIdxs).then([this](Sh3Task) {

                    mCmpDone++;
                    for (u64 i = 0; i < mNodeIdxs.rows(); i++)
                    {
                        mNodeIdxs(i, 0) = nodeMap(mNodeIdxs(i, 0));
                    }
                    });
                });
            //TreeBase::compare(self, curFeatures, curNodes, 1, mCmp, Comparitor::Eq);

            }).getClosure();
    }


    void SparseDecisionForest::initCmpCir()
    {
        mCmpCir = {};

        oc::BetaBundle features(mFeatureBitCount);
        oc::BetaBundle nodes(mBlockSize);
        oc::BetaBundle left(mBlockSize), right(mBlockSize), out(mBlockSize);

        mCmpCir.addInputBundle(features);
        mCmpCir.addInputBundle(nodes);
        mCmpCir.addInputBundle(left);
        mCmpCir.addInputBundle(right);
        mCmpCir.addOutputBundle(out);

        oc::BetaBundle cmp(1), temp(1);
        mCmpCir.addTempWireBundle(cmp);
        mCmpCir.addTempWireBundle(temp);

        if (mComparitor == Comparitor::Eq)
        {
            mLib.int_int_eq_build(mCmpCir, features, nodes, cmp);
            mLib.int_int_multiplex_build(mCmpCir, right, left, cmp, out, temp);
        }
        else
            throw RTE_LOC;
    }

    Sh3Task SparseDecisionForest::updateLabel(Sh3Task dep)
    {
        return dep.then([this](Sh3Task self) {
            

            if (mCurLabels.rows() == 0)
            {
                auto size = (mNumLabels + 7) / 8;
                mCurLabels.resize(mNumTrees, mNumLabels);
                extract(mNodes, mCurLabels, mNodeIdxs, size, mLabelOffset);
            }
            else
            {
                sbMatrix curIsDummyBits(mNumTrees, 1);
                extract(mNodes, curIsDummyBits, mNodeIdxs, 1, mIsDummyOffset);

                sbMatrix newLabels(mNumTrees, mNumLabels);
                auto size = (mNumLabels + 7) / 8;
                extract(mNodes, newLabels, mNodeIdxs, size, mLabelOffset);

                auto cir = mLib.int_int_multiplex(mNumLabels);
                mBin.setCir(cir, mNumTrees);

                mBin.setInput(0, mCurLabels);
                mBin.setInput(1, newLabels);
                mBin.setInput(2, curIsDummyBits);
                mBin.asyncEvaluate(self).then([this](Sh3Task) {
                    mBin.getOutput(0, mCurLabels);
                    });

            }

            }).getClosure();
    }

    Sh3Task SparseDecisionForest::vote(Sh3Task dep)
    {
        return TreeBase::vote(dep, mCurLabels, mFinalLabel);
    }



    namespace tests
    {

        void Sparse_shuffle_test(const oc::CLP& cmd)
        {
            using namespace oc;
            IOService ios;
            std::array<Sh3Runtime, 3> rts;
            makeRuntimes(rts, ios);
            PRNG prng(oc::toBlock(cmd.getOr("seed", 0)));
            SparseDecisionForest trees[3];

            u64 cols = 40;
            std::vector<u32> treeStartIdx{ 0, 10, 20 };

            i64Matrix nodes(treeStartIdx.back(), cols);


            for (u64 i = 0; i < nodes.rows(); i++)
            {
                for (u64 j = 0; j < nodes.cols(); j++)
                {
                    nodes(i, j) = i & 1;
                }
            }

            sbMatrix n0, n1, n2;
            share(pack(nodes), cols, n0, n1, n2, prng);
            trees[0].mNodes = n0;
            trees[1].mNodes = n1;
            trees[2].mNodes = n2;
            trees[0].mTreeStartIdx = treeStartIdx;
            trees[1].mTreeStartIdx = treeStartIdx;
            trees[2].mTreeStartIdx = treeStartIdx;
            auto gens = makeShareGens();
            trees[0].mGen = std::move(gens[0]);
            trees[1].mGen = std::move(gens[1]);
            trees[2].mGen = std::move(gens[2]);
            trees[0].mPrng.SetSeed(toBlock(1));
            trees[1].mPrng.SetSeed(toBlock(2));
            trees[2].mPrng.SetSeed(toBlock(2));

            auto t0 = trees[0].shuffleNodes(rts[0]);
            auto t1 = trees[1].shuffleNodes(rts[1]);
            auto t2 = trees[2].shuffleNodes(rts[2]);

            run(t0, t1, t2);


        }

        void Sparse_NodeNames_test(const oc::CLP& cmd)
        {
            using namespace oc;
            IOService ios;
            std::array<Sh3Runtime, 3> rts;
            makeRuntimes(rts, ios);
            PRNG prng(oc::toBlock(cmd.getOr("seed", 0)));
            SparseDecisionForest trees[3];

            u64 blockSize = 80;
            u64 blockSize8 = blockSize / 8;
            u64 cols = blockSize * 6;
            std::vector<u32> treeStartIdx{ 0, 10, 20 };

            i64Matrix nodes(treeStartIdx.back(), cols);


            for (u64 i = 0; i < nodes.rows(); i++)
            {
                for (u64 j = 0; j < nodes.cols(); j++)
                {
                    nodes(i, j) = i & 1;
                }
            }

            sbMatrix n0, n1, n2;
            share(pack(nodes), cols, n0, n1, n2, prng);

            auto gens = makeShareGens();
            trees[0].mGen = std::move(gens[0]);
            trees[1].mGen = std::move(gens[1]);
            trees[2].mGen = std::move(gens[2]);
            trees[0].mPrng.SetSeed(toBlock(1));
            trees[1].mPrng.SetSeed(toBlock(2));
            trees[2].mPrng.SetSeed(toBlock(2));

            //trees[0].sampleKeys();
            //trees[1].sampleKeys();
            //trees[2].sampleKeys();

            trees[0].mFeatureOffset = blockSize8 * 3;
            trees[1].mFeatureOffset = blockSize8 * 3;
            trees[2].mFeatureOffset = blockSize8 * 3;


            trees[0].mNodes = n0;
            trees[1].mNodes = n1;
            trees[2].mNodes = n2;
            trees[0].mTreeStartIdx = treeStartIdx;
            trees[1].mTreeStartIdx = treeStartIdx;
            trees[2].mTreeStartIdx = treeStartIdx;

            auto t0 = trees[0].computeNodeNames(rts[0]);
            auto t1 = trees[1].computeNodeNames(rts[1]);
            auto t2 = trees[2].computeNodeNames(rts[2]);

            run(t0, t1, t2);

            t0 = trees[0].computeFeatureNames(rts[0]);
            t1 = trees[1].computeFeatureNames(rts[1]);
            t2 = trees[2].computeFeatureNames(rts[2]);

            run(t0, t1, t2);


        }
        
        void Sparse_traversTree_test(const oc::CLP& cmd)
        {

            oc::Timer timer;
            timer.setTimePoint("...");
            using namespace oc;
            IOService ios;
            std::array<Sh3Runtime, 3> rts;
            makeRuntimes(rts, ios);
            PRNG prng(oc::toBlock(cmd.getOr("seed", 0)));
            SparseDecisionForest trees[3];
            trees[0].setTimer(timer);
            u64 numNodes = 100;
            u64 numTree = 1000;
            u64 featureCount = 100;
            u64 depth = 16;

            u64 featureBitCount = 1;
            u64 thresholdBitCount = 1,
                isDummySize = 1,
                numLabels = 10;

            u64 cols
                = oc::roundUpTo(thresholdBitCount, 8)
                + oc::roundUpTo(featureBitCount, 8)
                + oc::roundUpTo(isDummySize, 8)
                + oc::roundUpTo(numLabels, 8)
                + SparseDecisionForest::mBlockSize * 4;

            std::vector<u32> treeStartIdx(numTree + 1);
            for (u64 i = 0; i < treeStartIdx.size(); i++)
                treeStartIdx[i] = numTree * i;

            i64Matrix nodes(treeStartIdx.back(), (cols +63) / sizeof(i64));
            i64Matrix features(featureCount, (featureBitCount+63) / sizeof(i64));

            prng.get(nodes.data(), nodes.size());
            prng.get(features.data(), features.size());

            sbMatrix n[3], f[3];
            share(nodes, cols, n[0], n[1], n[2], prng);
            share(features, featureBitCount, f[0], f[1], f[2], prng);

            auto gens = makeShareGens();
            //Sh3Task t[3];
            for (auto i : { 0, 1, 2 })
                trees[i].init(depth, treeStartIdx, n[i], featureBitCount, numLabels, rts[i], std::move(gens[i]));

            timer.setTimePoint("init");


            std::function<void()> fn[3];
            for (auto i : { 0, 1, 2 })
                fn[i] = [&,i]() { trees[i].evaluate(rts[i], f[i]); };

            run(fn[0], fn[1], fn[2]);
            //run(t[0], t[1], t[2]);

            std::cout << timer << std::endl;
            std::cout <<"num cmp's "<< trees[0].mCmpDone << std::endl;
        }
    }
}

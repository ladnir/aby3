#include "SparseDecisionTree.h"
#include "aby3-DB/OblvPermutation.h"
#include "cryptoTools/Network/IOService.h"
#include "aby3_tests/testUtils.h"
#include "aby3-DB/LowMC.h"

namespace aby3
{


    void aby3::SparseDecisionForest::init(
        u64 depth,
        u64 numTrees,
        u64 featureCount,
        u64 featureBitCount,
        u64 nodeBitCount,
        u64 numLabels,
        Sh3Runtime& rt,
        Sh3ShareGen& gen)
    {

        rt;
    }

    Sh3Task SparseDecisionForest::shuffleNodes(Sh3Task dep)
    {
        return dep.then([this](CommPkg& comm, Sh3Task self) {
                oc::OblvPermutation op;
            switch (self.getRuntime().mPartyIdx)
            {
            case 0:
            {
                for (u64 i = 0; i < mTreeStartIdx.size() - 1; ++i)
                {
                    auto s = mTreeStartIdx[i];
                    auto e = mTreeStartIdx[i+1];
                    auto n = e - s;
                    std::vector<u32> perm(n);

                    for (u64 j = 0; j < n; j++)
                        perm[j] = j;

                    for (u64 j = 0; j < n; j++)
                    {
                        auto idx = mPrng.get<u32>() % (n - j);
                        std::swap(perm[j], perm[idx]);
                    }


                    auto ptr = &mNodes.mShares[0](s, 0);
                    oc::MatrixView<u8> share0((u8*)ptr, n, mNodes.i64Cols() * sizeof(i64));

                    op.program(comm.mPrev, comm.mNext, perm, mPrng, share0, "", oc::OutputType::Overwrite);
                    op.program(comm.mNext, comm.mPrev, perm, mPrng, share0, "", oc::OutputType::Additive);
                    comm.mNext.asyncSend(std::move(share0));

                    ptr = &mNodes.mShares[1](s, 0);
                    oc::MatrixView<u8> share1((u8*)ptr, n, mNodes.i64Cols() * sizeof(i64));
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
                    auto s = mTreeStartIdx[i];
                    auto e = mTreeStartIdx[i + 1];
                    auto n = e - s;
                    std::vector<u32> perm(n);

                    for (u64 j = 0; j < n; j++)
                        perm[j] = j;

                    auto ptr = &mNodes.mShares[0](s, 0);
                    oc::MatrixView<u8> share0((u8*)ptr, n, mNodes.i64Cols() * sizeof(i64));


                    oc::PRNG* prng = nullptr;
                    oc::Channel* prog, * recv;
                    if (self.getRuntime().mPartyIdx == 1)
                    {
                        for (u64 j = 0; j < mNodes.mShares[0].size(); j++)
                        {
                            mNodes.mShares[0](j) = mNodes.mShares[0](j) ^ mNodes.mShares[1](j);
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

                    op.send(*prog, *recv, share0,"");
                    auto f0 = op.asyncRecv(*prog, *recv, share0,n,"");
                    comm.mNext.asyncSend(share0);

                    ptr = &mNodes.mShares[1](s, 0);
                    oc::MatrixView<u8> share1((u8*)ptr, n, mNodes.i64Cols() * sizeof(i64));
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

    void SparseDecisionForest::sampleKeys()
    {

        const auto blockSize = 80;
        const auto rounds = 13;
        const auto sboxCount = 14;
        const auto dataComplex = 30;
        const auto keySize = 128;

        if (mLowMCCir.mGates.size() == 0)
        {
            std::stringstream filename;
            filename << "./.lowMCCircuit"
                << "_b" << blockSize
                << "_r" << rounds
                << "_d" << dataComplex
                << "_k" << keySize
                << ".bin";

            std::ifstream in;
            in.open(filename.str(), std::ios::in | std::ios::binary);

            if (in.is_open() == false)
            {
                oc::LowMC2<sboxCount, blockSize, keySize, rounds> cipher1(false, 1);
                cipher1.to_enc_circuit(mLowMCCir);

                std::ofstream out;
                out.open(filename.str(), std::ios::trunc | std::ios::out | std::ios::binary);
                mLowMCCir.levelByAndDepth();

                mLowMCCir.writeBin(out);
            }
            else
            {
                mLowMCCir.readBin(in);
            }
            mBlockSize = blockSize;
        }

        mFreatureKey.resize(rounds + 1);
        mNodeKey.resize(rounds + 1);
        for (auto& f : mFreatureKey)
        {
            f.resize(1, blockSize);
            mGen.getRandBinaryShare(f);
        }
        for (auto& f : mNodeKey)
        {
            f.resize(1, blockSize);
            mGen.getRandBinaryShare(f);
        }

    }

    Sh3Task SparseDecisionForest::computeNodeNames(Sh3Task dep)
    {
        return dep.then([this](Sh3Task self) {
            
            mNodeNames.resize(mNodes.rows() * 2, mBlockSize);
            auto bs = mBlockSize / 8;
            for (u64 b = 0; b < 2; ++b)
            {
                auto sPtr = mNodes.mShares[b].data();
                auto sStep = mNodes.i64Cols();
                auto dPtr = mNodeNames.mShares[b].data();
                auto dStep = mNodeNames.i64Cols();
                for (u64 j = 0; j < mNodes.rows(); j++)
                {

                    auto r = ((u8*)sPtr) + mROffset;
                    auto l = ((u8*)sPtr) + mLOffset;
                    
                    std::memcpy(mNodeNames.mShares[b][2 * j + 0].data(), r, bs); dPtr += dStep;
                    std::memcpy(mNodeNames.mShares[b][2 * j + 1].data(), l, bs); dPtr += dStep;

                    sPtr += sStep;
                }
            }
            
            mBin.setCir(&mLowMCCir, mNodeNames.rows());

            mBin.setInput(0, mNodeNames);
            for (u64 i = 0; i < mNodeKey.size(); ++i)
                mBin.setReplicatedInput(i + 1, mNodeKey[i]);

            mBin.asyncEvaluate(self).then([this](Sh3Task self) {
                mBin.getOutput(0, mNodeNames);
                });
        }).getClosure();
    }

    Sh3Task SparseDecisionForest::computeFeatureNames(Sh3Task dep)
    {
        return dep.then([this](Sh3Task self) {

            mFeatureNames.resize(mNodes.rows(), mBlockSize);
            //if (mFeatureNames.rows() != mFeatureCount)
            //    throw RTE_LOC;

            auto bs = mBlockSize / 8;
            for (u64 b = 0; b < 2; ++b)
            {
                auto sPtr = mNodes.mShares[b].data() + mFeatureOffset;
                auto sStep = mNodes.i64Cols();
                auto dPtr = mFeatureNames.mShares[b].data();
                auto dStep = mFeatureNames.i64Cols();
                for (u64 j = 0; j < mNodes.rows(); j++)
                {
                    std::memcpy(dPtr, sPtr, bs);

                    dPtr += dStep;
                    sPtr += sStep;
                }
            }

            mBin.setCir(&mLowMCCir, mFeatureNames.rows());

            mBin.setInput(0, mFeatureNames);
            for (u64 i = 0; i < mNodeKey.size(); ++i)
                mBin.setReplicatedInput(i + 1, mNodeKey[i]);

            mBin.asyncEvaluate(self).then([this](Sh3Task self) {
                mBin.getOutput(0, mFeatureNames);
                });
            }).getClosure();
    }

    Sh3Task SparseDecisionForest::traversTree(Sh3Task dep)
    {
        return {};
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

            trees[0].sampleKeys();
            trees[1].sampleKeys();
            trees[2].sampleKeys();

            trees[0].mBlockSize = blockSize;
            trees[0].mROffset = blockSize8 * 1;
            trees[0].mLOffset = blockSize8 * 2;
            trees[0].mFeatureOffset = blockSize8 * 3;
            trees[1].mBlockSize = blockSize;
            trees[1].mROffset = blockSize8 * 1;
            trees[1].mLOffset = blockSize8 * 2;
            trees[1].mFeatureOffset = blockSize8 * 3;
            trees[2].mBlockSize = blockSize;
            trees[2].mROffset = blockSize8 * 1;
            trees[2].mLOffset = blockSize8 * 2;
            trees[2].mFeatureOffset = blockSize8 * 3;


            trees[0].mNodes = n0;
            trees[1].mNodes = n1;
            trees[2].mNodes = n2;


            auto t0 = trees[0].computeNodeNames(rts[0]);
            auto t1 = trees[1].computeNodeNames(rts[1]);
            auto t2 = trees[2].computeNodeNames(rts[2]);

            run(t0, t1, t2);

            t0 = trees[0].computeFeatureNames(rts[0]);
            t1 = trees[1].computeFeatureNames(rts[1]);
            t2 = trees[2].computeFeatureNames(rts[2]);

            run(t0, t1, t2);


        }
    }
}

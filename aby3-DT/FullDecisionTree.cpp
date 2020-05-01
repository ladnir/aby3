#include "FullDecisionTree.h"
//#include "aby3-DB/"
#include "cryptoTools/Common/BitIterator.h"
#include "cryptoTools/Network/IOService.h"
#include "cryptoTools/Network/Session.h"
#include "aby3/sh3/Sh3Encryptor.h"
#include "aby3_tests/testUtils.h"

namespace aby3
{

    void FullDecisionTree::init(
        u64 depth,
        u64 numTrees,
        u64 featureCount,
        u64 featureBitCount,
        u64 nodeBitCount,
        u64 numLabels,
        Sh3Runtime& rt, Sh3ShareGen& gen, bool unitTest)
    {

        mUnitTest = unitTest;

        if (depth < 1)
            throw std::runtime_error("d must at least be 1");
        if (featureCount == 0)
            throw std::runtime_error("logic error");
        if (featureBitCount == 0)
            throw std::runtime_error("logic error");

        mDepth = depth;
        mNumTrees = numTrees;
        mFeatureCount = featureCount;
        mFeatureBitCount = featureBitCount;
        mNodeBitCount = nodeBitCount;
        mNumLabels = numLabels;

        mConv.init(rt, gen);

    }

    Sh3Task FullDecisionTree::evaluate(Sh3Task dep,
        const sbMatrix& nodes,
        const sbMatrix& features,
        const sbMatrix& mapping,
        const sbMatrix& labels,
        sbMatrix& output)
    {

        u64 nodesPerTree = (1ull << mDepth) - 1;
        u64 numLeaves = 1ull << mDepth;

        if (features.rows() != mFeatureBitCount)
            throw RTE_LOC;
        if (features.bitCount() != mFeatureCount)
            throw RTE_LOC;
        if (nodes.rows() != mNumTrees * nodesPerTree)
            throw RTE_LOC;
        if (nodes.bitCount() != mNodeBitCount)
            throw RTE_LOC;
        if (labels.rows() != mNumTrees)
            throw RTE_LOC;
        if (labels.bitCount() != numLeaves * mNumLabels)
            throw RTE_LOC;
        if (mapping.rows() != nodesPerTree * mNumTrees)
            throw RTE_LOC;
        if (mapping.bitCount() != mFeatureCount )
            throw RTE_LOC;

        struct State {
            sbMatrix mappedFeatures, cmp, y;
        };

        auto state = std::make_shared<State>();
        state->y.resize(mNumTrees, mNumLabels);
        state->mappedFeatures.resize(mNumTrees * nodesPerTree, mFeatureBitCount);
        state->cmp.resize(mNumTrees, nodesPerTree);
        output.resize(mNumLabels, 1);

        setTimePoint("start");
        dep = innerProd(dep, features, mapping, state->mappedFeatures);
        setTimePoint("mapped");
        dep = compare(dep, state->mappedFeatures, nodes, nodesPerTree, state->cmp);
        setTimePoint("compare");
        dep = reduce(dep, state->cmp, labels, mNumLabels, state->y);
        setTimePoint("reduce");
        dep = vote(dep, state->y, output).then([state](Sh3Task) {});
        setTimePoint("vote");

        return dep;
    }


    Sh3Task FullDecisionTree::reduce(
        Sh3Task dep,
        const sbMatrix& cmp,
        const sbMatrix& labels,
        u64 labelBitCount,
        sbMatrix& pred)
    {
        return dep.then([&, labelBitCount](Sh3Task self) {

            //sbMatrix cmp(numSums, nodesPerTree);
            u64 numTrees = cmp.rows();
            u64 nodesPerTree = cmp.bitCount();

            initReduceCircuit(labelBitCount);


            //mBin.enableDebug(self.getRuntime().mPartyIdx, mDebug.mPrev, mDebug.mNext);
            mBin.setCir(&mReduceCir, cmp.rows());
            mBin.setInput(0, cmp);
            mBin.setInput(1, labels);
            pred.resize(numTrees, labelBitCount);
            //mActive.resize(numSums, 1ull << (mDepth + 1));
            //mOutLabels.resize(labels.rows(), labels.bitCount());

            mBin.asyncEvaluate(self).then([&](Sh3Task self) {
                mBin.getOutput(0, pred);
                //mBin.getOutput(1, mActive);
                //mBin.getOutput(2, mOutLabels);
                }, "final-getOutput");

            }, "top-level-reduce").getClosure();
    }

    void FullDecisionTree::initReduceCircuit(u64 labelBitCount)
    {
        mReduceCir = {};

        auto str = [](auto x) -> std::string {return std::to_string(x); };

        u64 nodesPerTree = (1ull << mDepth) - 1;
        u64 numLabels = (1ull << mDepth);
        oc::BetaBundle cmp(nodesPerTree);
        oc::BetaBundle labels(numLabels * labelBitCount);

        oc::BetaBundle pred(labelBitCount);
        oc::BetaBundle active(1ull << (mDepth + 1));


        mReduceCir.addInputBundle(cmp);
        mReduceCir.addInputBundle(labels);
        mReduceCir.addOutputBundle(pred);

        mReduceCir.addTempWireBundle(active);
        mReduceCir.addConst(active[1], 1);

        for (u64 d = 0; d < mDepth; ++d)
        {
            auto parent = 1ull << d;
            auto child = 1ull << (d + 1);

            auto pEnd = child;

            while (parent != pEnd)
            {
                //active[child + 1] = active[parent]    &  cmp[parent]
                //active[child + 0] = active[parent]    & !cmp[parent]
                //                  = active[child + 1] ^  active[parent]
                mReduceCir.addGate(active[parent], cmp[parent - 1], oc::GateType::And, active[child + 1]);
                mReduceCir.addGate(active[parent], active[child + 1], oc::GateType::Xor, active[child]);

                //mReduceCir.addPrint("a[" + str(child) + "] = ");
                //mReduceCir.addPrint(active[child]);
                //mReduceCir.addPrint("\na[" + str(child+1) + "] = ");
                //mReduceCir.addPrint(active[child+1]);
                //mReduceCir.addPrint("\nc[" + str(parent) + "] = ");
                //mReduceCir.addPrint(cmp[parent-1]);
                //mReduceCir.addPrint("\numSums");

                //std::cout << "a[" << child << "] = a[" << parent << "] & ~c[" << child << "]" << std::endl;
                //std::cout << "a[" << child + 1 << "] = a[" << parent << "] & ~c[" << child + 1 << "]" << std::endl;
                child += 2;
                ++parent;
            }
        }

        u64 s = 1ull << mDepth;
        for (u64 i = 0, l = 0; i < numLabels; ++i)
        {
            for (u64 j = 0; j < labelBitCount; ++j, ++l)
            {

                //std::cout << "l[" << l << "] = a[" << s + i << "] & ~l[" << l << "]" << std::endl;
                mReduceCir.addGate(active[s + i], labels[l], oc::GateType::And, labels[l]);
            }
        }

        for (u64 j = 0; j < labelBitCount; ++j)
            mReduceCir.addGate(labels[j], labels[j + labelBitCount], oc::GateType::Xor, pred[j]);

        for (u64 i = 2; i < numLabels; ++i)
        {
            for (u64 j = 0; j < labelBitCount; ++j)
                mReduceCir.addGate(pred[j], labels[j + i * labelBitCount], oc::GateType::Xor, pred[j]);
        }

    }


    u8 parity(u64 x);



    namespace tests
    {
        i64Matrix innerProd(i64Matrix& x, i64Matrix& y)
        {
            u64 n = x.rows();
            u64 m = y.rows();
            u64 w64 = x.cols();

            if (y.cols() != w64)
                throw std::runtime_error(LOCATION);

            auto m64 = (m + 63) / 64;

            i64Matrix ret(n, m64);
            ret.setZero();
            for (u64 i = 0; i < n; ++i)
            {
                auto iter = oc::BitIterator((u8*)ret.row(i).data(), 0);

                for (u64 j = 0; j < m; ++j)
                {
                    i64 sum = 0;
                    for (u64 k = 0; k < w64; ++k)
                    {
                        sum ^= x(i, k) & y(j, k);
                    }

                    *iter++ = parity(sum);
                }
            }

            return ret;
        }



        void FullTree_innerProd_test(const osuCrypto::CLP& cmd)
        {
            using namespace oc;
            IOService ios;

            std::array<Sh3Runtime, 3> rts;
            makeRuntimes(rts, ios);

            int n = 43;
            int m = 17;
            int w = 465;

            int m64 = (m + 63) / 64;
            int w64 = (w + 63) / 64;

            PRNG prng(oc::toBlock(cmd.getOr("seed", 0)));
            FullDecisionTree trees[3];

            i64Matrix x(n, w64), y(m, w64), zz(n, m64);
            prng.get(x.data(), x.size());
            prng.get(y.data(), y.size());

            if (w % 64)
            {
                i64 mask = (1ll << (w % 64)) - 1;
                for (u64 i = 0; i < n; ++i)
                    x(i, x.cols() - 1) &= mask;
                for (u64 i = 0; i < m; ++i)
                    y(i, y.cols() - 1) &= mask;
            }

            auto z = innerProd(x, y);
            sbMatrix x0, x1, x2, y0, y1, y2, z0, z1, z2;

            share(x, w, x0, x1, x2, prng);
            share(y, w, y0, y1, y2, prng);
            auto t0 = trees[0].innerProd(rts[0], x0, y0, z0);
            auto t1 = trees[1].innerProd(rts[1], x1, y1, z1);
            auto t2 = trees[2].innerProd(rts[2], x2, y2, z2);
            run(t0, t1, t2);
            reveal(zz, z0, z1, z2);

            if (zz != z)
            {
                std::cout << "zz \n" << bitstr(zz, m) << std::endl;
                std::cout << "z \n" << bitstr(z, m) << std::endl;

                throw std::runtime_error("incorrect result. " LOCATION);
            }
        }


        void compare(i64Matrix& features, i64Matrix& nodes, i64Matrix& cmp)
        {

            for (u64 i = 0; i < features.size(); ++i)
            {
                cmp(i) = (features(i) == nodes(i));
            }
        }

        void FullTree_compare_test(const osuCrypto::CLP& cmd)
        {
            using namespace oc;
            IOService ios;
            std::array<Sh3Runtime, 3> rts;
            makeRuntimes(rts, ios);
            auto conv = makeShareGens();

            PRNG prng(oc::toBlock(cmd.getOr("seed", 0)));
            FullDecisionTree trees[3];


            u64 n = 43;
            u64 d = 3;
            u64 numFeatures = 10;
            u64 nodesPerTree = (1ull << d) - 1;
            u64 nodeBitCount = 1;
            u64 featureBitCount = 1;
            u64 numLabels = 1;

            trees[0].init(d, n, numFeatures, featureBitCount, nodeBitCount, numLabels, rts[0], conv[0]);
            trees[1].init(d, n, numFeatures, featureBitCount, nodeBitCount, numLabels, rts[1], conv[1]);
            trees[2].init(d, n, numFeatures, featureBitCount, nodeBitCount, numLabels, rts[2], conv[2]);

            i64Matrix nodes(n * nodesPerTree, nodeBitCount);
            i64Matrix mappedFeatures(n * nodesPerTree, featureBitCount);


            for (u64 i = 0; i < nodes.rows(); i++)
            {
                for (u64 j = 0; j < nodes.cols(); j++)
                {
                    nodes(i, j) = prng.getBit();
                    mappedFeatures(i, j) = prng.getBit();
                }
            }

            i64Matrix cmp(n, nodesPerTree), cmp2;
            compare(mappedFeatures, nodes, cmp);


            sbMatrix f0, f1, f2, n0, n1, n2, c0, c1, c2;

            share(pack(mappedFeatures), nodeBitCount, f0, f1, f2, prng);
            share(pack(nodes), nodeBitCount, n0, n1, n2, prng);


            auto t0 = trees[0].compare(rts[0], f0, n0, nodesPerTree, c0);
            auto t1 = trees[1].compare(rts[1], f1, n1, nodesPerTree, c1);
            auto t2 = trees[2].compare(rts[2], f2, n2, nodesPerTree, c2);

            run(t0, t1, t2);

            reveal(cmp2, c0, c1, c2);
            cmp2 = unpack(cmp2, nodesPerTree);

            if (cmp != cmp2)
            {
                std::cout << "cmp  \n" << bitstr(cmp, nodesPerTree) << std::endl;
                std::cout << "cmp2 \n" << bitstr(cmp2, nodesPerTree) << std::endl;

                throw std::runtime_error("incorrect result. " LOCATION);
            }
        }


        i64Matrix reduce(i64Matrix labels, i64Matrix cmp)
        {
            auto n = cmp.rows();
            auto nodesPerTree = cmp.cols();
            auto depth = oc::log2ceil(nodesPerTree + 1);
            auto labelBitCount = labels.cols() / (nodesPerTree + 1);

            std::vector<u64> active(n, 1);

            u64 jj = 0;

            //for (u64 d = 0; d < depth; ++d)
            //{
            //    std::cout << "d[" << d << "] ~~ ";
            //    auto w = 1ull << d;
            //    for (u64 j = 0; j < w; j++)
            //    {
            //        std::cout << " " << cmp(0, jj++);
            //    }

            //    std::cout << std::endl;
            //}

            for (u64 d = 0; d < depth; ++d)
            {
                for (u64 i = 0; i < n; ++i)
                {
                    //std::cout << "cmp["<<i<<"][" << d << "] = " << cmp(i, active[i]-1) << std::endl;
                    auto c = cmp(i, active[i] - 1);
                    //std::cout << "active[" << i << "] = 2 * " << (active[i]) <<" + " << c << std::endl;
                    active[i] = 2 * active[i] + c;
                }
            }

            auto s = 1ull << depth;
            //for (u64 i = 0; i < s; i++)
            //{
            //    std::cout << "label[0][" << i << "] = ";
            //    for (u64 j = 0; j < numLabels; j++)
            //    {
            //        std::cout <<  labels(0, i * numLabels + j) << " ";
            //    }
            //    std::cout << std::endl;
            //}

            i64Matrix ret(n, labelBitCount);
            for (u64 i = 0; i < n; i++)
            {
                auto idx = active[i] - s;
                for (u64 j = 0; j < labelBitCount; j++)
                {
                    ret(i, j) = labels(i, idx * labelBitCount + j);
                }
                //std::cout << "active[" << i << "] = " << (active[i]) <<" ~~ " << idx << std::endl
                //    << " -> " << ret.row(i) << std::endl;
            }

            return ret;
        }

        void FullTree_reduce_test(const osuCrypto::CLP& cmd)
        {
            using namespace oc;
            IOService ios;
            std::array<Sh3Runtime, 3> rts;
            makeRuntimes(rts, ios);
            auto conv = makeShareGens();

            PRNG prng(oc::toBlock(cmd.getOr("seed", 0)));
            FullDecisionTree trees[3];
            auto comm2 = makeComms(ios);
            trees[0].mDebug = comm2[0];
            trees[1].mDebug = comm2[1];
            trees[2].mDebug = comm2[2];

            u64 n = 465;
            u64 d = 3;
            u64 numFeatures = 10;
            u64 featureBitCount = 10;
            u64 nodeBitCount = 10;
            u64 nodesPerTree = (1ull << d) - 1;
            u64 numLeaves = (1ull << d);
            u64 numLabels = 100;

            trees[0].init(d, n, numFeatures, featureBitCount, nodeBitCount, numLabels, rts[0], conv[0]);
            trees[1].init(d, n, numFeatures, featureBitCount, nodeBitCount, numLabels, rts[1], conv[1]);
            trees[2].init(d, n, numFeatures, featureBitCount, nodeBitCount, numLabels, rts[2], conv[2]);


            i64Matrix label(n, numLeaves * numLabels);
            i64Matrix y(n, numLabels), yy;// , active, active2, labels1, labels2;
            i64Matrix cmp(n, nodesPerTree);

            for (u64 i = 0; i < cmp.rows(); i++)
                for (u64 j = 0; j < cmp.cols(); j++)
                    cmp(i, j) = prng.getBit();


            label.setZero();
            for (u64 i = 0; i < cmp.rows(); i++)
            {
                for (u64 j = 0; j < numLeaves; j++)
                {
                    auto k = (prng.get<u64>() % numLabels);
                    //auto k = j;
                    label(i, j * numLabels + k) = 1;
                }
            }

            y = reduce(label, cmp);

            trees[0].initReduceCircuit(numLabels);
            trees[0].mReduceCir.levelByAndDepth();
            evaluate(trees[0].mReduceCir, { cmp, label }, { &yy });
            if (yy != y)
            {
                std::cout << "y  \n" << y << std::endl;
                std::cout << "yy \n" << yy << std::endl;

                throw std::runtime_error("incorrect result. " LOCATION);
            }

            sbMatrix l0, l1, l2, y0, y1, y2, c0, c1, c2;
            share(pack(label), label.cols(), l0, l1, l2, prng);
            share(pack(cmp), cmp.cols(), c0, c1, c2, prng);

            auto t0 = trees[0].reduce(rts[0], c0, l0, numLabels, y0);
            auto t1 = trees[1].reduce(rts[1], c1, l1, numLabels, y1);
            auto t2 = trees[2].reduce(rts[2], c2, l2, numLabels, y2);

            run(t0, t1, t2);

            reveal(yy, y0, y1, y2);
            yy = unpack(yy, y0.bitCount());
            if (y != yy)
            {
                std::cout << "y  \n" << y << std::endl;
                std::cout << "yy \n" << yy << std::endl;

                throw std::runtime_error("incorrect result. " LOCATION);
            }
        }


        i64Matrix vote(i64Matrix pred)
        {
            i64Matrix ret(1, pred.cols());
            ret.setZero();

            for (u64 i = 0; i < pred.rows(); ++i)
            {
                ret.row(0) += pred.row(i);
            }

            i64 max = ret(0);
            u64 argMax = 0;
            for (u64 i = 1; i < ret.cols(); i++)
            {
                if (ret(i) > max)
                {
                    max = ret(i);
                    argMax = i;
                }
            }

            ret.setZero();
            ret(argMax) = 1;

            return ret;
        }

        void FullTree_vote_test(const osuCrypto::CLP& cmd)
        {

            using namespace oc;
            IOService ios;
            std::array<Sh3Runtime, 3> rts;
            makeRuntimes(rts, ios);
            PRNG prng(oc::toBlock(cmd.getOr("seed", 0)));
            FullDecisionTree trees[3];

            u64 d = 4;
            u64 n = 100;
            u64 numFeatures = 10;
            u64 featureBitCount = 2, nodeBitCount = 2;
            u64 numLabels = 10;
            auto logn = oc::log2ceil(n);

            auto conv = makeShareGens();
            trees[0].init(d, n, numFeatures, featureBitCount, nodeBitCount, numLabels, rts[0], conv[0]);
            trees[1].init(d, n, numFeatures, featureBitCount, nodeBitCount, numLabels, rts[1], conv[1]);
            trees[2].init(d, n, numFeatures, featureBitCount, nodeBitCount, numLabels, rts[2], conv[2]);



            i64Matrix pred(n, numLabels), packed(1, logn * numLabels), sums(1, numLabels), sums2;
            i64Matrix out(1, numLabels), r;

            sums.setZero();
            pred.setZero();
            for (u64 i = 0; i < n; ++i)
            {
                auto k = prng.get<u64>() % numLabels;
                pred(i, k) = 1;

                sums(k) += 1;
            }

            out = vote(pred);

            for (u64 i = 0, k = 0; i < numLabels; i++)
            {
                //std::cout << "sum[" << i << "] = " << sums(i) << std::endl;
                auto iter = oc::BitIterator((u8*)&sums(i), 0);
                for (u64 j = 0; j < logn; j++, ++k)
                {
                    packed(0, k) = *iter++;
                }
            }

            trees[0].initVotingCircuit(numLabels, logn);
            auto& cir = trees[0].mVoteCircuit;



            evaluate(cir, { packed }, { &r });

            if (out != r)
            {
                std::cout << " out \n" << out << std::endl;
                std::cout << " r   \n" << r << std::endl;
                throw std::runtime_error(LOCATION);
            }

            sbMatrix p0, p1, p2, r0, r1, r2;
            share(pack(pred), pred.cols(), p0, p1, p2, prng);


            auto t0 = trees[0].vote(rts[0], p0, r0);
            auto t1 = trees[1].vote(rts[1], p1, r1);
            auto t2 = trees[2].vote(rts[2], p2, r2);

            run(t0, t1, t2);

            //reveal(sums2, trees[0].mSums, trees[1].mSums, trees[2].mSums);

            //if (sums2 != sums)
            //{
            //    std::cout << "s  \n" << sums << std::endl;
            //    std::cout << "s2 \n" << sums2 << std::endl;
            //}

            reveal(r, r0, r1, r2);
            r = unpack(r, r0.bitCount());

            if (out != r)
            {
                std::cout << " out \n" << out << std::endl;
                std::cout << " r   \n" << r << std::endl;
                throw std::runtime_error(LOCATION);
            }
        }


        void FullTree_endToEnd_test(const oc::CLP& cmd)
        {

            using namespace oc;
            IOService ios;
            std::array<Sh3Runtime, 3> rts;
            makeRuntimes(rts, ios);
            PRNG prng(oc::toBlock(cmd.getOr("seed", 0)));
            FullDecisionTree trees[3];

            u64 d = 4;
            u64 numTrees = 100;
            u64 featureCount = 10;
            u64 featureBitCount = 1, nodeBitCount = 1;
            u64 numLabels = 10;
            auto nodesPerTree = (1ull << d) - 1;
            auto numLeaves = (1ull << d);
            auto conv = makeShareGens();
            trees[0].init(d, numTrees, featureCount, featureBitCount, nodeBitCount, numLabels, rts[0], conv[0]);
            trees[1].init(d, numTrees, featureCount, featureBitCount, nodeBitCount, numLabels, rts[1], conv[1]);
            trees[2].init(d, numTrees, featureCount, featureBitCount, nodeBitCount, numLabels, rts[2], conv[2]);

            std::array<sbMatrix, 3> n, f, m, l, o;


            resize(f, featureBitCount, featureCount);
            resize(n, numTrees * nodesPerTree, nodeBitCount);
            resize(l, numTrees, numLeaves * numLabels);
            resize(m, numTrees* nodesPerTree, featureCount);

            auto t0 = trees[0].evaluate(rts[0], n[0], f[0], m[0], l[0], o[0]);
            auto t1 = trees[1].evaluate(rts[1], n[1], f[1], m[1], l[1], o[1]);
            auto t2 = trees[2].evaluate(rts[2], n[2], f[2], m[2], l[2], o[2]);

            run(t0, t1, t2);


        }
    }
}
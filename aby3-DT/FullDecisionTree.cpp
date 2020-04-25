#include "FullDecisionTree.h"
//#include "aby3-DB/"
#include "cryptoTools/Common/BitIterator.h"
#include "cryptoTools/Network/IOService.h"
#include "cryptoTools/Network/Session.h"
#include "aby3/sh3/Sh3Encryptor.h"
#include "aby3_tests/testUtils.h"

namespace aby3
{

    void FullDecisionTree::init(u64 d, u64 fc, Sh3Runtime& rt, Sh3ShareGen& gen, bool unitTest)
    {

        mUnitTest = unitTest;
        if (d < 1)
            throw std::runtime_error("d must at least be 1");

        if (fc == 0)
            throw std::runtime_error("logic error");

        mDepth = d;
        mFeatureCount = fc;

        mConv.init(rt, gen);

    }

    sbMatrix FullDecisionTree::run(Sh3Runtime& rt)
    {

        u64 n = 1, featureBitCount = 1, nodeBitCount = 1;
        u64 labelBitCount = 8;

        sbMatrix features(1, mFeatureCount * featureBitCount);

        u64 nodesPerTree = (1ull << mDepth) - 1;
        u64 numLeaves = 1ull << mDepth;

        sbMatrix nodes(n * nodesPerTree, nodeBitCount);
        sbMatrix mappedFeatures(n * nodesPerTree, featureBitCount);
        sbMatrix label(n, numLeaves * labelBitCount);

        // there should be numSums mappings, on for each tree. 
        sbMatrix mapping(nodesPerTree, mFeatureCount * featureBitCount);

        innerProd(rt, features, mapping, mappedFeatures).get();


        sbMatrix cmp(n, nodesPerTree);
        compare(rt, mappedFeatures, nodes, cmp, Comparitor::Eq).get();

        sbMatrix y(n, labelBitCount);
        reduce(rt, cmp, label, labelBitCount, y).get();

        sbMatrix output(labelBitCount, 1);
        vote(rt, y, output).get();

        return output;
    }

    u8 parity(u64 x)
    {
        u8* ptr = (u8*)&x;

        u8 v = 0;
        for (u64 i = 0; i < sizeof(u64); ++i)
            v = v ^ ptr[i];

        u8 ret = 0;
        for (u64 i = 0; i < 8; ++i)
            ret = ret ^ (v >>= 1);

        return ret & 1;
    }

    Sh3Task  FullDecisionTree::compare(Sh3Task dep,
        sbMatrix& features,
        sbMatrix& nodes,
        sbMatrix& cmp,
        Comparitor type)
    {
        // features: (nodesPerTree * numSums) x featureBitCount
        // nodes:    (nodesPerTree * numSums) x nodeBitCount

        if (features.rows() != nodes.rows())
            throw std::runtime_error(LOCATION);

        u64 nodesPerTree = (1ull << mDepth) - 1;
        u64 numTrees = features.rows() / nodesPerTree;
        u64 featureBitCount = features.mBitCount;
        u64 nodeBitCount = nodes.mBitCount;

        cmp.resize(features.rows(), 1);

        if (type == Comparitor::Eq)
        {
            if (featureBitCount != nodeBitCount)
                throw std::runtime_error("mis match bit count. " LOCATION);


            auto cir = mLib.int_eq(featureBitCount);
            mBin.setCir(cir, features.rows());
            mBin.setInput(0, features);
            mBin.setInput(1, nodes);

            return mBin.asyncEvaluate(dep).then([&, numTrees, nodesPerTree](Sh3Task self) {
                mBin.getOutput(0, cmp);

                // currently cmp has the dimension
                //    (nodesPerTree * numSums) x 1
                // 
                // Next we are going to reshape it so that it has dimension
                //     numSums x nodesPerTree  

                for (u64 b = 0; b < 2; ++b)
                {
                    for (u64 i = 0, ii = 0, jj = 0; i < numTrees; ++i)
                    {
                        for (u64 j = 0; j < nodesPerTree;)
                        {
                            auto min = std::min<u64>(nodesPerTree - j, 64);
                            j += min;
                            i64 sum = 0;
                            for (u64 k = 0; k < min; ++k, ++ii)
                            {
                                sum |= (cmp.mShares[b](ii) & 1) << k;
                            }

                            cmp.mShares[b](jj) = sum;
                            ++jj;
                        }
                    }
                }

                cmp.resize(numTrees, nodesPerTree);
                });

        }
        else
            throw std::runtime_error("not impl. " LOCATION);
    }

    Sh3Task FullDecisionTree::reduce(Sh3Task dep, sbMatrix& cmp, sbMatrix& labels, u64 labelBitCount, sbMatrix& pred)
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

    Sh3Task FullDecisionTree::vote(Sh3Task dep, sbMatrix& pred_, sbMatrix& out)
    {
        struct Temp
        {
            sbMatrix* out;
            sbMatrix bSums;
            si64Matrix sums;
            si64Matrix pred;
        };

        auto temp = std::make_shared<Temp>();
        temp->out = &out;

        
        return mConv.bitInjection(dep, pred_, temp->pred).then(
            [this, temp](Sh3Task self) {

                auto& bSums = temp->bSums;
                auto& sums = temp->sums;
                auto& pred = temp->pred;
                sums.resize(1, pred.cols());
                temp->out->resize(1, sums.size());

                for (u64 b = 0; b < 2; ++b)
                {
                    sums.mShares[b].setZero();
                    for (u64 i = 0; i < pred.rows(); ++i)
                    {
                        for (u64 j = 0; j < pred.cols(); ++j)
                        {
                            sums.mShares[b](j) += pred.mShares[b](i, j);
                        }
                    }
                }
                u64 bitCount = oc::log2ceil(pred.rows());
                bSums.resize(1, sums.size() * bitCount);

                mSums = sums;

                mConv.toBinaryMatrix(self, sums, bSums).then([this, temp, bitCount](Sh3Task self) {

                    auto& bSums = temp->bSums;
                    auto& sums = temp->sums;
                    auto& pred = temp->pred;

                    initVotingCircuit(sums.size(), bitCount);

                    mBin.setCir(&mVoteCircuit, 1);
                    mBin.setInput(0, bSums);

                    mBin.asyncEvaluate(self).then([this, temp](Sh3Task self) {
                        mBin.getOutput(0, *temp->out);
                        });
                    }
                );
            }
        ).getClosure();
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
            auto child = 1ull << (d+1);

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

    void FullDecisionTree::initVotingCircuit(u64 numSums, u64 bitCount)
    {
        auto str = [](auto x, int width = 2) -> std::string {
            std::string s = std::to_string(x); 
            while (s.size() < width)
                s.insert(s.begin(), ' ');
            return s;
        };

        oc::BetaCircuit& cir = mVoteCircuit;
        cir = {};

        oc::BetaBundle in(numSums * bitCount);
        oc::BetaBundle out(numSums);

        struct Node
        {
            Node() :cmp(1),active(1){ }
            Node(Node&&) = default;
            Node(const Node&) = default;

            oc::BetaBundle val;
            oc::BetaBundle cmp;
            oc::BetaBundle active;
        };

        std::vector<Node> nodes(numSums * 2);

        cir.addInputBundle(in);
        cir.addOutputBundle(out);


        auto iter = in.mWires.begin();
        for (u64 i = 0; i < numSums; ++i)
        {
            auto& v = nodes[i + numSums].val.mWires;
            v.insert(v.end(), iter, iter + bitCount);
            iter += bitCount;

            cir.addPrint("in[" + str(i) + "] = v[" + str(i + numSums) + "] = ");
            cir.addPrint(nodes[i + numSums].val);
            cir.addPrint("\n");

        }
        for (u64 i = 0; i < numSums; ++i)
            nodes[i + numSums].active[0]  = out[i];

        u64 start = 1ull << oc::log2ceil(numSums);
        u64 end = numSums * 2;
        u64 step = 1;
        u64 depth = 0;
        while (end != start)
        {
            ++depth;
            for (u64 i = start; i < end; i += 2)
            {
                // we are computing the following:
                // vp = max(v0, v1)
                //
                // by the following logic:
                //
                // cmp = (v0 < v1);
                // vp  = cmp * v1 + (1-cmp) v0
                //     = v0 ^ (v0 ^ v1) & cmp

                oc::BetaBundle temp(2 * bitCount);
                auto& v0 = nodes[i].val;
                auto& v1 = nodes[i + 1].val;
                auto& vp = nodes[i / 2].val;
                auto& cmp = nodes[i / 2].cmp;
                auto& active = nodes[i / 2].active;

                vp.mWires.resize(bitCount);

                cir.addTempWireBundle(temp);
                cir.addTempWireBundle(vp);
                cir.addTempWireBundle(cmp);
                cir.addTempWireBundle(active);


                if (mUnitTest)
                {

                    TODO("fix circuit, interally it is not depth optimized...");
                    // this should be sub_msb.
                    mLib.int_int_sub_msb_build_do(cir,
                        v0, v1,
                        cmp, temp);

                    cir.addPrint("v0[" + str(i) + "] = ");
                    cir.addPrint(v0); 
                    cir.addPrint("\nv1[" + str(i+1) + "] = ");
                    cir.addPrint(v1);
                    cir.addPrint("\n\tc=");
                    cir.addPrint(cmp);
                    cir.addPrint("\n");
                }
                else
                {

                    // this should be sub_msb.
                    mLib.int_int_add_msb_build_do(cir,
                        v0, v1,
                        cmp, temp);
                }
                //mLib.int_int_sub_msb_build(cir, v0, v1, cmp, temp);

                for (u64 j = 0; j < bitCount; ++j)
                {
                    cir.addGate(v0[j], v1[j], oc::GateType::Xor, temp[j]);
                    cir.addGate(temp[j], cmp[0], oc::GateType::And, temp[j]);
                    cir.addGate(temp[j], v0[j], oc::GateType::Xor, vp[j]);
                }

                cir.addPrint("vp["+str(i/2,2)+"] = ");
                cir.addPrint(vp);
                cir.addPrint("\n");

            }

            start = start / 2;
            end = start * 2;
        }

        cir.addTempWire(nodes[1].active[0]);
        cir.addConst(nodes[1].active[0], 1);


        u64 parent = 1;
        while(parent != numSums)
        {
            auto child = parent * 2;
            std::cout << "a[" << child << "] <- " << "a[" << parent << "] & !c[" << parent << "]" << std::endl;
            std::cout << "a[" << child +1<< "] <- " << "a[" << parent << "] &  c[" << parent << "]" << std::endl;

            cir.addGate(nodes[parent].active[0], nodes[parent].cmp[0], oc::GateType::nb_And, nodes[child + 0].active[0]);
            cir.addGate(nodes[parent].active[0], nodes[parent].cmp[0], oc::GateType::And, nodes[child + 1].active[0]);

            ++parent;
        }

    }


    Sh3Task FullDecisionTree::innerProd(Sh3Task dep, sbMatrix& X, sbMatrix& Y, sbMatrix& ret)
    {
        return dep.then(Sh3Task::RoundFunc([&](CommPkg& comm, Sh3Task self) {
            //C[0]
            //    = A[0] * B[0]
            //    + A[0] * B[1]
            //    + A[1] * B[0]
            //    + mShareGen.getShare();

            //comm.mNext.asyncSendCopy(C[0]);
            //auto fu = comm.mPrev.asyncRecv(C[1]).share();

            //self.then([fu = std::move(fu)](CommPkg& comm, Sh3Task& self){
            //    fu.get();
            //});

            u64 n = X.rows();
            u64 m = Y.rows();
            u64 w = X.mBitCount;
            u64 w64 = X.i64Cols();

            if (Y.mBitCount != w)
                throw std::runtime_error("bad inner dimension");

            ret.resize(n, m);

            for (u64 i = 0; i < n; ++i)
            {
                auto xx0 = X.mShares[0][i];
                auto xx1 = X.mShares[1][i];
                auto zz0 = oc::BitIterator((u8*)ret.mShares[0][i].data(), 0);
                for (u64 j = 0; j < m; ++j)
                {
                    auto yy0 = Y.mShares[0][j];
                    auto yy1 = Y.mShares[1][j];

                    i64 sum = 0;
                    for (u64 k = 0; k < w64; ++k)
                    {
                        sum = sum
                            ^ xx0[k] & yy0[k]
                            ^ xx0[k] & yy1[k]
                            ^ xx1[k] & yy0[k];
                    }
                    *zz0++ = parity(sum);
                }
            }


            comm.mNext.asyncSendCopy(ret.mShares[0]);
            auto fu = comm.mPrev.asyncRecv(ret.mShares[1]).share();

            self.then([fu = std::move(fu)](CommPkg& comm, Sh3Task& self) {
                fu.get();
            });

            })).getClosure();
    }



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


        void compare(i64Matrix& features, i64Matrix& nodes, i64Matrix& cmp, FullDecisionTree::Comparitor type)
        {
            if (type != FullDecisionTree::Comparitor::Eq)
                throw std::runtime_error(LOCATION);

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
            auto type = FullDecisionTree::Comparitor::Eq;

            trees[0].init(d, numFeatures, rts[0], conv[0]);
            trees[1].init(d, numFeatures, rts[1], conv[1]);
            trees[2].init(d, numFeatures, rts[2], conv[2]);

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
            compare(mappedFeatures, nodes, cmp, type);


            sbMatrix f0, f1, f2, n0, n1, n2, c0, c1, c2;

            share(pack(mappedFeatures), nodeBitCount, f0, f1, f2, prng);
            share(pack(nodes), nodeBitCount, n0, n1, n2, prng);


            auto t0 = trees[0].compare(rts[0], f0, n0, c0, type);
            auto t1 = trees[1].compare(rts[1], f1, n1, c1, type);
            auto t2 = trees[2].compare(rts[2], f2, n2, c2, type);

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
                    auto c = cmp(i, active[i]-1);
                    //std::cout << "active[" << i << "] = 2 * " << (active[i]) <<" + " << c << std::endl;
                    active[i] = 2 * active[i] + c;
                }
            }

            auto s = 1ull << depth;
            //for (u64 i = 0; i < s; i++)
            //{
            //    std::cout << "label[0][" << i << "] = ";
            //    for (u64 j = 0; j < labelBitCount; j++)
            //    {
            //        std::cout <<  labels(0, i * labelBitCount + j) << " ";
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
            u64 nodesPerTree = (1ull << d) - 1;
            u64 numLeaves = (1ull << d);
            u64 labelBitCount = 100;
            auto type = FullDecisionTree::Comparitor::Eq;

            trees[0].init(d, numFeatures, rts[0], conv[0]);
            trees[1].init(d, numFeatures, rts[1], conv[1]);
            trees[2].init(d, numFeatures, rts[2], conv[2]);

            i64Matrix label(n, numLeaves * labelBitCount);
            i64Matrix y(n, labelBitCount), yy;// , active, active2, labels1, labels2;
            i64Matrix cmp(n, nodesPerTree);

            for (u64 i = 0; i < cmp.rows(); i++)
                for (u64 j = 0; j < cmp.cols(); j++)
                    cmp(i, j) = prng.getBit();


            label.setZero();
            for (u64 i = 0; i < cmp.rows(); i++)
            {
                for (u64 j = 0; j < numLeaves; j++)
                {
                    auto k = (prng.get<u64>() % labelBitCount);
                    //auto k = j;
                    label(i, j * labelBitCount + k) = 1;
                }
            }

            y = reduce(label, cmp);

            trees[0].initReduceCircuit(labelBitCount);
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

            auto t0 = trees[0].reduce(rts[0], c0, l0, labelBitCount, y0);
            auto t1 = trees[1].reduce(rts[1], c1, l1, labelBitCount, y1);
            auto t2 = trees[2].reduce(rts[2], c2, l2, labelBitCount, y2);

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

            auto conv = makeShareGens();
            trees[0].init(2, 2, rts[0], conv[0], true);
            trees[1].init(2, 2, rts[1], conv[1], true);
            trees[2].init(2, 2, rts[2], conv[2], true);

            u64 n = 100;
            u64 labelBitCount = 10;
            auto logn = oc::log2ceil(n);

            i64Matrix pred(n, labelBitCount), packed(1, logn * labelBitCount), sums(1, labelBitCount), sums2;
            i64Matrix out(1, labelBitCount), r;

            sums.setZero();
            pred.setZero();
            for (u64 i = 0; i < n; ++i)
            {
                auto k = prng.get<u64>() % labelBitCount;
                pred(i, k) = 1;

                sums(k) += 1;
            }

            out = vote(pred);

            for (u64 i = 0,k=0; i < labelBitCount; i++)
            {
                std::cout << "sum[" << i << "] = " << sums(i) << std::endl;
                auto iter = oc::BitIterator((u8*)&sums(i), 0);
                for (u64 j = 0; j < logn; j++, ++k)
                {
                    packed(0, k) = *iter++;
                }
            }

            trees[0].initVotingCircuit(labelBitCount, logn);
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

            reveal(sums2, trees[0].mSums, trees[1].mSums, trees[2].mSums);

            if (sums2 != sums)
            {
                std::cout << "s  \n" << sums << std::endl;
                std::cout << "s2 \n" << sums2 << std::endl;
            }

            reveal(r, r0, r1, r2);
            r = unpack(r, r0.bitCount());

            if (out != r)
            {
                std::cout << " out \n" << out << std::endl;
                std::cout << " r   \n" << r   << std::endl;
                throw std::runtime_error(LOCATION);
            }
        }


        void FullTree_endToEnd_test(const oc::CLP& cmd)
        {

        }
    }
}
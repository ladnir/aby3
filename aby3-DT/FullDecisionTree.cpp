#include "FullDecisionTree.h"
//#include "aby3-DB/"
#include "cryptoTools/Common/BitIterator.h"
#include "cryptoTools/Network/IOService.h"
#include "cryptoTools/Network/Session.h"
#include "aby3/sh3/Sh3Encryptor.h"
#include "aby3_tests/testUtils.h"

namespace aby3
{

    sbMatrix FullDecisionTree::run(u64 d, u64 fc, Sh3Runtime& rt)
    {

        if (d < 1)
            throw std::runtime_error("d must at least be 1");

        mDepth = d;

        u64 n = 1, featureBitCount = 1, nodeBitCount = 1;
        u64 labelBitCount = 8;

        sbMatrix features(1, fc * featureBitCount);

        u64 nodesPerTree = (1ull << mDepth) - 1;
        u64 numLeaves = 1ull << mDepth;

        sbMatrix nodes(n * nodesPerTree, nodeBitCount);
        sbMatrix mappedFeatures(n * nodesPerTree, featureBitCount);
        sbMatrix label(n * numLeaves, labelBitCount);

        // there should be n mappings, on for each tree. 
        sbMatrix mapping(nodesPerTree, fc * featureBitCount);

        innerProd(rt, features, mapping, mappedFeatures).get();


        sbMatrix cmp(n, nodesPerTree);
        compare(rt, mappedFeatures, nodes, cmp, Comparitor::Eq).get();

        sbMatrix y(n, labelBitCount);
        reduce(rt, cmp, label, y).get();

        sbMatrix output(labelBitCount,1);
        vote(rt, y, output).get();
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
        // features: (nodesPerTree * numTrees) x featureBitCount
        // nodes:    (nodesPerTree * numTrees) x nodeBitCount

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
                //    (nodesPerTree * numTrees) x 1
                // 
                // Next we are going to reshape it so that it has dimension
                //     numTrees x nodesPerTree  

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

                cmp.resize(numTrees, (nodesPerTree + 63) / 64);
                });

        }
        else
            throw std::runtime_error("not impl. " LOCATION);
    }

    Sh3Task FullDecisionTree::reduce(Sh3Task dep, sbMatrix& cmp, sbMatrix& labels, sbMatrix& pred)
    {
        return dep.then([&](Sh3Task self) {

            //sbMatrix cmp(numTrees, nodesPerTree);
            u64 numTrees = cmp.rows();
            u64 nodesPerTree = cmp.bitCount();

            initReduceCircuit(labels.bitCount());

            mBin.setCir(&mReduceCir, cmp.rows());
            mBin.setInput(0, cmp);
            mBin.setInput(1, labels);

            mBin.asyncEvaluate(self).then([&](Sh3Task self) {
                mBin.getOutput(0, pred);
                });

            }).getClosure();
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

                temp->sums.resize(temp->pred.cols(), 1);
                auto& bSums = temp->bSums;
                auto& sums = temp->sums;
                auto& pred = temp->pred;
                for (u64 b = 0; b < 2; ++b)
                {
                    for (u64 i = 0; i < pred.rows(); ++i)
                    {
                        for (u64 j = 0; j < pred.cols(); ++j)
                        {
                            sums.mShares[b](j) += pred.mShares[b](i, j);
                        }
                    }
                }
                u64 bitCount = oc::log2ceil(pred.rows());
                bSums.resize(sums.size(), bitCount);

                mConv.toBinaryMatrix(self, sums, bSums).then([this, temp](Sh3Task self) {

                    auto& bSums = temp->bSums;
                    auto bitCount = bSums.bitCount();
                    auto& sums = temp->sums;
                    auto& pred = temp->pred;

                    initVotingCircuit(sums.size(), bitCount);

                    mBin.setCir(&mVoteCircuit, 1);
                    mBin.setInput(0, bSums);

                    mBin.asyncEvaluate(self).then([this, temp](Sh3Task self) {

                        temp->out->resize(temp->bSums.rows(), 1);
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
        mReduceCir.addConst(active[0], 1);


        for (u64 d = 1; d < mDepth; ++d)
        {
            auto parent = 1ull << (d - 1);
            auto child = 1ull << d;

            auto pEnd = child;

            while (parent != pEnd)
            {
                mReduceCir.addGate(active[parent - 1], cmp[child - 1], oc::GateType::nb_And, active[child - 1]);
                mReduceCir.addGate(active[parent - 1], cmp[child - 0], oc::GateType::And, active[child - 0]);

                child += 2;
                ++parent;
            }
        }

        u64 s = 1ull << mDepth;
        for (u64 i = 0, l = 0; i < numLabels; ++i)
        {
            for (u64 j = 0; j < labelBitCount; ++j, ++l)
            {
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

    void FullDecisionTree::initVotingCircuit(u64 n, u64 bitCount)
    {

        oc::BetaCircuit& cir = mVoteCircuit;
        cir = {};

        std::vector<oc::BetaBundle> in(n);
        oc::BetaBundle out(n);

        struct Node
        {
            oc::BetaBundle val;
            oc::BetaWire cmp;
            oc::BetaWire active;
        };

        std::vector<Node> nodes(n * 2);

        for (u64 i = 0; i < in.size(); ++i)
        {
            in[i].mWires.resize(bitCount);
            cir.addInputBundle(in[i]);
            nodes[i + in.size()].val = in[i];
        }
        cir.addOutputBundle(out);
        for (u64 i = 0; i < in.size(); ++i)
            nodes[i + in.size()].active = out[i];

        u64 start = 1ull << oc::log2ceil(in.size());
        u64 end = in.size() * 2;
        u64 step = 1;

        while (end != start)
        {
            for (u64 i = start; i < end; i += 2)
            {
                // we are computing the following:
                // vp = max(v0, v1)
                //
                // by the following logic:
                //
                // cmp = (v0 < v1);
                // v0 = cmp * v1 + (1-cmp) v0
                //         = v0 ^ (v0 ^ v1) & cmp

                oc::BetaBundle temp(2 * bitCount);
                auto& v0 = nodes[i].val;
                auto& v1 = nodes[i + 1].val;
                auto& vp = nodes[i / 2].val;
                auto& cmp = nodes[i / 2].cmp;
                auto& active = nodes[i / 2].cmp;

                vp.mWires.resize(bitCount);

                cir.addTempWireBundle(temp);
                cir.addTempWireBundle(vp);
                cir.addTempWire(cmp);
                cir.addTempWire(active);


                TODO("fix this");
                // this should be sub_msb.
                mLib.int_int_add_msb_build_do(cir,
                    v0, v1,
                    cmp, temp);

                for (u64 j = 0; j < bitCount; ++j)
                {
                    cir.addGate(v0[j], v1[j], oc::GateType::Xor, temp[j]);
                    cir.addGate(temp[j], cmp, oc::GateType::And, temp[j]);
                    cir.addGate(temp[j], v0[j], oc::GateType::Xor, vp[j]);
                }
            }
        }

        start = start / 2;
        end = start * 2;

        cir.addConst(nodes[1].active, 1);

        for (u64 d = 1; d < mDepth; ++d)
        {
            auto parent = 1ull << (d - 1);
            auto child = 1ull << d;

            auto pEnd = std::min<u64>(child, nodes.size());

            while (parent != pEnd)
            {
                mReduceCir.addGate(nodes[parent].active, nodes[child + 0].cmp, oc::GateType::nb_And, nodes[child + 0].active);
                mReduceCir.addGate(nodes[parent].active, nodes[child + 1].cmp, oc::GateType::And, nodes[child + 1].active);

                child += 2;
                ++parent;
            }
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
            auto chl01 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "01").addChannel();
            auto chl10 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "01").addChannel();
            auto chl02 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "02").addChannel();
            auto chl20 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "02").addChannel();
            auto chl12 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "12").addChannel();
            auto chl21 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "12").addChannel();

            int n = 43;
            int m = 17;
            int w = 465;

            int m64 = (m + 63) / 64;
            int w64 = (w + 63) / 64;
            int trials = 10;
            CommPkg comms[3];
            comms[0] = { chl02, chl01 };
            comms[1] = { chl10, chl12 };
            comms[2] = { chl21, chl20 };

            Sh3Encryptor encs[3];
            Sh3Runtime rts[3];
            rts[0].init(0, comms[0]);
            rts[1].init(1, comms[1]);
            rts[2].init(2, comms[2]);
            encs[0].init(0, oc::toBlock(0ull), oc::toBlock(1));
            encs[1].init(1, oc::toBlock(1), oc::toBlock(2));
            encs[2].init(2, oc::toBlock(2), oc::toBlock(0ull));

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

            //std::cout << " x \n" << bitstr(x, w) << std::endl;
            //std::cout << " y \n" << bitstr(y, w) << std::endl;

            auto z = innerProd(x, y);

            //std::cout << " z \n" << bitstr(z, m) << std::endl;

            sbMatrix x0, x1, x2, y0, y1, y2, z0, z1, z2;

            auto t0 = rts[0].noDependencies();
            auto t1 = rts[1].noDependencies();
            auto t2 = rts[2].noDependencies();

            share(x, w, x0, x1, x2, prng);
            share(y, w, y0, y1, y2, prng);

            //t0 = encs[0].localPackedBinary(t0, x, x0);
            //t1 = encs[1].remotePackedBinary(t1, x1);
            //t2 = encs[2].remotePackedBinary(t2, x2);

            //t0 = encs[0].localPackedBinary(t0, y, y0);
            //t1 = encs[1].remotePackedBinary(t1, y1);
            //t2 = encs[2].remotePackedBinary(t2, y2);

            t0 = trees[0].innerProd(t0, x0, y0, z0);
            t1 = trees[1].innerProd(t1, x1, y1, z1);
            t2 = trees[2].innerProd(t2, x2, y2, z2);



            run(t0, t1, t2);


            //t0 = encs[0].reveal(t0, z0, zz);
            //t1 = encs[1].reveal(t1, 0, z1);
            //t2 = encs[2].reveal(t2, 0, z2);
            reveal(zz, z0, z1, z2);

            if (zz != z)
            {
                std::cout << "zz \n" << bitstr(zz, m) << std::endl;
                std::cout << "z \n" << bitstr(z, m) << std::endl;


                throw std::runtime_error("incorrect result. " LOCATION);
            }


        }
    }
}
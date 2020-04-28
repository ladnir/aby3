#include "Common.h"


namespace aby3
{


    Sh3Task  TreeBase::compare(Sh3Task dep,
        const sbMatrix& features,
        const sbMatrix& nodes,
        u64 nodesPerTree,
        sbMatrix& cmp,
        Comparitor type)
    {
        // features: (nodesPerTree * numSums) x featureBitCount
        // nodes:    (nodesPerTree * numSums) x nodeBitCount

        if (features.rows() != nodes.rows())
            throw std::runtime_error(LOCATION);

        //u64 nodesPerTree = (1ull << mDepth) - 1;
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

                if (nodesPerTree != 1)
                {
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
                }

                });

        }
        else
            throw std::runtime_error("not impl. " LOCATION);
    }


    Sh3Task TreeBase::vote(
        Sh3Task dep,
        const sbMatrix& pred_,
        sbMatrix& out)
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

                //mSums = sums;

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



    void TreeBase::initVotingCircuit(u64 numSums, u64 bitCount)
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
            Node() :cmp(1), active(1) { }
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

            //cir.addPrint("in[" + str(i) + "] = v[" + str(i + numSums) + "] = ");
            //cir.addPrint(nodes[i + numSums].val);
            //cir.addPrint("\n");

        }
        for (u64 i = 0; i < numSums; ++i)
            nodes[i + numSums].active[0] = out[i];

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

                    //cir.addPrint("v0[" + str(i) + "] = ");
                    //cir.addPrint(v0);
                    //cir.addPrint("\nv1[" + str(i + 1) + "] = ");
                    //cir.addPrint(v1);
                    //cir.addPrint("\n\tc=");
                    //cir.addPrint(cmp);
                    //cir.addPrint("\n");
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

                //cir.addPrint("vp[" + str(i / 2, 2) + "] = ");
                //cir.addPrint(vp);
                //cir.addPrint("\n");

            }

            start = start / 2;
            end = start * 2;
        }

        cir.addTempWire(nodes[1].active[0]);
        cir.addConst(nodes[1].active[0], 1);


        u64 parent = 1;
        while (parent != numSums)
        {
            auto child = parent * 2;
            //std::cout << "a[" << child << "] <- " << "a[" << parent << "] & !c[" << parent << "]" << std::endl;
            //std::cout << "a[" << child + 1 << "] <- " << "a[" << parent << "] &  c[" << parent << "]" << std::endl;

            cir.addGate(nodes[parent].active[0], nodes[parent].cmp[0], oc::GateType::nb_And, nodes[child + 0].active[0]);
            cir.addGate(nodes[parent].active[0], nodes[parent].cmp[0], oc::GateType::And, nodes[child + 1].active[0]);

            ++parent;
        }

    }
}
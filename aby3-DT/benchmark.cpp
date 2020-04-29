#include "benchmark.h"
#include "aby3-DT/SparseDecisionTree.h"
#include "cryptoTools/Network/IOService.h"
using namespace oc;
#include <iomanip>

namespace aby3
{

    void sparseRoutine(
        IOService& ios,
        std::string ip,
        u64 partyIdx,
        u64 depth,
        u64 numTrees,
        u64 nodesPerTree,
        u64 numLabels,
        u64 numFeatures,
        Timer* timer,
        bool verbose,
        std::array<u64, 2>& bt)
    {
        std::vector<u32> startIdx(numTrees + 1);
        for (u64 i = 0; i < startIdx.size(); i++)
            startIdx[i] = i * nodesPerTree;

        u64 featureBC = 1;
        u64 nodeSize =
            SparseDecisionForest::mBlockSize * 4 +
            3 * 8 +
            oc::roundUpTo(numLabels, 8);

        Sh3ShareGen gen;
        CommPkg comm;
        switch (partyIdx)
        {
        case 0:
            comm.mPrev = Session(ios, ip, oc::SessionMode::Server, "02").addChannel();
            comm.mNext = Session(ios, ip, oc::SessionMode::Client, "01").addChannel();
            gen.init(toBlock(1), toBlock(2));
            break;
        case 1:
            comm.mPrev = Session(ios, ip, oc::SessionMode::Server, "01").addChannel();
            comm.mNext = Session(ios, ip, oc::SessionMode::Client, "12").addChannel();
            gen.init(toBlock(2), toBlock(3));
            break;
        case 2:
            comm.mPrev = Session(ios, ip, oc::SessionMode::Server, "12").addChannel();
            comm.mNext = Session(ios, ip, oc::SessionMode::Client, "02").addChannel();
            gen.init(toBlock(3), toBlock(1));
            break;
        default:
            throw RTE_LOC;
        }

        Sh3Runtime rt;
        rt.init(partyIdx, comm);
        sbMatrix nodes(startIdx.back(), nodeSize);

        SparseDecisionForest trees;
        trees.init(depth, startIdx, nodes, featureBC, numLabels, rt, std::move(gen));

        sbMatrix features(numFeatures, featureBC);


        comm.mPrev.waitForConnection();
        comm.mNext.waitForConnection();
        char c;
        comm.mNext.send(c);
        comm.mPrev.send(c);
        comm.mNext.recv(c);
        comm.mPrev.recv(c);

        if (timer)
            timer->setTimePoint("start");

        if (timer && verbose)
            trees.setTimer(*timer);

        trees.evaluate(rt, features).get();


        if (timer)
            timer->setTimePoint("done");


        bt[0] = comm.mPrev.getTotalDataSent() + comm.mNext.getTotalDataSent();
        bt[1] = comm.mPrev.getTotalDataRecv() + comm.mNext.getTotalDataRecv();
    }

    struct Indexer
    {
        std::vector<std::vector<u64>> mVals;
        std::vector<u64> mCur;
        Indexer(std::vector<std::vector<u64>> vals)
            : mVals(vals)
            , mCur(mVals.size())
        {}

        std::vector<u64> next()
        {
            std::vector<u64> ret;
            for (u64 i = 0; i < mCur.size(); i++)
                ret.push_back(mVals[i][mCur[i]]);

            for (u64 i = 0; i < mCur.size(); i++)
            {
                mCur[i]++;
                if (mCur[i] != mVals[i].size())
                    break;
                mCur[i] = 0;
            }

            return ret;
        }

        u64 size()
        {
            if (mVals.size() == 0)
                return 0;
            u64 prod = 1;
            for (auto m : mVals)
                prod *= m.size();
            return prod;
        }
    };

    void benchmark_sparse(oc::CLP& cmd)
    {

        auto vecNumTrees = cmd.getManyOr<u64>("numTrees", { 400 });
        auto vecNodesPer = cmd.getManyOr<u64>("nodesPer", { 400 });
        auto vecDepth = cmd.getManyOr<u64>("depth", { 20 });
        auto vecNumLabels = cmd.getManyOr<u64>("numLabels", { 8 });
        auto vecNumFeatures = cmd.getManyOr<u64>("numFeature", { 100 });
        bool verbose = cmd.isSet("v");

        auto p = cmd.getManyOr<u64>("p", { 0,1,2 });

        IOService ios;
        std::vector<std::string> ip = cmd.getManyOr<std::string>("ip", { "127.0.0.1:1212" });
        while (ip.size() < p.size())
            ip.push_back(ip.back());

        Indexer idx({ vecNumTrees, vecNodesPer, vecDepth, vecNumLabels, vecNumFeatures });

        std::vector<std::array<u64, 2>> bt(p.size());
        std::vector<std::thread> thrds;
        for (u64 i = 1; i < p.size(); ++i)
            thrds.emplace_back([&, i, idx]() mutable
                {
                    auto size = idx.size();
                    for (u64 j = 0; j < size; j++)
                    {
                        auto ii = idx.next();
                        auto numTrees = ii[0];
                        auto nodesPer = ii[1];
                        auto depth = ii[2];
                        auto numLabels = ii[3];
                        auto numFeatures = ii[4];

                        sparseRoutine(
                            ios, ip[i], p[i], depth, numTrees,
                            nodesPer, numLabels, numFeatures,
                            nullptr, false, bt[i]);
                    }
                });

        Timer timer;
        if (p.size())
        {
            auto size = idx.size();
            for (u64 j = 0; j < size; j++)
            {
                auto ii = idx.next();
                auto numTrees = ii[0];
                auto nodesPer = ii[1];
                auto depth = ii[2];
                auto numLabels = ii[3];
                auto numFeatures = ii[4];

                sparseRoutine(
                    ios, ip[0], p[0], depth, numTrees,
                    nodesPer, numLabels, numFeatures,
                    &timer, verbose, bt[0]);


                auto start = timer.mTimes.front().first;
                auto end = timer.mTimes.back().first;

                auto time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

                if (verbose)
                {
                    std::cout << "numTrees     " << numTrees << std::endl;
                    std::cout << "nodesPer     " << nodesPer << std::endl;
                    std::cout << "depth        " << depth << std::endl;
                    std::cout << "numLabels    " << numLabels << std::endl;
                    std::cout << "numFeatures  " << numFeatures << std::endl;
                    std::cout << timer << std::endl;

                    for (u64 i = 0; i < p.size(); i++)
                    {
                        std::cout << "p" << p[i] << " send/recv  "
                            << bt[i][0] << " " << bt[i][1] << " bytes" << std::endl;
                    }
                }
                else
                {

                    std::cout << "n " << std::setw(6) << std::setfill(' ') << numTrees;
                    std::cout << " m " << std::setw(6) << std::setfill(' ') << nodesPer;
                    std::cout << " d " << std::setw(6) << std::setfill(' ') << depth;
                    std::cout << " l " << std::setw(6) << std::setfill(' ') << numLabels;
                    std::cout << " f " << std::setw(6) << std::setfill(' ') << numFeatures;
                    std::cout << " t " << std::setw(6) << std::setfill(' ') << time;
                    std::cout << " c " << std::setw(6) << std::setfill(' ') << bt[0][0];
                    std::cout << " " << std::setw(6) << std::setfill(' ') << bt[0][1] << std::endl;
                }
            }
        }


        for (auto& t : thrds)
            t.join();

    }

}
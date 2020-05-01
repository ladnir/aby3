#include "benchmark.h"
#include "aby3-DT/SparseDecisionTree.h"
#include "aby3-DT/FullDecisionTree.h"
#include "cryptoTools/Network/IOService.h"
using namespace oc;
#include <iomanip>

namespace aby3
{

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

    void printStats(
        u64 partyIdx,
        CommPkg& comm,
        Timer& timer,
        u64 numTrees,
        u64 nodesPerTree,
        u64 depth,
        u64 numLabels,
        u64 numFeatures,
        u64 featureBC,
        std::string tag,
        bool verbose)
    {

        std::array<std::array<u64, 2>, 3> bt;
        bt[partyIdx][0] = comm.mPrev.getTotalDataSent() + comm.mNext.getTotalDataSent();
        bt[partyIdx][1] = comm.mPrev.getTotalDataRecv() + comm.mNext.getTotalDataRecv();


        auto start = timer.mTimes.front().first;
        auto end = timer.mTimes.back().first;
        auto time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();


        if (partyIdx == 0)
        {
            comm.mPrev.recv(bt[2]);
            comm.mNext.recv(bt[1]);

            if (verbose)
            {
                std::cout << "Sparse       " << std::endl;
                std::cout << "numTrees     " << numTrees << std::endl;
                std::cout << "nodesPer     " << nodesPerTree << std::endl;
                std::cout << "depth        " << depth << std::endl;
                std::cout << "numLabels    " << numLabels << std::endl;
                std::cout << "numFeatures  " << numFeatures << std::endl;
                std::cout << "featureBC    " << featureBC << std::endl;
                std::cout << timer << std::endl;

                for (u64 i = 0; i < bt.size(); i++)
                {
                    std::cout << "p" << i << " send/recv  "
                        << bt[i][0] << " " << bt[i][1] << " bytes" << std::endl;
                }
            }
            else
            {

                bt[0][0] += bt[1][0] + bt[2][0];

                std::cout << tag;
                std::cout << " n " << std::setw(6) << std::setfill(' ') << numTrees;
                std::cout << " m " << std::setw(6) << std::setfill(' ') << nodesPerTree;
                std::cout << " d " << std::setw(6) << std::setfill(' ') << depth;
                std::cout << " l " << std::setw(6) << std::setfill(' ') << numLabels;
                std::cout << " f " << std::setw(6) << std::setfill(' ') << numFeatures;
                std::cout << " bc " << std::setw(6) << std::setfill(' ') << featureBC;
                std::cout << " t " << std::setw(6) << std::setfill(' ') << time;
                std::cout << " c " << std::setw(6) << std::setfill(' ') << bt[0][0] << std::endl;
            }
        }
        else if (partyIdx == 1)
        {
            comm.mPrev.send(bt[1]);
        }
        else
            comm.mNext.send(bt[2]);

        comm.mNext.resetStats();
        comm.mPrev.resetStats();
    }

    void sparseRoutine(
        IOService& ios,
        std::string ip,
        u64 partyIdx,
        Indexer idx,

        bool verbose)
    {
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

        comm.mPrev.waitForConnection();
        comm.mNext.waitForConnection();

        auto numParam = idx.size();
        for (u64 pIdx = 0; pIdx < numParam; ++pIdx)
        {
            auto params = idx.next();
            auto numTrees = params[0];
            auto nodesPerTree = params[1];
            auto depth = params[2];
            auto numLabels = params[3];
            auto numFeatures = params[4];
            auto featureBC = params[5];


            std::vector<u32> startIdx(numTrees + 1);
            for (u64 i = 0; i < startIdx.size(); i++)
                startIdx[i] = i * nodesPerTree;

            u64 nodeSize =
                SparseDecisionForest::mBlockSize * 4 +
                2 * 8 +
                oc::roundUpTo(featureBC, 8)+
                oc::roundUpTo(numLabels, 8);



            Sh3Runtime rt;
            rt.init(partyIdx, comm);
            sbMatrix nodes(startIdx.back(), nodeSize);

            SparseDecisionForest trees;
            trees.init(depth, startIdx, nodes, featureBC, numLabels, rt, gen.clone());

            sbMatrix features(numFeatures, featureBC);


            char c;
            comm.mNext.send(c);
            comm.mPrev.send(c);
            comm.mNext.recv(c);
            comm.mPrev.recv(c);
            comm.mNext.send(c);
            comm.mPrev.send(c);
            comm.mNext.recv(c);
            comm.mPrev.recv(c);

            Timer timer;
            timer.setTimePoint("start");

            if (verbose)
                trees.setTimer(timer);

            trees.evaluate(rt, features).get();


            timer.setTimePoint("done");
            auto tag = "sparse";
            printStats(partyIdx, comm, timer, numTrees, nodesPerTree, depth, numLabels, numFeatures,featureBC, tag, verbose);
        }
    }

    void denseRoutine(
        IOService& ios,
        std::string ip,
        u64 partyIdx,
        Indexer idx,
        bool verbose)
    {

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


        comm.mPrev.waitForConnection();
        comm.mNext.waitForConnection();

        auto numParams = idx.size();
        for (u64 pIdx = 0; pIdx < numParams; pIdx++)
        {

            auto params = idx.next();
            auto numTrees = params[0];
            auto depth = params[1];
            auto numLabels = params[2];
            auto numFeatures = params[3];
            u64 featureBC = params[4];

            Sh3Runtime rt;
            rt.init(partyIdx, comm);
            auto nodesPer = (1ull << depth) - 1;
            auto numLeaves = 1ull << depth;
            sbMatrix nodes(nodesPer * numTrees, featureBC);
            sbMatrix features(featureBC, numFeatures);
            sbMatrix leaves(numTrees, numLeaves * numLabels);
            sbMatrix mapping(nodesPer * numTrees, numFeatures);
            sbMatrix out;

            FullDecisionTree trees;
            trees.init(depth, numTrees, numFeatures, featureBC, featureBC, numLabels, rt, gen);


            char c;
            comm.mNext.send(c);
            comm.mPrev.send(c);
            comm.mNext.recv(c);
            comm.mPrev.recv(c);
            comm.mNext.send(c);
            comm.mPrev.send(c);
            comm.mNext.recv(c);
            comm.mPrev.recv(c);

            Timer timer;
            timer.setTimePoint("start");

            if (verbose)
                trees.setTimer(timer);

            trees.evaluate(rt, nodes, features, mapping, leaves, out).get();


            timer.setTimePoint("done");
            rt.cancelTasks();

            auto nodesPerTree = 1ull << depth;
            auto tag = "dense ";
            printStats(partyIdx, comm, timer, numTrees, nodesPerTree, depth, numLabels, numFeatures, featureBC, tag, verbose);
        }

    }


    void benchmark_dense(oc::CLP& cmd)
    {

        auto vecNumTrees = cmd.getManyOr<u64>("numTrees", { 400 });
        auto vecDepth = cmd.getManyOr<u64>("depth", { 6 });
        auto vecNumLabels = cmd.getManyOr<u64>("numLabels", { 8 });
        auto vecNumFeatures = cmd.getManyOr<u64>("numFeatures", { 100 });
        auto vecFeaturesBC = cmd.getManyOr<u64>("featureBC", { 1 });
        bool verbose = cmd.isSet("v");

        auto p = cmd.getManyOr<u64>("p", { 0,1,2 });
        std::vector<std::string> ip = cmd.getManyOr<std::string>("ip", { "127.0.0.1:1212" });
        while (ip.size() < p.size())
            ip.push_back(ip.back());
        Indexer idx({ vecNumTrees, vecDepth, vecNumLabels, vecNumFeatures, vecFeaturesBC });


        IOService ios;
        std::vector<std::thread> thrds;
        for (u64 i = 1; i < p.size(); ++i)
            thrds.emplace_back([&, i, idx]() mutable{
                    denseRoutine(ios, ip[i], p[i], idx, false);
                });

        Timer timer;
        if (p.size())
            denseRoutine(ios, ip[0], p[0], idx, verbose);


        for (auto& t : thrds)
            t.join();


    }

    void benchmark_sparse(oc::CLP& cmd)
    {

        auto vecNumTrees = cmd.getManyOr<u64>("numTrees", { 400 });
        auto vecNodesPer = cmd.getManyOr<u64>("nodesPer", { 400 });
        auto vecDepth = cmd.getManyOr<u64>("depth", { 20 });
        auto vecNumLabels = cmd.getManyOr<u64>("numLabels", { 8 });
        auto vecNumFeatures = cmd.getManyOr<u64>("numFeatures", { 100 });
        auto vecFeaturesBC = cmd.getManyOr<u64>("featureBC", { 1 });
        bool verbose = cmd.isSet("v");

        auto p = cmd.getManyOr<u64>("p", { 0,1,2 });
        std::vector<std::string> ip = cmd.getManyOr<std::string>("ip", { "127.0.0.1:1212" });
        while (ip.size() < p.size())
            ip.push_back(ip.back());
        Indexer idx({ vecNumTrees, vecNodesPer, vecDepth, vecNumLabels, vecNumFeatures, vecFeaturesBC });

        IOService ios;
        std::vector<std::thread> thrds;
        for (u64 i = 1; i < p.size(); ++i)
            thrds.emplace_back([&, i, idx]() mutable {
                    sparseRoutine(
                        ios, ip[i], p[i], idx, false);
                });

        Timer timer;
        if (p.size())
            sparseRoutine(ios, ip[0], p[0], idx, verbose);

        for (auto& t : thrds)
            t.join();
    }

}
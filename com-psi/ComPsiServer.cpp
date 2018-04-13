#include "ComPsiServer.h"

namespace osuCrypto
{
    void ComPsiServer::init(u64 idx, Session & prev, Session & next)
    {
        mIdx = idx;
        mNext = next.addChannel();
        mPrev = prev.addChannel();

        aby3::Sh3::CommPkg comm{ prev.addChannel(), next.addChannel() };
        mRt.init(idx, comm);

        mPrng.SetSeed(ZeroBlock);
        mEnc.init(idx, toBlock(idx), toBlock((idx + 1) % 3));

        std::string filename = "./lowMCCircuit_b" + ToString(sizeof(LowMC2<>::block)) + "_k" + ToString(sizeof(LowMC2<>::keyblock)) + ".bin";

        std::ifstream in;
        in.open(filename, std::ios::in | std::ios::binary);

        if (in.is_open() == false)
        {
            LowMC2<> cipher1(1);
            cipher1.to_enc_circuit(mLowMCCir);

            std::ofstream out;
            out.open(filename, std::ios::trunc | std::ios::out | std::ios::binary);
            mLowMCCir.levelByAndDepth();

            mLowMCCir.writeBin(out);
        }
        else
        {
            mLowMCCir.readBin(in);
        }

    }

    SharedTable ComPsiServer::localInput(Table & t)
    {
        SharedTable ret;
        //ret.mKeys.resize(t.mKeys.rows(), mKeyBitCount);
        //mEnc.localPackedBinary(mRt.mComm, t.mKeys, ret.mKeys);
        ret.mKeys.resize(t.mKeys.rows(), mKeyBitCount / 64);
        mEnc.localBinMatrix(mRt.mComm, t.mKeys, ret.mKeys);

        return ret;
    } 

    SharedTable ComPsiServer::remoteInput(u64 partyIdx, u64 numRows)
    {
        SharedTable ret;
        //ret.mKeys.resize(numRows, mKeyBitCount);
        //mEnc.remotePackedBinary(mRt.mComm, ret.mKeys);
        ret.mKeys.resize(numRows, mKeyBitCount / 64);
        mEnc.remoteBinMatrix(mRt.mComm, ret.mKeys);
        return ret;
    }

    SharedTable ComPsiServer::intersect(SharedTable & A, SharedTable & B)
    {
        std::array<SharedTable*, 2> AB{ &A,&B };
        std::array<u64, 2> reveals{ 0,1 };
        aby3::Sh3::i64Matrix keys = computeKeys(AB, reveals);



        throw std::runtime_error(LOCATION);
    }
    aby3::Sh3::i64Matrix ComPsiServer::computeKeys(span<SharedTable*> tables, span<u64> reveals)
    {
        aby3::Sh3::i64Matrix ret;
        std::vector<aby3::Sh3BinaryEvaluator> binEvals(tables.size());

        auto blockSize = mLowMCCir.mInputs[0].size();
        auto rounds = mLowMCCir.mInputs.size() - 1;

        aby3::Sh3::sb64Matrix oprfRoundKey(1, blockSize / 64), temp;
        for (u64 i = 0; i < tables.size(); ++i)
        {
            //auto shareCount = tables[i]->mKeys.shareCount();
            auto shareCount = tables[i]->mKeys.rows();

            if (i == 0)
            {
                binEvals[i].enableDebug(mIdx, mPrev, mNext);
            }
            binEvals[i].setCir(&mLowMCCir, shareCount);

            binEvals[i].setInput(0, tables[i]->mKeys);

            for (u64 j = 0; j < rounds; ++j)
            {
                mEnc.rand(oprfRoundKey);
                temp.resize(shareCount, blockSize / 64);
                for (u64 k = 0; k < shareCount; ++k)
                {
                    temp.mShares[0].row(k) = oprfRoundKey.mShares[0].row(0);
                    temp.mShares[1].row(k) = oprfRoundKey.mShares[1].row(0);
                }

                binEvals[i].setInput(j + 1, temp);
            }


            binEvals[i].asyncEvaluate(mRt.noDependencies());
        }

        // actaully runs the computations
        mRt.runAll();


        std::vector<aby3::Sh3::sPackedBin> temps(tables.size());

        for (u64 i = 0; i < tables.size(); ++i)
        {
            //auto shareCount = tables[i]->mKeys.shareCount();
            auto shareCount = tables[i]->mKeys.rows();
            temps[i].resize(shareCount, blockSize);

            if (reveals[i] == mIdx)
            {
                if (ret.size())
                    throw std::runtime_error("only one output per party. " LOCATION);
                ret.resize(shareCount, blockSize / 64);


                binEvals[i].getOutput(0, temps[i]);
                mEnc.reveal(mRt.noDependencies(), temps[i], ret);
            }
            else
            {
                binEvals[i].getOutput(0, temps[i]);
                mEnc.reveal(mRt.noDependencies(), reveals[i], temps[i]);
            }
        }

        // actaully perform the reveals
        mRt.runAll();

        return ret;
    }
}

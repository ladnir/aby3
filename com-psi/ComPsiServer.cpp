#include "ComPsiServer.h"

namespace osuCrypto
{
    void ComPsiServer::init(u64 idx, Session & next, Session & prev)
    {
        mIdx = idx;
        mNext = next.addChannel();
        mPrev = prev.addChannel();
        mPrng.SetSeed(ZeroBlock);
        mEng.init(idx, prev, next, 0, mPrng.get<block>());
        //mBinEng.init(idx, prev, next);
    }

    SharedTable ComPsiServer::input(Table & t)
    {
        SharedTable ret;
        mEng.localInput(t.mKeys, ret.mKeys);
        return ret;
    }

    SharedTable ComPsiServer::input(u64 idx)
    {
        SharedTable ret;
        mEng.remoteInput(idx, ret.mKeys);
        return ret;
    }

    SharedTable ComPsiServer::intersect(SharedTable & A, SharedTable & B)
    {
        std::array<SharedTable*, 2> AB{ &A,&B };
        std::array<u64, 2> reveals{ 0,1 };
        std::vector<block> keys = computeKeys(AB, reveals);



        throw std::runtime_error(LOCATION);
    }
    std::vector<block> ComPsiServer::computeKeys(span<SharedTable*> tables, span<u64> reveals)
    {
        std::vector<block> ret;
        std::vector<Lynx::BinaryEngine> binEng(tables.size());

        std::vector<Lynx::Matrix> oprfKey = getRandOprfKey();
        std::vector<Lynx::PackedBinMatrix> binKeys(tables.size());
        std::vector<Lynx::CompletionHandle> handles(tables.size());

        for (u64 i = 0; i < tables.size(); ++i)
            handles[i] = mEng.asyncConvertToPackedBinary(tables[i]->mKeys, binKeys[i], { mNext, mPrev});


        for (u64 i = 0; i < tables.size(); ++i)
        {
            binEng[i].init(mIdx, mPrev, mNext);
            binEng[i].setCir(&mLowMCCir, tables[i]->rows());

            for (u64 j = 0; j < mLowMC.roundkeys.size(); ++j)
            {
                binEng[i].setReplicatedInput(j, oprfKey[j]);
            }
        }

        for (u64 i = 0; i < tables.size(); ++i)
        {
            handles[i].get();
            binEng[i].setInput(mLowMC.roundkeys.size(), binKeys[i]);
        }

        while (binEng[0].hasMoreRounds())
        {
            for (u64 i = 0; i < tables.size(); ++i)
            {
                binEng[i].evaluateRound();
            }
        }

        std::vector<Lynx::PackedBinMatrix> outs(tables.size());
        for (u64 i = 0; i < tables.size(); ++i)
        {
            binEng[i].getOutput(0, outs[i]);
            auto r = mEng.reconstructShare(outs[i], reveals[i]);

            if (r.size() != 0 && r.cols() != 2)
                throw std::runtime_error(LOCATION);

            for (u64 j = 0; j < r.rows(); ++j)
            {
                auto& b = *(block*)&r(j * 2);
                ret.push_back(b);
            }

        }


    }
}

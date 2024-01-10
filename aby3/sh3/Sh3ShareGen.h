#pragma once
#include "Sh3Types.h"
#include <cryptoTools/Crypto/PRNG.h>

namespace aby3
{
    struct Sh3ShareGen
    {
        void init(block prevSeed, block nextSeed, u64 buffSize = 256)
        {
			mCommon.SetSeed(oc::toBlock(3488535245, 2454523));
			mNextCommon.SetSeed(nextSeed);
			mPrevCommon.SetSeed(prevSeed);

            mShareGenIdx = 0;
            mShareBuff[0].resize(buffSize);
            mShareBuff[1].resize(buffSize);

            mShareGen[0].setKey(mPrevCommon.get<block>());
            mShareGen[1].setKey(mNextCommon.get<block>());

            refillBuffer();
        }

        void init(CommPkg& comm, block& seed, u64 buffSize = 256)
        {
            comm.mNext.asyncSendCopy(seed);
            block prevSeed;
            comm.mPrev.recv(prevSeed);
            init(prevSeed, seed, buffSize);
        }

        void validate(CommPkg& comm)
        {
            auto next = mNextCommon.get<block>();
            auto prev = mPrevCommon.get<block>();

            comm.mNext.send(next);
            block pp;
            comm.mPrev.recv(pp);

            if (pp != prev)
                throw RTE_LOC;
        }

        u64 mShareIdx = 0, mShareGenIdx = 0;
		oc::PRNG mNextCommon, mPrevCommon, mCommon;
        std::array<oc::AES, 2> mShareGen;
        std::array<std::vector<block>, 2> mShareBuff;
        void refillBuffer()
        {
            mShareGen[0].ecbEncCounterMode(mShareGenIdx, mShareBuff[0].size(), mShareBuff[0].data());
            mShareGen[1].ecbEncCounterMode(mShareGenIdx, mShareBuff[1].size(), mShareBuff[1].data());
            mShareGenIdx += mShareBuff[0].size();
            mShareIdx = 0;
        }



        i64 getShare()
        {
            if (mShareIdx + sizeof(i64) > mShareBuff[0].size() * sizeof(block))
            {
                refillBuffer();
            }

            i64 ret
                = *(u64*)((u8*)mShareBuff[0].data() + mShareIdx)
                - *(u64*)((u8*)mShareBuff[1].data() + mShareIdx);

            mShareIdx += sizeof(i64);

            //ret = 0;
            return ret;
        }

        i64 getBinaryShare()
        {
            if (mShareIdx + sizeof(i64) > mShareBuff[0].size() * sizeof(block))
            {
                refillBuffer();
            }

            i64 ret
                = *(u64*)((u8*)mShareBuff[0].data() + mShareIdx)
                ^ *(u64*)((u8*)mShareBuff[1].data() + mShareIdx);

            mShareIdx += sizeof(i64);

            //ret = 0;
            return ret;
        }


        si64 getRandIntShare()
        {
            if (mShareIdx + sizeof(i64) > mShareBuff[0].size() * sizeof(block))
            {
                refillBuffer();
            }

            si64 r;
            r[0] = *(u64*)((u8*)mShareBuff[1].data() + mShareIdx);
            r[1] = *(u64*)((u8*)mShareBuff[0].data() + mShareIdx);

            mShareIdx += sizeof(i64);

            return r;
        }

        //si64 getRandIntShare(int neighbor) {
        //    if (mShareIdx + 2*sizeof(i64) > mShareBuff[neighbor].size()*sizeof(block))
        //    {
        //        refillBuffer();
        //    }
        //    si64 r;
        //    r[0] = *(u64*)((u8*)mShareBuff[neighbor].data() + mShareIdx); 
        //    r[1] = *(u64*)((u8*)mShareBuff[neighbor].data() + mShareIdx); 
        //    mShareIdx += 2*sizeof(i64);
        //    
        //    return r;
        //}

        sb64 getRandBinaryShare()
        {
            auto i = getRandIntShare();
            return { { i[0], i[1] } };
        }
    };
}
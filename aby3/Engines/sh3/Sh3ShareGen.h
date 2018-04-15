#pragma once
#include "Sh3Types.h"
#include <cryptoTools/Crypto/AES.h>

namespace aby3
{
    struct Sh3ShareGen
    {
        void init(block prevSeed, block nextSeed, u64 buffSize = 256)
        {

            mShareGenIdx = 0;
            mShareBuff[0].resize(buffSize);
            mShareBuff[1].resize(buffSize);

            mShareGen[0].setKey(prevSeed);
            mShareGen[1].setKey(nextSeed);

            refillBuffer();
        }

        void init(Sh3::CommPkg& comm, block& seed, u64 buffSize = 256)
        {
            comm.mNext.asyncSendCopy(seed);
            block prevSeed;
            comm.mPrev.recv(prevSeed);
            init(prevSeed, seed, buffSize);
        }


        u64 mShareIdx = 0, mShareGenIdx = 0;
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


        Sh3::si64 getRandIntShare()
        {
            if (mShareIdx + sizeof(i64) > mShareBuff[0].size() * sizeof(block))
            {
                refillBuffer();
            }

            Sh3::si64 r;
            r[0] = *(u64*)((u8*)mShareBuff[1].data() + mShareIdx);
            r[1] = *(u64*)((u8*)mShareBuff[0].data() + mShareIdx);

            mShareIdx += sizeof(i64);

            return r;
        }

        Sh3::sb64 getRandBinaryShare()
        {
            auto i = getRandIntShare();
            return { { i[0], i[1] } };
        }
    };
}
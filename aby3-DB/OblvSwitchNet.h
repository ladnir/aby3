#pragma once
#include <cryptoTools/Common/Defines.h>
#include <cryptoTools/Common/Matrix.h>
#include <cryptoTools/Network/Channel.h>
#include "OblvPermutation.h"

namespace osuCrypto
{
    class PRNG;


    class OblvSwitchNet
    {
    public:
        struct Node
        {
            u32 mSrcIdx;
            span<u32> mSrcDests;
        };

        struct Program
        {
            struct SrcDest
            {
                u32 mSrc,
                    mDest;
            };
            std::vector<u32> mCounts;
            std::vector<SrcDest> mSrcDests;
            //std::vector<Node> mNodes;

            void reset()
            {
                mNextFreeIdx = 0;
                mNextFreeNodeIdx = 0;
            }

            void finalize();

            void init(u32 srcSize, u32 destSize);

            void addSwitch(u32 srcIdx, u32 destSize);

            u32 mNextFreeIdx = 0;
            u32 mNextFreeNodeIdx = 0;
            u32 mSrcSize = 0;
            u32 nextFree();

            void validate();
        };

        OblvSwitchNet() = default;

        std::string mTag;
        OblvSwitchNet(std::string tag)
            :mTag(tag)
        {}

        void sendRecv(Channel& programChl, Channel& helpChl, Matrix<u8> src, MatrixView<u8> dest);
        void help(Channel& programChl, Channel& sendrChl, PRNG& prng, u32 destRows, u32 srcRows, u32 bytes);
        void program(
            Channel& helpChl, 
            Channel& sendrChl,
            Program& prog, 
            PRNG& prng, 
            MatrixView<u8> dest,
            OutputType type = OutputType::Overwrite);



        void sendSelect(Channel& programChl, Channel& helpChl, Matrix<u8> src);
        void recvSelect(Channel& programChl, Channel& sendrChl, MatrixView<u8> dest, u64 srcRows);
        void programSelect(
            Channel& helpChl,
            Channel& sendrChl,
            Program& prog,
            PRNG& prng,
            MatrixView<u8> dest);


        void sendDuplicate(Channel& programChl, PRNG& prng, MatrixView<u8> srcDest);
        void helpDuplicate(Channel& programChl, u64 rows, u32 bytes);
        void programDuplicate(
            Channel& sendrChl,
            Channel& helperChl,
            Program& prog,
            PRNG& prng,
            MatrixView<u8> srcDest);

    };

}
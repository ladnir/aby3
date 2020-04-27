#pragma once
#include <cryptoTools/Common/Defines.h>
#include <cryptoTools/Common/Matrix.h>
#include <cryptoTools/Network/Channel.h>
namespace osuCrypto
{
    class PRNG;
    enum class OutputType
    {
        Overwrite,
        Additive
    };

    class OblvPermutation
    {
    public:



        void send(Channel& programChl, Channel& revcrChl, Matrix<u8> src, std::string tag);
        void recv(Channel& programChl, Channel& sendrChl, MatrixView<u8> dest, u64 srcRows, std::string tag, OutputType type = OutputType::Overwrite);
        void program(Channel& revcrChl, Channel& sendrChl, std::vector<u32> perm, PRNG& prng, MatrixView<u8> dest, std::string tag, OutputType type = OutputType::Overwrite);


        struct AsyncRecv
        {

            //u32 recvCount;// = gsl::narrow<u32>((srcRows + step - 1) / step);
            std::vector<std::pair<std::future<void>, std::vector<u8>>>  recvs1;//(recvCount);
            std::vector<std::pair<std::future<void>, std::vector<u32>>> recvs2;//(recvCount);


            MatrixView<u8> mDest;
            u64 mSrcRows;
            std::string mTag;
            OutputType mType;

            AsyncRecv(
                Channel& programChl,
                Channel& sendrChl,
                MatrixView<u8> dest,
                u64 srcRows,
                std::string tag,
                OutputType type);

            void get();
        };

        AsyncRecv asyncRecv(Channel& programChl, Channel& sendrChl, MatrixView<u8> dest, u64 srcRows, std::string tag, OutputType type = OutputType::Overwrite);

    };

}

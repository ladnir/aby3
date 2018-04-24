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



        void send(Channel& programChl, Channel& revcrChl, Matrix<u8> src);
        void recv(Channel& programChl, Channel& sendrChl, MatrixView<u8> dest, OutputType type = OutputType::Overwrite);
        void program(Channel& revcrChl, Channel& sendrChl, std::vector<u32> perm, PRNG& prng, MatrixView<u8> dest, OutputType type = OutputType::Overwrite);

    };

}

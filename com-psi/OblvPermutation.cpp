#include "OblvPermutation.h"
#include <cryptoTools/Crypto/PRNG.h>
#include <cryptoTools/Common/Matrix.h>

namespace osuCrypto
{

    void OblvPermutation::send(Channel & programChl, Channel & recvrChl, Matrix<u8> src)
    {
        u32 rows = src.rows();
        //std::vector<u32> pi1(src.rows());
        //for (u32 i = 0; i < pi1.size(); ++i)
        //{
        //    pi1[i] = i;
        //}

        u32 step = 1 << 14;
        block seed = ZeroBlock;
        //programChl.recv(seed);
        PRNG prng(seed, 256);
        for (u32 i = 0, j = rows; i < rows;)
        {
            auto min = std::min<u32>(rows - i, step);
            auto start = i;
            auto size = min * src.cols();;

            while (min--)
            {

                auto idx = prng.get<u32>() % j + i;
                //std::swap(pi1[i], pi1[idx]);

                //std::cout << pi1[i] << " -> " << i << " -> ___" << std::endl;

                //std::cout << "data[" << i << "] = ";
                for (u32 k = 0; k < src.cols(); ++k)
                {
                    std::swap(src(i, k), src(idx, k));

                    //std::cout << " " << std::hex << int(src(i, k));
                    //}
                    ////std::cout << std::endl;
                    //for (u32 k = 0; k < src.cols(); ++k)
                    //{
                    src(i, k) ^= prng.get<u8>();
                    //    std::cout << " " << std::hex << int(src(i, k));
                }
                //std::cout << std::endl << std::dec;

                ++i, --j;
            }

            auto data = src.data() + start * src.cols();

            if (i == rows)
                recvrChl.asyncSend(data, size, [a = std::move(src)](){});
            else
                recvrChl.asyncSend(data, size);
        }
    }

    void OblvPermutation::recv(
        Channel & programChl,
        Channel & sendrChl,
        MatrixView<u8> dest,
        OutputType type)
    {
        u32 rows = dest.rows();
        u32 step = 1 << 14;
        u32 recvCount = (rows + step - 1) / step;

        std::vector<std::pair<std::future<void>, std::vector<u8>>> recvs1(recvCount);
        std::vector<std::pair<std::future<void>, std::vector<u32>>> recvs2(recvCount);
        for (u32 i = 0; i < recvCount; ++i)
        {
            recvs1[i].first = sendrChl.asyncRecv(recvs1[i].second);
            recvs2[i].first = programChl.asyncRecv(recvs2[i].second);

        }

        for (u32 i = 0; i < recvCount; ++i)
        {
            recvs1[i].first.get();
            recvs2[i].first.get();

            auto& perm = recvs2[i].second;
            auto& data = recvs1[i].second;
            if (perm.size() * dest.stride() != data.size())
                throw std::runtime_error(LOCATION);

            auto iter = data.data();
            if (type == Overwrite)
            {
                for (u32 j = 0; j < perm.size(); ++j)
                {
                    u8* destPtr = &*(dest.begin() + perm[j] * dest.stride());

                    //std::cout << "____ -> " << j << " -> " << perm[j] << std::endl;

                    memcpy(destPtr, iter, dest.stride());
                    iter += dest.stride();
                }
            }
            else
            {
                for (u32 j = 0; j < perm.size(); ++j)
                {
                    u8* destPtr = &*(dest.begin() + perm[j] * dest.stride());

                    for (u32 k = 0; k < dest.stride(); ++k)
                    {
                        destPtr[k] ^= *iter;
                        ++iter;
                    }
                }
            }
        }
    }


    void OblvPermutation::program(Channel & recvrChl, Channel & sendrChl, std::vector<u32> perm, PRNG& p, MatrixView<u8> dest, OutputType type)
    {
        auto rows = dest.rows();
        //std::vector<u32> pi1(rows);
        //for (u32 i = 0; i <rows; ++i)
        //{
        //    pi1[i] = i;
        //}

        u32 step = 1 << 14;
        block seed = ZeroBlock;// p.get<block>();
        //sendrChl.asyncSend(seed);
        PRNG prng(seed, 256);


        for (u32 i = 0, j = rows; i < rows;)
        {
            auto min = std::min<u32>(rows - i, step);
            auto start = i;
            auto size = min;

            while (min--)
            {

                auto idx = prng.get<u32>() % j + i;
                std::swap(perm[i], perm[idx]);


                //std::cout << "perm'[" << i << "] = " << perm[i] << std::endl;
                auto destPtr = &dest(perm[i], 0);

                if (type == Overwrite)
                {
                    prng.get(destPtr, dest.stride());
                }
                else
                {
                    for (u32 k = 0; k < dest.stride(); ++k)
                    {
                        destPtr[k] ^= prng.get<u8>();
                    }
                }
                ++i, --j;
            }

            u32* data = perm.data() + start;
            if (i == rows)
                recvrChl.asyncSend(data, size, [d = std::move(perm)](){});
            else
                recvrChl.asyncSend(data, size);
        }

    }
}

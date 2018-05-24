#include "OblvPermutation.h"
#include <cryptoTools/Crypto/PRNG.h>
#include <cryptoTools/Common/Matrix.h>
#include <unordered_set>
namespace osuCrypto
{

    void OblvPermutation::send(
        Channel & programChl, 
        Channel & recvrChl, 
        Matrix<u8> src,
        std::string tag)
    {
        u32 rows = src.rows();
        //std::vector<u32> pi1(src.rows());
        //for (u32 i = 0; i < pi1.size(); ++i)
        //{
        //    pi1[i] = i;
        //}

        u32 step = 1 << 14;
        block seed = ZeroBlock;

        //ostreamLock(std::cout) << "send count " << (rows - 1 + step) / step << " (" << rows << ") " << tag << std::endl;


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
                    auto c = prng.get<u8>();
                    src(i, k) ^= c;

                    //if (k == 0)
                    //{
                    //    std::cout << "src[" << i << "] = " << int(c) << "\t" << int(src(i,k)) << std::endl;
                    //}
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

        //std::cout << "p0" << prng.get<int>() << std::endl;
    }

    void OblvPermutation::recv(
        Channel & programChl,
        Channel & sendrChl,
        MatrixView<u8> dest, 
        u64 srcRows,
        std::string tag,
        OutputType type)
    {
        u32 step = 1 << 14;
        u32 recvCount = (srcRows + step - 1) / step;

        std::vector<std::pair<std::future<void>, std::vector<u8>>> recvs1(recvCount);
        std::vector<std::pair<std::future<void>, std::vector<u32>>> recvs2(recvCount);
        for (u32 i = 0; i < recvCount; ++i)
        {
            recvs1[i].first = sendrChl.asyncRecv(recvs1[i].second);
            recvs2[i].first = programChl.asyncRecv(recvs2[i].second);

        }
        //ostreamLock(std::cout) << "recv count " << recvCount << " (" << srcRows <<") " << tag << std::endl;

        for (u32 i = 0; i < recvCount; ++i)
        {
            recvs1[i].first.get();
            recvs2[i].first.get();


            auto& perm = recvs2[i].second;
            auto& data = recvs1[i].second;
            if (perm.size() * dest.stride() != data.size())
                throw std::runtime_error(LOCATION);

            //ostreamLock(std::cout) << "recv size[" << i << "] -> " << data.size()<< " "<< tag << std::endl;

            auto iter = data.data();
            if (type == OutputType::Overwrite)
            {
                for (u32 j = 0; j < perm.size(); ++j)
                {
                    if (perm[j] != -1)
                    {

                        u8* destPtr = &*(dest.begin() + perm[j] * dest.stride());

                        //std::cout << "____ -> " << j << " -> " << perm[j] << std::endl;

                        memcpy(destPtr, iter, dest.stride());
                    }
                        iter += dest.stride();
                }
            }
            else
            {
                for (u32 j = 0; j < perm.size(); ++j)
                {
                    if (perm[j] != -1)
                    {
                        u8* destPtr = &*(dest.begin() + perm[j] * dest.stride());

                        for (u32 k = 0; k < dest.stride(); ++k)
                        {
                            destPtr[k] ^= *iter;
                            ++iter;
                        }
                    }
                    else
                    {
                        iter += dest.stride();
                    }
                }
            }
        }
    }



    void OblvPermutation::program(
        Channel & recvrChl, 
        Channel & sendrChl, 
        std::vector<u32> perm, 
        PRNG& p, 
        MatrixView<u8> dest,
        std::string tag,
        OutputType type)
    {
        auto rows = perm.size();

#ifndef NDEBUG
        if (true)
        {
            std::unordered_set<u32> set;
            set.reserve(dest.size());
            for (auto p : perm)
            {
                if (p != -1)
                {
                    auto r = set.emplace(p);
                    if (r.second == false)
                        throw std::runtime_error("");
                }
            }

        }
#endif

        //std::vector<u32> pi1(rows);
        //for (u32 i = 0; i <rows; ++i)
        //{
        //    pi1[i] = i;
        //}

        u32 step = 1 << 14;
        block seed = ZeroBlock;// p.get<block>();
        //sendrChl.asyncSend(seed);
        PRNG prng(seed, 256);

        //ostreamLock(std::cout) << "prog count " << (rows - 1 + step) / step << " (" << rows << ") " << tag << std::endl;


        std::vector<u8> temp(dest.stride());
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
                if (perm[i] != -1)
                {
                    auto destPtr = &dest(perm[i], 0);

                    if (type == OutputType::Overwrite)
                    {
                        prng.get(destPtr, dest.stride());
                        //std::cout << "dest[" << perm[i]<<", "<<i << "] = " << int(destPtr[0]) << std::endl;
                    }
                    else
                    {
                        for (u32 k = 0; k < dest.stride(); ++k)
                        {
                            destPtr[k] ^= prng.get<u8>();
                        }
                    }
                }
                else
                {
                    prng.get(temp.data(), temp.size());
                    //std::cout << "dest[" << perm[i] << ", " << i << "] = " << int(temp[0]) << std::endl;
                }
                ++i, --j;
            }

            u32* data = perm.data() + start;
            if (i == rows)
                recvrChl.asyncSend(data, size, [d = std::move(perm)](){});
            else
                recvrChl.asyncSend(data, size);

            //ostreamLock(std::cout) << "send size[" << i << "] -> " << size <<" " << tag << std::endl;
        }

        ///std::cout << "p0" << prng.get<int>() << std::endl;

    }
}

#include "OblvSwitchNet.h"
#include <cryptoTools/Common/BitIterator.h>
#include <cryptoTools/Crypto/PRNG.h>
#include <iomanip>

namespace osuCrypto
{
    void OblvSwitchNet::sendRecv(Channel & programChl, Channel & helpChl, Matrix<u8> src, MatrixView<u8> dest)
    {
        std::stringstream ss;
        ss << "send_" << src.rows() <<"_"<< dest.rows() << "_" << src.cols();

        auto s = ss.str();
        programChl.asyncSendCopy(s);
        programChl.recv(s);
        if (s != ss.str())
            throw RTE_LOC;

        TODO("make sure this resize is a no-op by reserving enough before...");
        if (src.rows() < dest.rows())
            src.resize(dest.rows(), src.cols());

        sendSelect(programChl, helpChl, std::move(src));
        helpDuplicate(programChl, dest.rows(), dest.cols());

        OblvPermutation oblvPerm;
        oblvPerm.recv(programChl, helpChl, dest, dest.rows(), mTag + "_sendRecv_final");
    }

    void OblvSwitchNet::help(Channel& programChl, Channel& sendrChl, PRNG& prng, u32 destRows, u32 srcRows, u32 bytes)
    {
        std::stringstream ss;
        ss << "help_" << destRows << "_" << bytes;

        auto s = ss.str();
        programChl.asyncSendCopy(s);
        programChl.recv(s);

        if (s != ss.str())
            throw RTE_LOC;


        Matrix<u8> temp(destRows, bytes);
        recvSelect(programChl, sendrChl, temp, srcRows);

        //programChl.recv(s);
        //if (s != "test")
        //    throw RTE_LOC;

        sendDuplicate(programChl, prng, temp);

        OblvPermutation oblvPerm;
        oblvPerm.send(programChl, sendrChl, std::move(temp), mTag + "_help_final");
    }

    void OblvSwitchNet::program(
        Channel & helpChl, 
        Channel & sendrChl, 
        Program & prog, 
        PRNG & prng, 
        MatrixView<u8> dest,
        OutputType type)
    {
        {
            std::stringstream ss;
            ss << "send_" << prog.mSrcSize << "_" << dest.rows() << "_" << dest.cols();

            sendrChl.asyncSendCopy(ss.str());

            std::string str;
            sendrChl.recv(str);
            if (str != ss.str())
            {
                std::cout << "exp: " << ss.str() << std::endl;
                std::cout << "act: " << str << std::endl;

                throw std::runtime_error("");
            }
        }

        {
            std::stringstream ss;
            ss << "help_" << dest.rows() << "_" << dest.cols();

            helpChl.asyncSendCopy(ss.str());

            std::string str;
            helpChl.recv(str);
            if (str != ss.str())
            {
                std::cout << "exp: " << ss.str() << std::endl;
                std::cout << "act: " << str << std::endl;

                throw std::runtime_error("");
            }
        }


        Matrix<u8> temp(dest.rows(), dest.cols());

        programSelect(helpChl, sendrChl, prog, prng, temp);

        //helpChl.send("test");

        programDuplicate(helpChl, sendrChl, prog, prng, temp);

        std::vector<u32> perm(prog.mSrcDests.size());

        if (type == OutputType::Overwrite)
        {
            for (auto i = 0; i < perm.size(); ++i)
            {
                perm[i] = prog.mSrcDests[i].mDest;
                memcpy(&dest(perm[i], 0), &temp(i, 0), dest.cols());
            }
        }
        else
        {
            for (auto i = 0; i < perm.size(); ++i)
            {
                perm[i] = prog.mSrcDests[i].mDest;
                auto srcPtr = &temp(i, 0);
                auto destPtr = &dest(perm[i], 0);
                for (u64 j = 0; j < dest.cols(); ++j)
                {
                    destPtr[j] ^= srcPtr[j];
                }
            }
        }

        OblvPermutation oblvPerm;
        oblvPerm.program(sendrChl, helpChl, std::move(perm), prng, dest, mTag + "_prog_final", OutputType::Additive);
    }



    void OblvSwitchNet::sendSelect(Channel & programChl, Channel & helpChl, Matrix<u8> src)
    {
        OblvPermutation oblvPerm;
        oblvPerm.send(programChl, helpChl, std::move(src), mTag + "_send_select");
    }

    void OblvSwitchNet::recvSelect(Channel & programChl, Channel & sendrChl, MatrixView<u8> dest, u64 srcRows)
    {
        OblvPermutation oblvPerm;
        oblvPerm.recv(programChl, sendrChl, dest, srcRows, mTag + "_recv_select");
    }


    void OblvSwitchNet::programSelect(
        Channel & recvrChl,
        Channel & sendrChl,
        Program& prog,
        PRNG& prng,
        MatrixView<u8> dest)
    {
        if (prog.mCounts.size())
            prog.finalize();
        prog.validate();
        prog.reset();



        auto size = std::max<u64>(prog.mSrcDests.size(), prog.mSrcSize);
        std::vector<u32> perm1(size, -1);
        auto switchIdx = 0;

        while (switchIdx < prog.mSrcDests.size())
        {
            auto src = prog.mSrcDests[switchIdx].mSrc;

            perm1[src] = switchIdx++;

            while (switchIdx < prog.mSrcDests.size() &&
                prog.mSrcDests[switchIdx - 1].mSrc == prog.mSrcDests[switchIdx].mSrc)
            {
                auto src = prog.nextFree();
                perm1[src] = switchIdx++;
            }
        }


        //for (auto i = 0; i < perm1.size(); ++i)
        //{
        //    //if (perm1[i] == -1)
        //    //    perm1[i] = prog.nextFree();

        //    std::cout << "perm[" << i << "] = " << perm1[i] << std::endl;
        //}

        OblvPermutation oblvPerm;
        oblvPerm.program(recvrChl, sendrChl, std::move(perm1), prng, dest, mTag + "_prog_select");


    }

    void OblvSwitchNet::Program::finalize()
    {
        auto counts = std::move(mCounts);
        auto k = counts.size();
        auto n = mSrcDests.size();

        // Compute partial sums.
        for (int i = 0, sum = 0; i < k; ++i) {
            int tmp = counts[i];
            counts[i] = sum;
            sum += tmp;
        }

        // Move elements.
        for (int i = n - 1; i >= 0; --i) {
            auto& val = mSrcDests[i];
            int j = counts[val.mSrc];

            if (j < i) {
                do {
                    ++counts[val.mSrc];

                    // Swap val and a[j].
                    std::swap(val, mSrcDests[j]);

                    j = counts[val.mSrc];
                } while (j < i);
            }
        }
    }

    void OblvSwitchNet::Program::init(u32 srcSize, u32 destSize)
    {
        mSrcSize = srcSize;
        mSrcDests.resize(0);
        mSrcDests.reserve(destSize);
        mCounts.resize(0);
        mCounts.resize(srcSize, 0);
    }

    void OblvSwitchNet::Program::addSwitch(u32 srcIdx, u32 destIdx)
    {
        mSrcDests.push_back({ srcIdx, destIdx });
        ++mCounts[srcIdx];
    }

    u32 OblvSwitchNet::Program::nextFree()
    {
        while (
            mNextFreeNodeIdx < mSrcDests.size() &&
            mNextFreeIdx == mSrcDests[mNextFreeNodeIdx].mSrc)
        {
            while (
                mNextFreeNodeIdx < mSrcDests.size() &&
                mNextFreeIdx == mSrcDests[mNextFreeNodeIdx].mSrc)
                ++mNextFreeNodeIdx;

            ++mNextFreeIdx;
        }

        return mNextFreeIdx++;
    }


    void OblvSwitchNet::Program::validate()
    {
        for (auto i = 1; i < mSrcDests.size(); ++i)
        {
            if (mSrcDests[i].mSrc < mSrcDests[i - 1].mSrc)
                throw std::runtime_error("");
        }
    }


    void OblvSwitchNet::sendDuplicate(Channel & programChl, PRNG& prng, MatrixView<u8> src)
    {
        auto rows = src.rows();
        PRNG otPrng2(toBlock(645687456));
        std::vector<u8> sendData((rows - 1) * src.stride() * 2);
        otPrng2.get(sendData.data(), sendData.size());

        std::vector<u8> bits((rows + 6) / 8);
        BitIterator iter(bits.data(), 0);

        programChl.recv(bits.data(), bits.size());

        std::array<u8*, 2> m{ sendData.data() , sendData.data() + src.stride() };

        for (u64 i = 1; i < rows; ++i)
        {
            bool p = *iter;
            ++iter;

            auto i0 = i - p;
            auto i1 = i - !p;

            u32 m0 = m[0][0];
            u32 m1 = m[1][0];
            u32 s0 = src(i0, 0);
            u32 s1 = src(i1, 0);

            for (u32 j = 0; j < src.stride(); ++j)
            {
                m[0][j] ^= src(i0, j);
                m[1][j] ^= src(i1, j);
            }

            prng.get(&src(i, 0), src.stride());

            u32 ss0 = src(i, 0);

            for (u32 j = 0; j < src.stride(); ++j)
            {
                m[0][j] ^= src(i, j);
                m[1][j] ^= src(i, j);
            }

            //ostreamLock (std::cout)            
            //    << "s[" << i << ", 0] = src[" << i0 << "] ^ w[" << i << ", 0] ^ s'[" << i << "]\n"
            //    << "s[" << i << ", 0] = " <<std::setw(6) << s0 << " ^ " <<std::setw(7)<< m0 << " ^ " << std::setw(7) << ss0 << " = " << int(m[0][1])
            //    << "\n"
            //    << "s[" << i << ", 1] = src[" << i1 << "] ^ w[" << i << ", 1] ^ s'[" << i << "]\n"
            //    << "s[" << i << ", 1] = " << std::setw(6) << s1 << " ^ " << std::setw(7) << m1 << " ^ " << std::setw(7) << ss0 <<" = " <<int(m[1][1]) <<"\n"<< std::endl;


            m[0] += 2 * src.stride();
            m[1] += 2 * src.stride();
        }

        if (m[0] != sendData.data() + sendData.size())
            throw std::runtime_error("");

        programChl.asyncSend(std::move(sendData));
    }

    void OblvSwitchNet::helpDuplicate(Channel & programChl, u64 rows, u32 bytes)
    {
        PRNG otPrng2(toBlock(645687456));
        PRNG otPrng1(toBlock(345345));
        std::vector<u8> bits((rows + 6) / 8);
        otPrng1.get(bits.data(), bits.size());
        BitIterator iter(bits.data(), 0);

        std::vector<u8> share0((rows - 1) * bytes), temp(bytes);
        std::array<u8*, 2> ptrs{ share0.data(), temp.data() };


        //ostreamLock o(std::cout);
        for (u32 i = 1; i < rows; ++i)
        {
            auto p = *iter;
            auto m0 = ptrs[p];
            auto m1 = ptrs[!p];

            otPrng2.get(m0, bytes);
            otPrng2.get(m1, bytes);

            //o << "w[" << i  << ", " << *iter << "] = " << int(ptrs[0][0]) << std::endl;

            ptrs[0] += bytes;
            ++iter;
        }


        if (ptrs[0] != share0.data() + share0.size())
            throw std::runtime_error("");
        programChl.asyncSend(std::move(share0));
    }

    void OblvSwitchNet::programDuplicate(
        Channel & sendrChl,
        Channel & helperChl,
        Program& prog,
        PRNG& prng,
        MatrixView<u8> dest)
    {

        PRNG otPrng(toBlock(345345));

        auto rows = dest.rows();
        std::vector<u8> bits((rows + 6) / 8);
        otPrng.get(bits.data(), bits.size());

        BitIterator iter(bits.data(), 0);

        {
            //ostreamLock o(std::cout);
            for (u32 i = 1; i < rows; ++i)
            {
                bool dup = (prog.mSrcDests[i - 1].mSrc == prog.mSrcDests[i].mSrc);

                //o << "r[" << i << "] = " << *iter << "\n";
                //o << "p[" << i << "] = " << int(dup) << "\n";

                *iter ^= dup;
                ++iter;
            }
        }

        sendrChl.asyncSend(bits);
        iter = BitIterator(bits.data(), 0);




        std::vector<u8> share0, share1;
        auto fu0 = helperChl.asyncRecv(share0);
        auto fu1 = sendrChl.asyncRecv(share1);

        fu0.get();
        fu1.get();
        if (share0.size() != dest.size() - dest.stride())
            throw std::runtime_error("");
        if (share1.size() != 2 * (dest.size() - dest.stride()))
            throw std::runtime_error("");

        //auto destPtr = dest.data() + dest.stride();
        auto s0Ptr = share0.data();
        auto s1Ptr = share1.data();

        for (u32 i = 1; i < rows; ++i)
        {
            bool dup = (prog.mSrcDests[i - 1].mSrc == prog.mSrcDests[i].mSrc);
            bool p = *iter ^ dup;
            ++iter;

            auto destPtr = &dest(i, 0);
            auto srcPtr = &dest(i - 1, 0);

            memcpy(destPtr, srcPtr, dest.stride() * dup);


            s1Ptr += p * dest.stride();

            //ostreamLock o(std::cout);

            //o   << "dest[" << i << "] = dest[" << i << "] ^ w[" << i << ", " << int(p) << "] ^ s[" << i << ", " << p << "]\n"
            //    << "dest[" << i << "] = " << std::setw(7) << int(destPtr[0]) << " ^ " << std::setw(7) << int(s0Ptr[0]) << " ^ " << std::setw(7) << int(s1Ptr[0]) << " = ";

            for (u32 j = 0; j < dest.stride(); ++j)
                destPtr[j] ^= s0Ptr[j] ^ s1Ptr[j];

            //o << int(destPtr[0]) << std::endl;

            s0Ptr += dest.stride();
            s1Ptr += dest.stride() + (!p) * dest.stride();
        }

    }
}

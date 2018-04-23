#include "OblvSwitchNet.h"
#include "OblvPermutation.h"


namespace osuCrypto
{
    void OblvSwitchNet::sendSelect(Channel & programChl, Channel & revcrChl, Matrix<u8> src)
    {
        OblvPermutation oblvPerm;
        oblvPerm.send(programChl, revcrChl, std::move(src));
    }
    
    void OblvSwitchNet::recvSelect(Channel & programChl, Channel & sendrChl, MatrixView<u8> dest)
    {
        OblvPermutation oblvPerm;
        oblvPerm.recv(programChl, sendrChl, dest);
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


        
        
        std::vector<u32> perm1(prog.mSrcSize, -1);
        auto switchIdx = 0;
        auto permIdx = 0;
        auto destIdx = prog.mSrcDests.size();
        auto next = dest.rows();

        while (switchIdx < prog.mSrcDests.size())
        {
            auto src = prog.mSrcDests[switchIdx][0];

            perm1[src] = switchIdx++;

            while (switchIdx < prog.mSrcDests.size() &&
                prog.mSrcDests[switchIdx - 1][0] == prog.mSrcDests[switchIdx][0])
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
        oblvPerm.program(recvrChl, sendrChl, std::move(perm1), prng, dest);


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
            int j = counts[val[0]];

            if (j < i) {
                do {
                    ++counts[val[0]];

                    // Swap val and a[j].
                    std::swap(val, mSrcDests[j]);

                    j = counts[val[0]];
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
        while (mNextFreeIdx == mSrcDests[mNextFreeNodeIdx][0])
        {
            while (mNextFreeIdx == mSrcDests[mNextFreeNodeIdx][0])
                ++mNextFreeNodeIdx;

            ++mNextFreeIdx;
        }

        return mNextFreeIdx++;
    }


    void OblvSwitchNet::Program::validate()
    {
        for (auto i = 1; i < mSrcDests.size(); ++i)
        {
            if (mSrcDests[i][0] < mSrcDests[i - 1][0])
                throw std::runtime_error("");
        }
    }
}

#include "ComPsiServer.h"
#include <cryptoTools/Common/CuckooIndex.h>
#include "OblvPermutation.h"
#include <iomanip>

namespace osuCrypto
{
    void ComPsiServer::init(u64 idx, Session & prev, Session & next)
    {
        mIdx = idx;
        mNext = next.addChannel();
        mPrev = prev.addChannel();

        aby3::Sh3::CommPkg comm{ prev.addChannel(), next.addChannel() };
        mRt.init(idx, comm);

        mPrng.SetSeed(ZeroBlock);
        mEnc.init(idx, toBlock(idx), toBlock((idx + 1) % 3));

        const auto blockSize = 80;
        const auto rounds = 13;
        const auto sboxCount = 14;
        const auto dataComplex = 30;
        const auto keySize = 128;

        //std::string filename = "./lowMCCircuit_b" + ToString(sizeof(LowMC2<>::block)) + "_k" + ToString(sizeof(LowMC2<>::keyblock)) + ".bin";
        std::stringstream filename;
        filename << "./lowMCCircuit"
            << "_b" << blockSize 
            << "_r" << rounds 
            << "_d" << dataComplex 
            << "_k" << keySize 
            << ".bin";

        std::ifstream in;
        in.open(filename.str(), std::ios::in | std::ios::binary);

        if (in.is_open() == false)
        {
            LowMC2<sboxCount, blockSize, keySize, rounds> cipher1(1);
            cipher1.to_enc_circuit(mLowMCCir);

            std::ofstream out;
            out.open(filename.str(), std::ios::trunc | std::ios::out | std::ios::binary);
            mLowMCCir.levelByAndDepth();

            mLowMCCir.writeBin(out);
        }
        else
        {
            mLowMCCir.readBin(in);
        }

    }

    SharedTable ComPsiServer::localInput(Table & t)
    {
        SharedTable ret;
        //ret.mKeys.resize(t.mKeys.rows(), mKeyBitCount);
        //mEnc.localPackedBinary(mRt.mComm, t.mKeys, ret.mKeys);
        ret.mKeys.resize(t.mKeys.rows(), mKeyBitCount);
        mEnc.localBinMatrix(mRt.mComm, t.mKeys, ret.mKeys);

        return ret;
    }

    SharedTable ComPsiServer::remoteInput(u64 partyIdx, u64 numRows)
    {
        SharedTable ret;
        //ret.mKeys.resize(numRows, mKeyBitCount);
        //mEnc.remotePackedBinary(mRt.mComm, ret.mKeys);
        ret.mKeys.resize(numRows, mKeyBitCount);
        mEnc.remoteBinMatrix(mRt.mComm, ret.mKeys);
        return ret;
    }

    SharedTable ComPsiServer::intersect(SharedTable & A, SharedTable & B)
    {
        std::array<SharedTable*, 2> AB{ &A,&B };
        std::array<u64, 2> reveals{ 0,1 };
        aby3::Sh3::i64Matrix keys = computeKeys(AB, reveals);

        //cuckooHash(A);


        throw std::runtime_error(LOCATION);
    }
    Matrix<u8> ComPsiServer::cuckooHash(SharedTable & A, aby3::Sh3::i64Matrix& keys)
    {
        if (mIdx != 0)
            throw std::runtime_error(LOCATION);
        CuckooIndex<CuckooTypes::NotThreadSafe> cuckoo;
        cuckoo.init(A.mKeys.rows(), 5, 0, 3);

        if (keys.cols() != 2)
            throw std::runtime_error(LOCATION);

        span<block> view((block*)keys.data(), keys.rows());

        block hashSeed = CCBlock;
        cuckoo.insert(view, hashSeed);

        Matrix<u8> share0(cuckoo.mBins.size(), (A.mKeys.bitCount() + 7) / 8);
        Matrix<u8> share2(cuckoo.mBins.size(), (A.mKeys.bitCount() + 7) / 8);

        auto stride = share0.cols();
        std::vector<u32> perm(cuckoo.mBins.size(), -1);
   
        u32 next = A.mKeys.rows();
        for (u32 i =0; i < cuckoo.mBins.size(); ++i)
        {
            if (cuckoo.mBins[i].isEmpty() == false)
            {
                auto inputIdx = cuckoo.mBins[i].idx();
                auto src = &A.mKeys.mShares[0](inputIdx, 0);
                auto dest = &share0(i, 0);
                memcpy(dest, src, stride);
                perm[inputIdx] = i;
            }
            else
            {
                perm[next++] = i;
            }
        }

        OblvPermutation oblvPerm;

        //std::this_thread::sleep_for(std::chrono::seconds(1));
        //for (u64 i = 0; i < perm.size(); ++i)
        //{
        //    std::cout << i << " -> " << perm[i] << std::endl;
        //}

        oblvPerm.program(mNext, mPrev, std::move(perm), mPrng, share2, OblvPermutation::Overwrite);


        //aby3::Sh3::i64Matrix a(A.mKeys.rows(), 2);
        //mEnc.reveal(mRt.mComm, A.mKeys, a);
        //Matrix<u8> share1(share0.rows(), share0.cols());
        //mNext.recv(share1.data(), share1.size());


        //bool failed = false;
        //for (u32 i = 0; i < keys.rows(); ++i)
        //{
        //    block key = cuckoo.mHashes[i];
        //    auto idx = cuckoo.find(key).mCuckooPositon;

        //    auto exp = (u8*)a.row(i).data();
        //    //
        //    //std::cout << i << " @ " << idx << "\n\t exp:";

        //    //for (u32 j = 0; j < share0.stride(); ++j)std::cout << " " <<std::hex << std::setw(2)<< int(exp[j]);
        //    //std::cout << "\n\t act:";

        //    //for (u32 j = 0; j < share0.stride(); ++j) std::cout << " " << std::hex << std::setw(2) << int(share0[idx][j] ^ share1[idx][j] ^ share2[idx][j]);
        //    //std::cout << "\n\t  s0: ";
        //    //for (u32 j = 0; j < share0.stride(); ++j) std::cout << " " << std::hex << std::setw(2) << int(share0[idx][j]);
        //    //std::cout << "\n\t  s1: ";
        //    //for (u32 j = 0; j < share0.stride(); ++j) std::cout << " " << std::hex << std::setw(2) << int(share1[idx][j]);
        //    //std::cout << "\n\t  s2: ";
        //    //for (u32 j = 0; j < share0.stride(); ++j) std::cout << " " << std::hex << std::setw(2) << int(share2[idx][j]);



        //    for (u32 j = 0; j < share0.stride(); ++j)
        //    {
        //        if (exp[j] != (share0[idx][j] ^ share1[idx][j] ^ share2[idx][j]))
        //        {
        //            failed = true;
        //        }
        //    }

        //    
        //    std::cout << std::endl << std::dec;

        //}

        //if(failed)
        //    throw std::runtime_error(LOCATION);


        return std::move(share0);
    }

    void ComPsiServer::cuckooHashSend(SharedTable & A)
    {
        if (mIdx != 2)
            throw std::runtime_error(LOCATION);
        auto cuckooParams = CuckooIndex<>::selectParams(A.mKeys.rows(), 5, 0, 3);
        Matrix<u8> share1(cuckooParams.numBins(), (A.mKeys.bitCount() + 7) / 8);

        //auto dest = share1.data();
        for (u32 i = 0; i < A.mKeys.rows(); ++i)
        {
            auto src0 = (u8*)(A.mKeys.mShares[0].data() + i * A.mKeys.i64Cols());
            auto src1 = (u8*)(A.mKeys.mShares[1].data() + i * A.mKeys.i64Cols());

            //std::cout << std::dec << " src[" << i << "] = ";

            for (u32 j = 0; j < share1.cols(); ++j)
            {
                share1(i, j) = src0[j] ^ src1[j];
                //std::cout << " " << std::setw(2) << std::hex << int(share1(i, j));
            }
            //std::cout << std::dec << std::endl;
        }

        OblvPermutation oblvPerm;
        oblvPerm.send(mNext, mPrev, std::move(share1));

        mEnc.reveal(mRt.mComm, 0, A.mKeys);
    }

    Matrix<u8> ComPsiServer::cuckooHashRecv(SharedTable & A)
    {

        if (mIdx != 1)
            throw std::runtime_error(LOCATION);

        auto cuckooParams = CuckooIndex<>::selectParams(A.mKeys.rows(), 5, 0, 3);
        Matrix<u8> share1(cuckooParams.numBins(), (A.mKeys.bitCount() + 7) / 8);
        share1.setZero();

        OblvPermutation oblvPerm;
        oblvPerm.recv(mPrev, mNext, share1);
        

        //auto dest = share1.data();
        //for (u32 i = 0; i < share1.rows(); ++i)
        //{
        //    std::cout << std::dec << " s1[" << i <<"] = ";

        //    for (u32 j = 0; j < share1.cols(); ++j)
        //    {
        //        std::cout << " " << std::setw(2) << std::hex<<int(share1(i,j));
        //    }
        //    std::cout << std::dec << std::endl;
        //}

        

        mEnc.reveal(mRt.mComm, 0, A.mKeys);
        mPrev.asyncSendCopy(share1.data(), share1.size());



        return std::move(share1);
    }

    aby3::Sh3::i64Matrix ComPsiServer::computeKeys(span<SharedTable*> tables, span<u64> reveals)
    {
        aby3::Sh3::i64Matrix ret;
        std::vector<aby3::Sh3BinaryEvaluator> binEvals(tables.size());

        auto blockSize = mLowMCCir.mInputs[0].size();
        auto rounds = mLowMCCir.mInputs.size() - 1;

        aby3::Sh3::sbMatrix oprfRoundKey(1, blockSize);// , temp;
        for (u64 i = 0; i < tables.size(); ++i)
        {
            //auto shareCount = tables[i]->mKeys.shareCount();
            auto shareCount = tables[i]->mKeys.rows();

            //if (i == 0)
            //{
            //    binEvals[i].enableDebug(mIdx, mPrev, mNext);
            //}
            binEvals[i].setCir(&mLowMCCir, shareCount);

            binEvals[i].setInput(0, tables[i]->mKeys);

            for (u64 j = 0; j < rounds; ++j)
            {
                mEnc.rand(oprfRoundKey);
                //temp.resize(shareCount, blockSize);
                //for (u64 k = 0; k < shareCount; ++k)
                //{
                //    for (u64 l = 0; l < temp.mShares[0].cols(); ++l)
                //    {
                //        temp.mShares[0](k, l) = oprfRoundKey.mShares[0](0, l);
                //        temp.mShares[1](k, l) = oprfRoundKey.mShares[1](0, l);
                //    }
                //}
                //binEvals[i].setInput(j + 1, temp);
                binEvals[i].setReplicatedInput(j + 1, oprfRoundKey);
            }


            binEvals[i].asyncEvaluate(mRt.noDependencies());
        }

        // actaully runs the computations
        mRt.runAll();


        std::vector<aby3::Sh3::sPackedBin> temps(tables.size());

        for (u64 i = 0; i < tables.size(); ++i)
        {
            //auto shareCount = tables[i]->mKeys.shareCount();
            auto shareCount = tables[i]->mKeys.rows();
            temps[i].resize(shareCount, blockSize);

            if (reveals[i] == mIdx)
            {
                if (ret.size())
                    throw std::runtime_error("only one output per party. " LOCATION);
                ret.resize(shareCount, blockSize / 64);


                binEvals[i].getOutput(0, temps[i]);
                mEnc.reveal(mRt.noDependencies(), temps[i], ret);
            }
            else
            {
                binEvals[i].getOutput(0, temps[i]);
                mEnc.reveal(mRt.noDependencies(), reveals[i], temps[i]);
            }
        }

        // actaully perform the reveals
        mRt.runAll();

        return ret;
    }
}

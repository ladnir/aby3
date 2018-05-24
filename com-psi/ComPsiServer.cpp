#include "ComPsiServer.h"
#include "OblvPermutation.h"
#include <iomanip>
#include "OblvSwitchNet.h"

namespace osuCrypto
{
    int ComPsiServer_ssp = 10;
    bool ComPsiServer_debug = false;

    void ComPsiServer::init(u64 idx, Session & prev, Session & next)
    {
        mIdx = idx;

        mComm = { prev.addChannel(), next.addChannel() };

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
            LowMC2<sboxCount, blockSize, keySize, rounds> cipher1(false, 1);
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


    void ComPsiServer::p0CheckSelect(MatrixView<u8> cuckooTable, std::array<MatrixView<u8>, 3> a2)
    {
        mComm.mNext.send(cuckooTable.data(), cuckooTable.size());
        mComm.mNext.send(a2[0].data(), a2[0].size());
        mComm.mNext.send(a2[1].data(), a2[1].size());
        mComm.mNext.send(a2[2].data(), a2[2].size());
    }
    std::string hexString(u8* ptr, u32 size)
    {
        std::stringstream ss;
        for (u64 i = 0; i < size; ++i)
        {
            ss << std::hex << std::setw(2) << std::setfill('0') << u32(ptr[i]);
        }

        return ss.str();
    }

    void ComPsiServer::p1CheckSelect(Matrix<u8> cuckooTable, std::array<MatrixView<u8>, 3> a2, aby3::Sh3::i64Matrix& keys)
    {

        Matrix<u8> c2(cuckooTable.rows(), cuckooTable.cols());
        std::array<Matrix<u8>, 3> a3;

        a3[0].resize(a2[0].rows(), cuckooTable.cols());
        a3[1].resize(a2[1].rows(), cuckooTable.cols());
        a3[2].resize(a2[2].rows(), cuckooTable.cols());

        mComm.mPrev.recv(c2.data(), c2.size());
        mComm.mPrev.recv(a3[0].data(), a3[0].size());
        mComm.mPrev.recv(a3[1].data(), a3[1].size());
        mComm.mPrev.recv(a3[2].data(), a3[2].size());


        for (u64 i = 0; i < cuckooTable.size(); ++i)
            cuckooTable(i) = cuckooTable(i) ^ c2(i);


        std::array<Matrix<u8>, 3> select;
        select[0] = a2[0];
        select[1] = a2[1];
        select[2] = a2[2];

        for (u64 h = 0; h < 3; ++h)
        {
            //a2[h](i) = a2[h](i) ^ a3[h](i);

            for (auto i = 0; i < a2[0].rows(); ++i)
            {
                for (auto j = 0; j < a2[0].cols(); ++j)
                {
                    select[h](i, j) ^= a3[h](i, j);
                }

                if (h == 0)
                {
                    std::cout << "p " << mIdx << " Select[0][" << i << "] = " << hexString(&select[h](i, 0), a2[0].cols()) << " = " << hexString(&a2[h](i, 0), a2[h].cols()) << " ^ " << hexString(&a3[h](i, 0), a3[h].cols()) << std::endl;

                }
            }

        }


        span<block> view((block*)keys.data(), keys.rows());

        auto cuckooParams = CuckooIndex<>::selectParams(keys.rows(), ComPsiServer_ssp, 0, 3);
        auto numBins = cuckooParams.numBins();



        bool failed = false;
        for (u64 i = 0; i < view.size(); ++i)
        {
            std::array<u64, 3> hx;
            for (u64 h = 0; h < 3; ++h)
            {
                hx[h] = CuckooIndex<>::getHash(view[i], h, numBins);

                auto repeat = false;
                for (u64 hh = 0; hh < h; ++hh)
                {
                    if (hx[hh] == hx[h])
                        repeat = true;
                }

                if (repeat == false)
                {

                    auto destPtr = &select[h](i, 0);
                    auto srcPtr = &cuckooTable(hx[h], 0);

                    std::cout << "h " << h << " i " << i << " " << hexString(destPtr, select[h].cols()) << " <- hx " << hx[h] << " " << hexString((u8*)&view[i], 10) << std::endl;

                    for (u64 j = 0; j < select[h].cols(); ++j)
                    {
                        if (srcPtr[j] != destPtr[j])
                        {
                            std::cout << " neq " << hexString(srcPtr, select[h].cols()) << std::endl;
                            //throw std::runtime_error("");
                            failed = true;
                            continue;
                        }
                    }
                }
            }


        }

        if (failed)
            throw RTE_LOC;
    }

    SharedTable ComPsiServer::intersect(SharedTable & A, SharedTable & B)
    {
        std::array<SharedTable*, 2> AB{ &A,&B };
        std::array<u64, 2> reveals{ 0,1 };
        aby3::Sh3::i64Matrix keys = computeKeys(AB, reveals);
        auto bits = A.mKeys.bitCount();
        auto bytes = (bits + 7) / 8;

        SharedTable C;
        C.mKeys.resize(B.mKeys.rows(), mKeyBitCount);
        aby3::Sh3::PackedBin plainFlags(B.rows(), 1);

        switch (mIdx)
        {
        case 0:
        {
            if (ComPsiServer_debug)
            {
                ostreamLock o(std::cout);
                for (u64 i = 0; i < keys.rows(); ++i)
                {
                    o << "A.key[" << i << "] = " << hexString((u8*)&keys(i, 0), 10) << std::endl;
                }
            }

            auto cuckooTable = cuckooHash(A, keys);
            if (bytes != cuckooTable.cols())
                throw std::runtime_error("");

            Matrix<u8> a2(B.rows() * 3, cuckooTable.cols());
            std::array<MatrixView<u8>, 3> A2{
                MatrixView<u8>(a2.data() + 0 * B.rows() * cuckooTable.cols(), B.rows(), cuckooTable.cols()),
                MatrixView<u8>(a2.data() + 1 * B.rows() * cuckooTable.cols(), B.rows(), cuckooTable.cols()),
                MatrixView<u8>(a2.data() + 2 * B.rows() * cuckooTable.cols(), B.rows(), cuckooTable.cols()),
            };

            selectCuckooPos(cuckooTable, A2);

            if (ComPsiServer_debug)
                p0CheckSelect(cuckooTable, A2);


            aby3::Sh3::sPackedBin intersectionFlags(B.rows(), 1);
            compare(B, A2, intersectionFlags);

            //char c(0);
            //mComm.mNext.send(c);
            //mComm.mPrev.send(c);
            //mComm.mNext.recv(c);
            //mComm.mPrev.recv(c);
            mEnc.revealAll(mRt.noDependencies(), intersectionFlags, plainFlags).get();

            break;
        }
        case 1:
        {
            if (ComPsiServer_debug)
            {
                ostreamLock o(std::cout);
                for (u64 i = 0; i < keys.rows(); ++i)
                {
                    o << "B.key[" << i << "] = " << hexString((u8*)&keys(i, 0), 10) << std::endl;
                }
            }

            auto cuckooTable = cuckooHashRecv(A);
            Matrix<u8> a2(B.rows() * 3, cuckooTable.cols());
            std::array<MatrixView<u8>, 3> A2{
                MatrixView<u8>(a2.data() + 0 * B.rows() * cuckooTable.cols(), B.rows(), cuckooTable.cols()),
                MatrixView<u8>(a2.data() + 1 * B.rows() * cuckooTable.cols(), B.rows(), cuckooTable.cols()),
                MatrixView<u8>(a2.data() + 2 * B.rows() * cuckooTable.cols(), B.rows(), cuckooTable.cols()),
            };

            selectCuckooPos(cuckooTable, A2, keys);

            if (ComPsiServer_debug)
                p1CheckSelect(cuckooTable, A2, keys);


            aby3::Sh3::sPackedBin intersectionFlags(B.rows(), 1);
            compare(B, A2, intersectionFlags);


            //char c(0);
            //mComm.mNext.send(c);
            //mComm.mPrev.send(c);
            //mComm.mNext.recv(c);
            //mComm.mPrev.recv(c);

            mEnc.revealAll(mRt.noDependencies(), intersectionFlags, plainFlags).get();
            break;
        }
        case 2:
        {
            auto cuckooParams = CuckooIndex<>::selectParams(A.mKeys.rows(), ComPsiServer_ssp, 0, 3);
            cuckooHashSend(A, cuckooParams);
            selectCuckooPos(B.rows(), cuckooParams.numBins(), bytes);
            aby3::Sh3::sPackedBin intersectionFlags(B.rows(), 1);
            compare(B, intersectionFlags);

            //char c(0);
            //mComm.mNext.send(c);
            //mComm.mPrev.send(c);
            //mComm.mNext.recv(c);
            //mComm.mPrev.recv(c);
            mEnc.revealAll(mRt.noDependencies(), intersectionFlags, plainFlags).get();
            break;
        }
        default:
            throw std::runtime_error("");
        }


        BitIterator iter((u8*)plainFlags.mData.data(), 0);
        auto size = 0;


        ostreamLock o(std::cout);
        for (u64 i = 0; i < B.rows(); ++i)
        {

            //std::cout << mIdx << " " << i << " " << *iter << std::endl;;
            if (*iter)
            {
                auto s0 = &B.mKeys.mShares[0](i, 0);
                auto s1 = &B.mKeys.mShares[1](i, 0);
                auto d0 = &C.mKeys.mShares[0](size, 0);
                auto d1 = &C.mKeys.mShares[1](size, 0);

                memcpy(d0, s0, bytes);
                memcpy(d1, s1, bytes);

                ++size;
            }

            ++iter;
        }

        C.mKeys.resize(size, bits);

        return C;
    }

    Matrix<u8> ComPsiServer::cuckooHash(SharedTable & A, aby3::Sh3::i64Matrix& keys)
    {
        if (mIdx != 0)
            throw std::runtime_error(LOCATION);
        CuckooIndex<CuckooTypes::NotThreadSafe> cuckoo;
        cuckoo.init(A.mKeys.rows(), ComPsiServer_ssp, 0, 3);


        if (keys.cols() != 2)
            throw std::runtime_error(LOCATION);
        span<block> view((block*)keys.data(), keys.rows());

        if (ComPsiServer_debug)
        {
            auto numBins = cuckoo.mBins.size();
            ostreamLock o(std::cout);
            for (u64 i = 0; i < keys.rows(); ++i)
            {
                o << "A.key[" << i << "] = " << hexString((u8*)&keys(i, 0), 10);
                for (u64 h = 0; h < 3; ++h)
                {
                    auto hx = CuckooIndex<>::getHash(view[i], h, numBins);

                    o << " " << hx;
                }

                o << std::endl;
            }
        }

        cuckoo.insert(view);



        if (ComPsiServer_debug)
            cuckoo.print();

        Matrix<u8> share0(cuckoo.mBins.size(), (A.mKeys.bitCount() + 7) / 8);
        //Matrix<u8> share2(cuckoo.mBins.size(), (A.mKeys.bitCount() + 7) / 8);

        auto stride = share0.cols();
        std::vector<u32> perm(cuckoo.mBins.size(), -1);

        u32 next = A.mKeys.rows();
        for (u32 i = 0; i < cuckoo.mBins.size(); ++i)
        {
            if (cuckoo.mBins[i].isEmpty() == false)
            {
                auto inputIdx = cuckoo.mBins[i].idx();
                auto src = &A.mKeys.mShares[0](inputIdx, 0);
                auto dest = &share0(i, 0);
                memcpy(dest, src, stride);
                perm[inputIdx] = i;

                if (ComPsiServer_debug)
                    std::cout << "in[" << inputIdx << "] -> cuckoo[" << i << "]" << std::endl;
            }
            else
            {
                perm[next++] = i;
            }
        }

        OblvPermutation oblvPerm;
        oblvPerm.program(mComm.mNext, mComm.mPrev, std::move(perm), mPrng, share0, (std::to_string(mIdx)) + "_cuckoo_hash", OutputType::Additive);


        //{
        //    Matrix<u8> share1(cuckoo.mBins.size(), (A.mKeys.bitCount() + 7) / 8);
        //    mComm.mNext.recv(share1.data(), share1.size());

        //    std::vector<u8> temp(share0.cols());

        //    for (u64 i = 0; i < share1.rows(); ++i)
        //    {
        //        for (u64 j = 0; j < temp.size(); ++j)
        //        {
        //            temp[j] = share0(i, j) ^ share1(i, j);
        //        }

        //        std::cout << "cuckoo[" << i << "] = " << hexString(temp.data(), temp.size()) << std::endl;;

        //    }

        //}


        return std::move(share0);
    }


    void ComPsiServer::cuckooHashSend(SharedTable & A, CuckooParam& cuckooParams)
    {
        if (mIdx != 2)
            throw std::runtime_error(LOCATION);
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
        oblvPerm.send(mComm.mNext, mComm.mPrev, std::move(share1), (std::to_string(mIdx)) + "_cuckoo_hash_send");

        //mEnc.reveal(mRt.mComm, 0, A.mKeys);
    }

    Matrix<u8> ComPsiServer::cuckooHashRecv(SharedTable & A)
    {

        if (mIdx != 1)
            throw std::runtime_error(LOCATION);

        auto cuckooParams = CuckooIndex<>::selectParams(A.mKeys.rows(), ComPsiServer_ssp, 0, 3);
        Matrix<u8> share1(cuckooParams.numBins(), (A.mKeys.bitCount() + 7) / 8);
        share1.setZero();

        OblvPermutation oblvPerm;
        oblvPerm.recv(mComm.mPrev, mComm.mNext, share1, share1.rows(), (std::to_string(mIdx)) + "_cuckoo_hash_recv");


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



        //mEnc.reveal(mRt.mComm, 0, A.mKeys);

        //if (ComPsiServer_debug)
        //    mComm.mPrev.asyncSendCopy(share1.data(), share1.size());



        return std::move(share1);
    }

    void ComPsiServer::selectCuckooPos(MatrixView<u8> cuckooHashTable, std::array<MatrixView<u8>, 3> dest)
    {
        if (mIdx != 0)
            throw std::runtime_error("");

        if (dest[0].cols() != cuckooHashTable.cols())
            throw std::runtime_error("");

        auto size = dest[0].rows();

        OblvSwitchNet snet(std::to_string(mIdx));
        for (u64 h = 0; h < 3; ++h)
        {

            snet.sendRecv(mComm.mNext, mComm.mPrev, cuckooHashTable, dest[h]);
        }
    }

    void ComPsiServer::selectCuckooPos(u32 destRows, u32 srcRows, u32 bytes)
    {
        OblvSwitchNet snet(std::to_string(mIdx));
        for (u64 h = 0; h < 3; ++h)
        {
            snet.help(mComm.mPrev, mComm.mNext, mPrng, destRows, srcRows, bytes);
        }
    }

    void ComPsiServer::selectCuckooPos(
        MatrixView<u8> cuckooHashTable,
        std::array<MatrixView<u8>, 3> dest,
        aby3::Sh3::i64Matrix & keys)
    {
        if (mIdx != 1)
            throw std::runtime_error("");

        auto cuckooParams = CuckooIndex<>::selectParams(keys.rows(), ComPsiServer_ssp, 0, 3);
        auto numBins = cuckooParams.numBins();

        span<block> view((block*)keys.data(), keys.rows());

        if (dest[0].cols() != cuckooHashTable.cols())
            throw std::runtime_error("");

        if (dest[0].rows() != keys.rows())
            throw std::runtime_error("");

        OblvSwitchNet snet(std::to_string(mIdx));

        std::array<OblvSwitchNet::Program, 3> progs;
        for (u64 h = 0; h < 3; ++h)
        {
            progs[h].init(cuckooHashTable.rows(), keys.rows());
        }

        for (u64 i = 0; i < view.size(); ++i)
        {
            std::array<u64, 3> hx{
                CuckooIndex<>::getHash(view[i], 0, numBins),
                CuckooIndex<>::getHash(view[i], 1, numBins),
                CuckooIndex<>::getHash(view[i], 2, numBins)
            };

            if (GSL_UNLIKELY(hx[0] == hx[1] || hx[0] == hx[2] || hx[1] == hx[2]))
            {
                if (hx[0] == hx[1])
                    hx[1] = (hx[1] + 1) % numBins;

                if (hx[0] == hx[2] || hx[1] == hx[2])
                    hx[2] = (hx[2] + 1) % numBins;

                if (hx[0] == hx[2] || hx[1] == hx[2])
                    hx[2] = (hx[2] + 1) % numBins;
            }

            for (u64 h = 0; h < 3; ++h)
            {


                progs[h].addSwitch(hx[h], i);

                auto destPtr = &dest[h](i, 0);
                auto srcPtr = &cuckooHashTable(hx[h], 0);

                memcpy(destPtr, srcPtr, dest[h].cols());
            }

        }

        for (u64 h = 0; h < 3; ++h)
        {
            snet.program(mComm.mNext, mComm.mPrev, progs[h], mPrng, dest[h], OutputType::Additive);
        }

    }

    void ComPsiServer::compare(SharedTable & B, std::array<MatrixView<u8>, 3> a, aby3::Sh3::sPackedBin& intersectionFlags)
    {
        auto size = a[0].rows();


        std::array<aby3::Sh3::sPackedBin, 3> A;
        A[0].resize(size, mKeyBitCount),
            A[1].resize(size, mKeyBitCount),
            A[2].resize(size, mKeyBitCount);

        auto t0 = mEnc.localPackedBinary(mRt.noDependencies(), a[0], A[0], true);
        auto t1 = mEnc.localPackedBinary(mRt.noDependencies(), a[1], A[1], true);
        auto t2 = mEnc.localPackedBinary(mRt.noDependencies(), a[2], A[2], true);
        t0.getRuntime().runOne();

        auto cir = getBasicCompare();

        aby3::Sh3BinaryEvaluator eval;

        if (ComPsiServer_debug)
            eval.enableDebug(mIdx, mComm.mPrev, mComm.mNext);

        eval.setCir(&cir, size);
        eval.setInput(0, B.mKeys);
        t0.get();
        eval.setInput(1, A[0]);
        t1.get();
        eval.setInput(2, A[1]);
        t2.get();
        eval.setInput(3, A[2]);

        std::vector<std::vector<aby3::Sh3BinaryEvaluator::DEBUG_Triple>>plainWires;
        if (ComPsiServer_debug)
        {
            eval.distributeInputs();
            plainWires = eval.mPlainWires_DEBUG;
        }

        eval.asyncEvaluate(mRt.noDependencies()).get();

        eval.getOutput(0, intersectionFlags);

        if (ComPsiServer_debug)
        {

            aby3::Sh3::i64Matrix bb(B.mKeys.rows(), B.mKeys.i64Cols());
            mEnc.revealAll(mComm, B.mKeys, bb);
            std::array<Matrix<u8>, 3> aa, select;
            aa[0].resize(a[0].rows(), a[0].cols());
            aa[1].resize(a[1].rows(), a[1].cols());
            aa[2].resize(a[2].rows(), a[2].cols());
            select[0].resize(a[0].rows(), a[0].cols());
            select[1].resize(a[1].rows(), a[1].cols());
            select[2].resize(a[2].rows(), a[2].cols());

            if (mIdx == 0)
            {
                mComm.mNext.asyncSend(a[0].data(), a[0].size());
                mComm.mNext.asyncSend(a[1].data(), a[1].size());
                mComm.mNext.asyncSend(a[2].data(), a[2].size());
                mComm.mNext.recv(aa[0].data(), aa[0].size());
                mComm.mNext.recv(aa[1].data(), aa[1].size());
                mComm.mNext.recv(aa[2].data(), aa[2].size());
            }
            else
            {
                mComm.mPrev.asyncSend(a[0].data(), a[0].size());
                mComm.mPrev.asyncSend(a[1].data(), a[1].size());
                mComm.mPrev.asyncSend(a[2].data(), a[2].size());
                mComm.mPrev.recv(aa[0].data(), aa[0].size());
                mComm.mPrev.recv(aa[1].data(), aa[1].size());
                mComm.mPrev.recv(aa[2].data(), aa[2].size());
            }
            std::vector<std::array<bool, 3>> exp(size);

            {

                ostreamLock o(std::cout);

                for (auto i = 0; i < a[0].rows(); ++i)
                {
                    for (auto j = 0; j < a[0].cols(); ++j)
                    {
                        select[0](i, j) = aa[0](i, j) ^ a[0](i, j);
                        select[1](i, j) = aa[1](i, j) ^ a[1](i, j);
                        select[2](i, j) = aa[2](i, j) ^ a[2](i, j);
                    }

                    std::cout << "p " << mIdx << " select[0][" << i << "] = " << hexString(&select[0](i, 0), a[0].cols()) << " = " << hexString(&aa[0](i, 0), a[0].cols()) << " ^ " << hexString(&a[0](i, 0), a[0].cols()) << std::endl;
                }

                for (auto i = 0; i < size; ++i)
                {
                    std::cout << "select[" << i << "] "
                        << hexString(select[0][i].data(), select[0][i].size()) << " "
                        << hexString(select[1][i].data(), select[1][i].size()) << " "
                        << hexString(select[2][i].data(), select[2][i].size()) << " vs "
                        << hexString((u8*)bb.row(i).data(), select[2][i].size()) << std::endl;;

                    if (memcmp(select[0][i].data(), bb.row(i).data(), select[0].cols()) == 0)
                        exp[i][0] = true;
                    if (memcmp(select[1][i].data(), bb.row(i).data(), select[1].cols()) == 0)
                        exp[i][1] = true;
                    if (memcmp(select[2][i].data(), bb.row(i).data(), select[2].cols()) == 0)
                        exp[i][2] = true;

                    BitIterator iter0(select[0][i].data(), 0);
                    BitIterator iter1(select[1][i].data(), 0);
                    BitIterator iter2(select[2][i].data(), 0);

                    for (u64 b = 0; b < cir.mInputs[0].size(); ++b)
                    {
                        if (*iter0++ != plainWires[i][cir.mInputs[1][b]].val())
                            throw RTE_LOC;
                        if (*iter1++ != plainWires[i][cir.mInputs[2][b]].val())
                            throw RTE_LOC;
                        if (*iter2++ != plainWires[i][cir.mInputs[3][b]].val())
                            throw RTE_LOC;
                    }

                }
            }

            aby3::Sh3::sPackedBin r0 = intersectionFlags;
            aby3::Sh3::sPackedBin r1 = intersectionFlags;
            aby3::Sh3::sPackedBin r2 = intersectionFlags;
            r0.mShares[0].setZero();
            r0.mShares[1].setZero();
            r1.mShares[0].setZero();
            r1.mShares[1].setZero();
            r2.mShares[0].setZero();
            r2.mShares[1].setZero();
            eval.getOutput(1, r0);
            eval.getOutput(2, r1);
            eval.getOutput(3, r2);

            aby3::Sh3::PackedBin iflag(r0.shareCount(), r0.bitCount());
            aby3::Sh3::PackedBin rr0(r0.shareCount(), r0.bitCount());
            aby3::Sh3::PackedBin rr1(r1.shareCount(), r1.bitCount());
            aby3::Sh3::PackedBin rr2(r2.shareCount(), r2.bitCount());

            mEnc.revealAll(mRt.noDependencies(), intersectionFlags, iflag);
            mEnc.revealAll(mRt.noDependencies(), r0, rr0);
            mEnc.revealAll(mRt.noDependencies(), r1, rr1);
            mEnc.revealAll(mRt.noDependencies(), r2, rr2).get();

            BitIterator f((u8*)iflag.mData.data(), 0);
            BitIterator i0((u8*)rr0.mData.data(), 0);
            BitIterator i1((u8*)rr1.mData.data(), 0);
            BitIterator i2((u8*)rr2.mData.data(), 0);


            ostreamLock o(std::cout);
            for (u64 i = 0; i < r0.shareCount(); ++i)
            {
                u8 ff = *f++;
                u8 ii0 = *i0++;
                u8 ii1 = *i1++;
                u8 ii2 = *i2++;


                o << "circuit[" << mIdx << "][" << i << "] "
                    << " b  " << *(u64*)bb.row(i).data()
                    << " a0 " << *(u64*)select[0][i].data()
                    << " a1 " << *(u64*)select[1][i].data()
                    << " a2 " << *(u64*)select[2][i].data()
                    << " -> " << int(ff) << " = (" << int(ii0) << " " << int(ii1) << " " << int(ii2) << ")" << std::endl;

                if (ii0 != exp[i][0])
                    throw std::runtime_error("");

                if (ii1 != exp[i][1])
                    throw std::runtime_error("");

                if (ii2 != exp[i][2])
                    throw std::runtime_error("");

            }
        }
    }

    void ComPsiServer::compare(SharedTable & B, aby3::Sh3::sPackedBin & intersectionFlags)
    {
        auto size = B.rows();


        aby3::Sh3::sPackedBin
            A0(size, mKeyBitCount),
            A1(size, mKeyBitCount),
            A2(size, mKeyBitCount);

        auto t0 = mEnc.remotePackedBinary(mRt.noDependencies(), A0);
        auto t1 = mEnc.remotePackedBinary(mRt.noDependencies(), A1);
        auto t2 = mEnc.remotePackedBinary(mRt.noDependencies(), A2);
        t0.getRuntime().runOne();

        auto cir = getBasicCompare();

        aby3::Sh3BinaryEvaluator eval;
        if (ComPsiServer_debug)
            eval.enableDebug(mIdx, mComm.mPrev, mComm.mNext);
        eval.setCir(&cir, size);
        eval.setInput(0, B.mKeys);
        t0.get();
        eval.setInput(1, A0);
        t1.get();
        eval.setInput(2, A1);
        t2.get();
        eval.setInput(3, A2);


        eval.distributeInputs();

        eval.asyncEvaluate(mRt.noDependencies()).get();

        eval.getOutput(0, intersectionFlags);

        if (ComPsiServer_debug)
        {

            aby3::Sh3::i64Matrix bb(B.mKeys.rows(), B.mKeys.i64Cols());
            mEnc.revealAll(mComm, B.mKeys, bb);

            aby3::Sh3::sPackedBin a0 = intersectionFlags;
            aby3::Sh3::sPackedBin a1 = intersectionFlags;
            aby3::Sh3::sPackedBin a2 = intersectionFlags;

            a0.mShares[0].setZero();
            a0.mShares[1].setZero();
            a1.mShares[0].setZero();
            a1.mShares[1].setZero();
            a2.mShares[0].setZero();
            a2.mShares[1].setZero();
            eval.getOutput(1, a0);
            eval.getOutput(2, a1);
            eval.getOutput(3, a2);

            aby3::Sh3::PackedBin iflag(a0.shareCount(), a0.bitCount());
            aby3::Sh3::PackedBin rr0(a0.shareCount(), a0.bitCount());
            aby3::Sh3::PackedBin rr1(a0.shareCount(), a0.bitCount());
            aby3::Sh3::PackedBin rr2(a0.shareCount(), a0.bitCount());


            mEnc.revealAll(mRt.noDependencies(), intersectionFlags, iflag);
            mEnc.revealAll(mRt.noDependencies(), a0, rr0);
            mEnc.revealAll(mRt.noDependencies(), a1, rr1);
            mEnc.revealAll(mRt.noDependencies(), a2, rr2).get();

            BitIterator f((u8*)iflag.mData.data(), 0);
            BitIterator i0((u8*)rr0.mData.data(), 0);
            BitIterator i1((u8*)rr1.mData.data(), 0);
            BitIterator i2((u8*)rr2.mData.data(), 0);


            ostreamLock o(std::cout);
            for (u64 i = 0; i < a0.shareCount(); ++i)
            {
                u8 ff = *f++;
                u8 ii0 = *i0++;
                u8 ii1 = *i1++;
                u8 ii2 = *i2++;
                if (ff != (ii0 ^ ii1 ^ ii2))
                    throw std::runtime_error("");


                o << "circuit[" << mIdx << "][" << i << "] -> " << int(ff) << " = (" << int(ii0) << " " << int(ii1) << " " << int(ii2) << ")" << std::endl;
            }
        }

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
        }

        for (u64 j = 0; j < rounds; ++j)
        {
            mEnc.rand(oprfRoundKey);

            for (u64 i = 0; i < tables.size(); ++i)
            {
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
        }

        for (u64 i = 0; i < tables.size(); ++i)
        {


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

    BetaCircuit ComPsiServer::getBasicCompare()
    {
        BetaCircuit r;

        BetaBundle a0(mKeyBitCount), a1(mKeyBitCount), a2(mKeyBitCount), b(mKeyBitCount);
        r.addInputBundle(b);
        r.addInputBundle(a0);
        r.addInputBundle(a1);
        r.addInputBundle(a2);
        BetaBundle out(1), c0(1), c1(1), c2(1);
        r.addOutputBundle(out);
        r.addOutputBundle(c0);
        r.addOutputBundle(c1);
        r.addOutputBundle(c2);


        // compute a0 = a0 ^ b ^ 1
        // compute a1 = a1 ^ b ^ 1
        // compute a2 = a2 ^ b ^ 1
        auto size = mKeyBitCount;
        for (auto i = 0; i < size; ++i)
        {
            r.addGate(a0[i], b[i], GateType::Nxor, a0[i]);
            r.addGate(a1[i], b[i], GateType::Nxor, a1[i]);
            r.addGate(a2[i], b[i], GateType::Nxor, a2[i]);
        }

        // check if a0,a1, or a2 are all ones, meaning ai == b
        //while (size != 1)
        //{
        //    for (u64 i = 0, j = size - 1; i < j; ++i, --j)
        //    {
        //        r.addGate(a0[i], a0[j], GateType::And, a0[i]);
        //        r.addGate(a1[i], a1[j], GateType::And, a1[i]);
        //        r.addGate(a2[i], a2[j], GateType::And, a2[i]);
        //    }
        //    size = (size + 1) / 2;
        //}
        //for (i64 i = 1; i < size; ++i)
        //{
        //    r.addGate(a0[0], a0[i], GateType::And, a0[0]);
        //    r.addGate(a1[0], a1[i], GateType::And, a1[0]);
        //    r.addGate(a2[0], a2[i], GateType::And, a2[0]);
        //}

        r.addGate(a0[0], a0[1], GateType::And, c0[0]);
        r.addGate(a1[0], a1[1], GateType::And, c1[0]);
        r.addGate(a2[0], a2[1], GateType::And, c2[0]);

        for (i64 i = 2; i < size; ++i)
        {
            r.addGate(c0[0], a0[i], GateType::And, c0[0]);
            r.addGate(c1[0], a1[i], GateType::And, c1[0]);
            r.addGate(c2[0], a2[i], GateType::And, c2[0]);
        }



        //TODO("enable this, make sure to not use colliding hash function values on a single items")
        // return the parity if the three eq tests. 
        // this will be 1 if a single items matchs. 
        // We should never have 2 matches so this is 
        // effectively the same as using GateType::Or
        r.addGate(c0[0], c1[0], GateType::Xor, out[0]);
        r.addGate(c2[0], out[0], GateType::Xor, out[0]);

        return r;
    }

    u64 SharedTable::rows()
    {
        return mKeys.rows();
    }
}

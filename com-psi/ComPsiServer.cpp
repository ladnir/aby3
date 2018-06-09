#include "ComPsiServer.h"
#include "OblvPermutation.h"
#include <iomanip>
#include "OblvSwitchNet.h"

namespace osuCrypto
{
    int ComPsiServer_ssp = 40;
    bool ComPsiServer_debug = true;
    bool debug_print = false;
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
        ret.mColumns.resize(t.mColumns.size());

        u64 rows = t.rows();

        mComm.mNext.asyncSendCopy(rows);
        mComm.mPrev.asyncSendCopy(rows);

        std::vector<std::array<u64, 2>> sizes(t.mColumns.size());
        for (u64 i = 0; i < t.mColumns.size(); ++i)
            sizes[i] = { t.mColumns[i].getBitCount(), (u64)t.mColumns[i].getTypeID() };

        mComm.mNext.asyncSendCopy(sizes);
        mComm.mPrev.asyncSend(std::move(sizes));

        for (u64 i = 0; i < t.mColumns.size(); ++i)
        {
            ret.mColumns[i].mType = t.mColumns[i].mType;
            ret.mColumns[i].mName = t.mColumns[i].mName;
            ret.mColumns[i].resize(rows, t.mColumns[i].getBitCount());

            mComm.mNext.asyncSendCopy(t.mColumns[i].mName);
            mComm.mPrev.asyncSendCopy(t.mColumns[i].mName);

            mEnc.localBinMatrix(mRt.mComm, t.mColumns[i], ret.mColumns[i]);
        }

        return ret;
    }

    SharedTable ComPsiServer::remoteInput(u64 partyIdx)
    {
        SharedTable ret;
        auto chl = ((mIdx + 1) % 3 == partyIdx) ? mComm.mNext : mComm.mPrev;

        u64 rows;
        chl.recv(rows);
        std::vector<std::array<u64, 2>> sizes;
        chl.recv(sizes);

        ret.mColumns.resize(sizes.size());

        for (u64 i = 0; i < ret.mColumns.size(); ++i)
        {
            if (sizes[i][1] == (u64)TypeID::IntID)
                ret.mColumns[i].mType = std::make_shared<IntType>(sizes[i][0]);
            if (sizes[i][1] == (u64)TypeID::StringID)
                ret.mColumns[i].mType = std::make_shared<StringType>(sizes[i][0]);

            ret.mColumns[i].resize(rows, sizes[i][0]);
            chl.recv(ret.mColumns[i].mName);
            mEnc.remoteBinMatrix(mRt.mComm, ret.mColumns[i]);
        }

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

                if (debug_print && h == 0)
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

                    if (debug_print)
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
        auto aa = A[A.mColumns[0].mName];
        auto bb = B[B.mColumns[0].mName];
        return join(aa, bb, { aa });
    }


    SharedTable ComPsiServer::join(
        SharedTable::ColRef leftJoinCol, 
        SharedTable::ColRef rightJoinCol, 
        std::vector<SharedTable::ColRef> selects)
    {

        setTimePoint("intersect_start");
        std::array<SharedTable::ColRef, 2> AB{ leftJoinCol,rightJoinCol };
        std::array<u64, 2> reveals{ 0,1 };
        aby3::Sh3::i64Matrix keys = computeKeys(AB, reveals);

        setTimePoint("intersect_compute_keys");

        auto bits = leftJoinCol.mCol.getBitCount();
        auto bytes = (bits + 7) / 8;

        auto& A = leftJoinCol.mTable;
        auto& B = rightJoinCol.mTable;

        //C.mKeys.resize(B.mKeys.rows(), mKeyBitCount);
        aby3::Sh3::PackedBin plainFlags(B.rows(), 1);


        // all of the coolumns out of the left table that need to be selected.
        std::vector<SharedTable::ColRef> leftSelect;
        leftSelect.reserve(A.mColumns.size());
        leftSelect.push_back(leftJoinCol);
        for (auto& c : selects)
            if (&leftJoinCol.mCol != &c.mCol && &c.mTable == &A)
                leftSelect.push_back(c);

        switch (mIdx)
        {
        case 0:
        {
            if (debug_print && ComPsiServer_debug)
            {
                ostreamLock o(std::cout);
                for (u64 i = 0; i < keys.rows(); ++i)
                {
                    o << "A.key[" << i << "] = " << hexString((u8*)&keys(i, 0), 10) << std::endl;
                }
            }

            auto cuckooTable = cuckooHash(leftSelect, keys);

            setTimePoint("intersect_cuckoo_hash");


            if (bytes != cuckooTable.cols())
                throw std::runtime_error("");

            Matrix<u8> a2(B.rows() * 3, cuckooTable.cols());
            std::array<MatrixView<u8>, 3> A2{
                MatrixView<u8>(a2.data() + 0 * B.rows() * cuckooTable.cols(), B.rows(), cuckooTable.cols()),
                MatrixView<u8>(a2.data() + 1 * B.rows() * cuckooTable.cols(), B.rows(), cuckooTable.cols()),
                MatrixView<u8>(a2.data() + 2 * B.rows() * cuckooTable.cols(), B.rows(), cuckooTable.cols()),
            };

            selectCuckooPos(cuckooTable, A2);

            setTimePoint("intersect_select_cuckoo");


            if (ComPsiServer_debug)
                p0CheckSelect(cuckooTable, A2);


            aby3::Sh3::sPackedBin intersectionFlags(B.rows(), 1);
            compare(B, A2, intersectionFlags);
            setTimePoint("intersect_compare");

            //char c(0);
            //mComm.mNext.send(c);
            //mComm.mPrev.send(c);
            //mComm.mNext.recv(c);
            //mComm.mPrev.recv(c);
            mEnc.revealAll(mRt.noDependencies(), intersectionFlags, plainFlags).get();

            setTimePoint("intersect_done");

            break;
        }
        case 1:
        {
            if (debug_print && ComPsiServer_debug)
            {
                ostreamLock o(std::cout);
                for (u64 i = 0; i < keys.rows(); ++i)
                {
                    o << "B.key[" << i << "] = " << hexString((u8*)&keys(i, 0), 10) << std::endl;
                }
            }

            auto cuckooTable = cuckooHashRecv(A);
            setTimePoint("intersect_cuckoo_hash");


            Matrix<u8> a2(B.rows() * 3, cuckooTable.cols());
            std::array<MatrixView<u8>, 3> A2{
                MatrixView<u8>(a2.data() + 0 * B.rows() * cuckooTable.cols(), B.rows(), cuckooTable.cols()),
                MatrixView<u8>(a2.data() + 1 * B.rows() * cuckooTable.cols(), B.rows(), cuckooTable.cols()),
                MatrixView<u8>(a2.data() + 2 * B.rows() * cuckooTable.cols(), B.rows(), cuckooTable.cols()),
            };

            selectCuckooPos(cuckooTable, A2, keys);

            setTimePoint("intersect_select_cuckoo");

            if (ComPsiServer_debug)
                p1CheckSelect(cuckooTable, A2, keys);

            aby3::Sh3::sPackedBin intersectionFlags(B.rows(), 1);
            compare(B, A2, intersectionFlags);

            setTimePoint("intersect_compare");

            //char c(0);
            //mComm.mNext.send(c);
            //mComm.mPrev.send(c);
            //mComm.mNext.recv(c);
            //mComm.mPrev.recv(c);

            mEnc.revealAll(mRt.noDependencies(), intersectionFlags, plainFlags).get();
            setTimePoint("intersect_done");
            break;
        }
        case 2:
        {
            auto cuckooParams = CuckooIndex<>::selectParams(A.rows(), ComPsiServer_ssp, 0, 3);
            cuckooHashSend(A, cuckooParams);
            setTimePoint("intersect_cuckoo_hash");

            selectCuckooPos(B.rows(), cuckooParams.numBins(), bytes);
            setTimePoint("intersect_select_cuckoo");


            aby3::Sh3::sPackedBin intersectionFlags(B.rows(), 1);
            compare(B, intersectionFlags);
            setTimePoint("intersect_compare");

            //char c(0);
            //mComm.mNext.send(c);
            //mComm.mPrev.send(c);
            //mComm.mNext.recv(c);
            //mComm.mPrev.recv(c);
            mEnc.revealAll(mRt.noDependencies(), intersectionFlags, plainFlags).get();

            setTimePoint("intersect_done");
            break;
        }
        default:
            throw std::runtime_error("");
        }


        BitIterator iter((u8*)plainFlags.mData.data(), 0);
        auto size = 0;
        SharedTable C;
        C.mColumns.resize(B.mColumns.size());

        auto rows = std::min<u64>(A.rows(), B.rows());
        for (u64 j = 0; j < B.mColumns.size(); ++j)
        {
            C.mColumns[j].mType = B.mColumns[j].mType;
            C.mColumns[j].mName = B.mColumns[j].mName;
            C.mColumns[j].resize(rows, B.mColumns[j].getBitCount());
        }

        for (u64 i = 0; i < B.rows(); ++i)
        {

            //std::cout << mIdx << " " << i << " " << *iter << std::endl;;
            if (*iter)
            {
                for (u64 j = 0; j < B.mColumns.size(); ++j)
                {
                    auto s0 = &B.mColumns[j].mShares[0](i, 0);
                    auto s1 = &B.mColumns[j].mShares[1](i, 0);
                    auto d0 = &C.mColumns[j].mShares[0](size, 0);
                    auto d1 = &C.mColumns[j].mShares[1](size, 0);

                    memcpy(d0, s0, bytes);
                    memcpy(d1, s1, bytes);
                }

                ++size;
            }

            ++iter;
        }

        for (u64 j = 0; j < B.mColumns.size(); ++j)
        {
            C.mColumns[j].resize(size, C.mColumns[j].getBitCount());
        }

        return C;
    }



    Matrix<u8> ComPsiServer::cuckooHash(span<SharedTable::ColRef> selects, aby3::Sh3::i64Matrix& keys)
    {
        if (mIdx != 0)
            throw std::runtime_error(LOCATION);
        CuckooIndex<CuckooTypes::NotThreadSafe> cuckoo;
        cuckoo.init(keys.rows(), ComPsiServer_ssp, 0, 3);


        if (keys.cols() != 2)
            throw std::runtime_error(LOCATION);
        span<block> view((block*)keys.data(), keys.rows());

        if (debug_print && ComPsiServer_debug)
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



        if (debug_print && ComPsiServer_debug)
            cuckoo.print();


        u64 totalBytes = 0;
        std::vector<u64> strides(selects.size());
        for (u64 j = 0; j < selects.size(); ++j)
        {
            strides[j] = (selects[j].mCol.getBitCount() + 7) / 8;
            totalBytes += strides[j];
        }

        Matrix<u8> share0(cuckoo.mBins.size(), totalBytes);
        //Matrix<u8> share2(cuckoo.mBins.size(), (A.mKeys.bitCount() + 7) / 8);

        std::vector<u32> perm(cuckoo.mBins.size(), -1);

        u32 next = keys.rows();
        for (u32 i = 0; i < cuckoo.mBins.size(); ++i)
        {
            if (cuckoo.mBins[i].isEmpty() == false)
            {
                auto inputIdx = cuckoo.mBins[i].idx();
                perm[inputIdx] = i;
                auto dest = &share0(i, 0);

                if (debug_print && ComPsiServer_debug)
                    std::cout << "in[" << inputIdx << "] -> cuckoo[" << i << "]" << std::endl;

                for (u64 j = 0; j < selects.size(); ++j)
                {
                    auto& t = selects[j].mCol;

                    auto src = &t.mShares[0](inputIdx, 0);
                    memcpy(dest, src, strides[j]);
                    dest += strides[j];
                }
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


        u64 totalBytes = 0;
        std::vector<u64> strides(A.mColumns.size());
        for (u64 j = 0; j < A.mColumns.size(); ++j)
        {
            strides[j] = (A.mColumns[j].getBitCount() + 7) / 8;
            totalBytes += strides[j];
        }

        Matrix<u8> share1(cuckooParams.numBins(), totalBytes);

        //auto dest = share1.data();
        for (u32 i = 0; i < A.rows(); ++i)
        {

            u64 h = 0;
            for (u64 j = 0; j < strides.size(); ++j)
            {
                auto& t = A.mColumns[j];
                auto src0 = (u8*)(t.mShares[0].data() + i * t.i64Cols());
                auto src1 = (u8*)(t.mShares[1].data() + i * t.i64Cols());

                //std::cout << std::dec << " src[" << i << "] = ";

                for (u32 k = 0; k < strides[j]; ++k, ++h)
                {
                    share1(i, h) = src0[k] ^ src1[k];
                    //std::cout << " " << std::setw(2) << std::hex << int(share1(i, j));
                }
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

        u64 totalBytes = 0;
        std::vector<u64> strides(A.mColumns.size());
        for (u64 j = 0; j < A.mColumns.size(); ++j)
        {
            strides[j] = (A.mColumns[j].getBitCount() + 7) / 8;
            totalBytes += strides[j];
        }

        auto cuckooParams = CuckooIndex<>::selectParams(A.rows(), ComPsiServer_ssp, 0, 3);
        Matrix<u8> share1(cuckooParams.numBins(), totalBytes);

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
        A[0].reset(size, mKeyBitCount);
        A[1].reset(size, mKeyBitCount);
        A[2].reset(size, mKeyBitCount);

        auto t0 = mEnc.localPackedBinary(mRt.noDependencies(), a[0], A[0], true);
        auto t1 = mEnc.localPackedBinary(mRt.noDependencies(), a[1], A[1], true);
        auto t2 = mEnc.localPackedBinary(mRt.noDependencies(), a[2], A[2], true);
        t0.getRuntime().runOne();

        auto cir = getBasicCompare();

        aby3::Sh3BinaryEvaluator eval;

        if (ComPsiServer_debug)
            eval.enableDebug(mIdx, mComm.mPrev, mComm.mNext);

        eval.setCir(&cir, size);
        eval.setInput(0, B.mColumns[0]);
        t0.get();
        eval.setInput(1, A[0]);
        t1.get();
        eval.setInput(2, A[1]);
        t2.get();
        eval.setInput(3, A[2]);

        std::vector<std::vector<aby3::Sh3BinaryEvaluator::DEBUG_Triple>>plainWires;
        eval.distributeInputs();

        if (ComPsiServer_debug)
            plainWires = eval.mPlainWires_DEBUG;

        mRt.runAll();

        eval.asyncEvaluate(mRt.noDependencies()).get();

        eval.getOutput(0, intersectionFlags);

        if (ComPsiServer_debug)
        {

            aby3::Sh3::i64Matrix bb(B.mColumns[0].rows(), B.mColumns[0].i64Cols());
            mEnc.revealAll(mComm, B.mColumns[0], bb);
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

                    if (debug_print)
                        std::cout << "p " << mIdx << " select[0][" << i << "] = " << hexString(&select[0](i, 0), a[0].cols()) << " = " << hexString(&aa[0](i, 0), a[0].cols()) << " ^ " << hexString(&a[0](i, 0), a[0].cols()) << std::endl;
                }

                for (auto i = 0; i < size; ++i)
                {
                    if (debug_print)
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

            aby3::Sh3::sPackedBin r0(intersectionFlags.shareCount(), intersectionFlags.bitCount());
            aby3::Sh3::sPackedBin r1(intersectionFlags.shareCount(), intersectionFlags.bitCount());
            aby3::Sh3::sPackedBin r2(intersectionFlags.shareCount(), intersectionFlags.bitCount());
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

                auto t0 = ii0 != exp[i][0];
                auto t1 = ii1 != exp[i][1];
                auto t2 = ii2 != exp[i][2];

                if (debug_print || t0 || t1 || t2)
                    o << "circuit[" << mIdx << "][" << i << "] "
                    << " b  " << *(u64*)bb.row(i).data()
                    << " a0 " << *(u64*)select[0][i].data()
                    << " a1 " << *(u64*)select[1][i].data()
                    << " a2 " << *(u64*)select[2][i].data()
                    << " -> " << int(ff) << " = (" << int(ii0) << " " << int(ii1) << " " << int(ii2) << ")" << std::endl;

                if (t0)
                    throw std::runtime_error("");

                if (t1)
                    throw std::runtime_error("");

                if (t2)
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
        eval.setInput(0, B.mColumns[0]);
        t0.get();
        eval.setInput(1, A0);
        t1.get();
        eval.setInput(2, A1);
        t2.get();
        eval.setInput(3, A2);


        eval.distributeInputs();

        mRt.runAll();

        eval.asyncEvaluate(mRt.noDependencies()).get();

        eval.getOutput(0, intersectionFlags);

        if (ComPsiServer_debug)
        {

            aby3::Sh3::i64Matrix bb(B.mColumns[0].rows(), B.mColumns[0].i64Cols());
            mEnc.revealAll(mComm, B.mColumns[0], bb);

            aby3::Sh3::sPackedBin a0(intersectionFlags.shareCount(), intersectionFlags.bitCount());
            aby3::Sh3::sPackedBin a1(intersectionFlags.shareCount(), intersectionFlags.bitCount());
            aby3::Sh3::sPackedBin a2(intersectionFlags.shareCount(), intersectionFlags.bitCount());

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

                if (debug_print)
                    o << "circuit[" << mIdx << "][" << i << "] -> " << int(ff) << " = (" << int(ii0) << " " << int(ii1) << " " << int(ii2) << ")" << std::endl;
            }
        }

    }



    aby3::Sh3::i64Matrix ComPsiServer::computeKeys(span<SharedTable::ColRef> cols, span<u64> reveals)
    {
        aby3::Sh3::i64Matrix ret;
        std::vector<aby3::Sh3BinaryEvaluator> binEvals(cols.size());

        auto blockSize = mLowMCCir.mInputs[0].size();
        auto rounds = mLowMCCir.mInputs.size() - 1;

        aby3::Sh3::sbMatrix oprfRoundKey(1, blockSize);// , temp;
        for (u64 i = 0; i < cols.size(); ++i)
        {
            //auto shareCount = tables[i]->mKeys.shareCount();
            auto shareCount = cols[i].mTable.rows();

            //if (i == 0)
            //{
            //    binEvals[i].enableDebug(mIdx, mPrev, mNext);
            //}
            binEvals[i].setCir(&mLowMCCir, shareCount);

            binEvals[i].setInput(0, cols[i].mCol);
        }

        for (u64 j = 0; j < rounds; ++j)
        {
            mEnc.rand(oprfRoundKey);

            for (u64 i = 0; i < cols.size(); ++i)
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

        for (u64 i = 0; i < cols.size(); ++i)
        {


            binEvals[i].asyncEvaluate(mRt.noDependencies());
        }

        // actaully runs the computations
        mRt.runAll();


        std::vector<aby3::Sh3::sPackedBin> temps(cols.size());

        for (u64 i = 0; i < cols.size(); ++i)
        {
            //auto shareCount = tables[i]->mKeys.shareCount();
            auto shareCount = cols[i].mCol.rows();
            temps[i].reset(shareCount, blockSize);

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
        BetaBundle out(1), c0(1), c1(1), c2(1);
        r.addInputBundle(b);
        r.addInputBundle(a0);
        r.addInputBundle(a1);
        r.addInputBundle(a2);
        r.addOutputBundle(out);

        if (ComPsiServer_debug)
        {
            r.addOutputBundle(c0);
            r.addOutputBundle(c1);
            r.addOutputBundle(c2);
        }


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
        while (size != 1)
        {
            for (u64 i = 0, j = size - 1; i < j; ++i, --j)
            {
                r.addGate(a0[i], a0[j], GateType::And, a0[i]);
                r.addGate(a1[i], a1[j], GateType::And, a1[i]);
                r.addGate(a2[i], a2[j], GateType::And, a2[i]);
            }
            size = (size + 1) / 2;
        }


        // return the parity if the three eq tests. 
        // this will be 1 if a single items matchs. 
        // We should never have 2 matches so this is 
        // effectively the same as using GateType::Or
        r.addGate(a0[0], a1[0], GateType::Xor, out[0]);
        r.addGate(a2[0], out[0], GateType::Xor, out[0]);

        if (ComPsiServer_debug)
        {
            r.addCopy(a0[0], c0[0]);
            r.addCopy(a1[0], c1[0]);
            r.addCopy(a2[0], c2[0]);
        }

        return r;
    }

    u64 SharedTable::rows()
    {
        return mColumns.size() ? mColumns[0].rows() : 0;
    }
}

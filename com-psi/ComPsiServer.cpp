#include "ComPsiServer.h"
#include "OblvPermutation.h"
#include <iomanip>
#include "OblvSwitchNet.h"

namespace osuCrypto
{
    int ComPsiServer_ssp = 40;
    bool ComPsiServer_debug = false;
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

            aby3::Sh3::sbMatrix& a1 = ret.mColumns[i];
            
            mEnc.localBinMatrix(mRt.mComm, t.mColumns[i].mData, a1);
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


    void ComPsiServer::p0CheckSelect(MatrixView<u8> cuckooTable, span<Matrix<u8>> a2)
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

    void ComPsiServer::p1CheckSelect(Matrix<u8> cuckooTable, span<Matrix<u8>> a2, aby3::Sh3::i64Matrix& keys)
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

        auto& leftTable = leftJoinCol.mTable;
        auto& rightTable = rightJoinCol.mTable;


        // the number of bits
        u64 selectByteCount = leftJoinCol.mCol.getByteCount();
        // all of the columns out of the right table that need to be selected.
        std::vector<SharedTable::ColRef> circuitInputCols;
        circuitInputCols.reserve(rightTable.mColumns.size());
        circuitInputCols.push_back(leftJoinCol);

        SharedTable C;
        C.mColumns.resize(selects.size());

        std::vector<SharedTable::ColRef> circuitOutCols;
        circuitOutCols.reserve(circuitInputCols.size() - 1);
        for (u64 j = 0; j < selects.size(); ++j)
        {
            auto& s = selects[j];
            C.mColumns[j].mType = s.mCol.mType;
            C.mColumns[j].mName = s.mCol.mName;
            C.mColumns[j].resize(rightTable.rows(), s.mCol.getBitCount());
            C.mColumns[j].mShares[0].setZero();
            C.mColumns[j].mShares[1].setZero();

            if (&leftJoinCol.mCol != &s.mCol && &s.mTable == &rightTable)
            {
                selectByteCount += s.mCol.getByteCount();
                circuitInputCols.push_back(s);
                circuitOutCols.push_back(C[s.mCol.mName]);
            }
        }


        std::vector<SharedTable::ColRef> srcColumns; srcColumns.reserve(selects.size());
        for (u64 i = 0; i < selects.size(); ++i)
        {
            // if the select column is from the left table (which stays in place), we will simply copy the
            // shares out of the left table into the result table
            if (&selects[i].mTable == &leftTable)
                srcColumns.push_back(leftTable[selects[i].mCol.mName]);

            // if the select column is from the right table (which gets permuted), but in the column that we are 
            // joining on, then observe that we can actaully use the left table's join column, since they will be equal
            // for all items in the intersection. This is more efficient since it does not need to go through the circuit.
            else if(&selects[i].mTable == &rightTable &&&selects[i].mCol == &rightJoinCol.mCol)
                srcColumns.push_back(leftTable[leftJoinCol.mCol.mName]);

            // finally, if the select column is from the right table we will make the src column the output
            // column itself. Note that when we do the final copy, the src and dest columns will be the same, 
            // however, the copy will move the rows around which is desired.
            else if (&selects[i].mTable == &rightTable)
                srcColumns.push_back(C[selects[i].mCol.mName]);

            // throw if the select column is not from the left or right table.
            else
                throw RTE_LOC;
        }
        

        aby3::Sh3::sPackedBin intersectionFlags;// (rightTable.rows(), 1);
        switch (mIdx)
        {
        case 0:
        {
            if (debug_print && ComPsiServer_debug)
            {
                ostreamLock o(std::cout);
                for (u64 i = 0; i < keys.rows(); ++i)
                    o << "A.key[" << i << "] = " << hexString((u8*)&keys(i, 0), 10) << std::endl;
            }

            auto cuckooTable = cuckooHash(circuitInputCols, keys);

            setTimePoint("intersect_cuckoo_hash");


            //Matrix<u8> a2(rightTable.rows() * 3, cuckooTable.cols());
            //span<Matrix<u8>> circuitInputShare{
            //    MatrixView<u8>(a2.data() + 0 * rightTable.rows() * cuckooTable.cols(), rightTable.rows(), cuckooTable.cols()),
            //    MatrixView<u8>(a2.data() + 1 * rightTable.rows() * cuckooTable.cols(), rightTable.rows(), cuckooTable.cols()),
            //    MatrixView<u8>(a2.data() + 2 * rightTable.rows() * cuckooTable.cols(), rightTable.rows(), cuckooTable.cols()),
            //};

            auto circuitInputShare = selectCuckooPos(cuckooTable, rightTable.rows());
            setTimePoint("intersect_select_cuckoo");


            if (ComPsiServer_debug)
                p0CheckSelect(cuckooTable, circuitInputShare);

            intersectionFlags.reset(rightTable.rows(), 1);
            compare(leftJoinCol, rightJoinCol, circuitInputShare, circuitOutCols, intersectionFlags);

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

            auto cuckooTable = cuckooHashRecv(circuitInputCols);
            setTimePoint("intersect_cuckoo_hash");


            //Matrix<u8> a2(rightTable.rows() * 3, cuckooTable.cols());
            //span<Matrix<u8>> circuitInputShare{
            //    MatrixView<u8>(a2.data() + 0 * rightTable.rows() * cuckooTable.cols(), rightTable.rows(), cuckooTable.cols()),
            //    MatrixView<u8>(a2.data() + 1 * rightTable.rows() * cuckooTable.cols(), rightTable.rows(), cuckooTable.cols()),
            //    MatrixView<u8>(a2.data() + 2 * rightTable.rows() * cuckooTable.cols(), rightTable.rows(), cuckooTable.cols()),
            //};

            auto circuitInputShare = selectCuckooPos(cuckooTable, rightTable.rows(), keys);

            setTimePoint("intersect_select_cuckoo");

            if (ComPsiServer_debug)
                p1CheckSelect(cuckooTable, circuitInputShare, keys);

            intersectionFlags.reset(rightTable.rows(), 1);
            compare(leftJoinCol, rightJoinCol, circuitInputShare, circuitOutCols, intersectionFlags);

            break;
        }
        case 2:
        {
            auto cuckooParams = CuckooIndex<>::selectParams(leftTable.rows(), ComPsiServer_ssp, 0, 3);
            cuckooHashSend(circuitInputCols, cuckooParams);
            setTimePoint("intersect_cuckoo_hash");

            selectCuckooPos(rightTable.rows(), cuckooParams.numBins(), selectByteCount);
            setTimePoint("intersect_select_cuckoo");

            intersectionFlags.reset(rightTable.rows(), 1);
            compare(leftJoinCol, rightJoinCol, circuitOutCols, intersectionFlags);

            break;
        }
        default:
            throw std::runtime_error("");
        }

        setTimePoint("intersect_compare");

        aby3::Sh3::PackedBin plainFlags(rightTable.rows(), 1);
        mEnc.revealAll(mRt.noDependencies(), intersectionFlags, plainFlags).get();
        setTimePoint("intersect_done");
        BitIterator iter((u8*)plainFlags.mData.data(), 0);

        std::vector<u64> sizes(srcColumns.size());
        for (u64 i = 0; i < sizes.size(); ++i)
            sizes[i] = srcColumns[i].mCol.getByteCount();

        auto size = 0;
        for (u64 i = 0; i < rightTable.rows(); ++i)
        {

            //std::cout << mIdx << " " << i << " " << *iter << std::endl;;
            if (*iter)
            {
                for (u64 j = 0; j < srcColumns.size(); ++j)
                {
                    auto s0 = &srcColumns[j].mCol.mShares[0](i, 0);
                    auto s1 = &srcColumns[j].mCol.mShares[1](i, 0);
                    auto d0 = &C.mColumns[j].mShares[0](size, 0);
                    auto d1 = &C.mColumns[j].mShares[1](size, 0);

                    memmove(d0, s0, sizes[j]);
                    memmove(d1, s1, sizes[j]);
                }

                ++size;
            }

            ++iter;
        }

        for (u64 j = 0; j < C.mColumns.size(); ++j)
        {
            C.mColumns[j].resize(size, C.mColumns[j].getBitCount());
        }

        return C;
    }



    SharedTable ComPsiServer::leftUnion(
        SharedTable::ColRef leftJoinCol,
        SharedTable::ColRef rightJoinCol,
        std::vector<SharedTable::ColRef> leftSelects,
        std::vector<SharedTable::ColRef> rightSelects)
    {
        return {};

#ifdef UNION_ENABLED

        if (leftSelects.size() != rightSelects.size())
            throw RTE_LOC;

        for (u64 i = 0; i < leftSelects.size(); ++i)
        {
            if (leftSelects[i].mCol.getTypeID() != rightSelects[i].mCol.getTypeID() ||
                leftSelects[i].mCol.getBitCount() != rightSelects[i].mCol.getBitCount())
                throw RTE_LOC;
        }

        setTimePoint("union_start");
        std::array<SharedTable::ColRef, 2> AB{ leftJoinCol,rightJoinCol };
        std::array<u64, 2> reveals{ 0,1 };
        aby3::Sh3::i64Matrix keys = computeKeys(AB, reveals);

        setTimePoint("union_compute_keys");

        auto bits = leftJoinCol.mCol.getBitCount();
        auto bytes = (bits + 7) / 8;

        auto& A = leftJoinCol.mTable;
        auto& B = rightJoinCol.mTable;


        // all of the columns out of the left table that need to be selected.
        std::vector<SharedTable::ColRef> leftSelect;
        leftSelect.reserve(A.mColumns.size());
        leftSelect.push_back(leftJoinCol);
        for (auto& c : selects)
            if (&leftJoinCol.mCol != &c.mCol && &c.mTable == &A)
                leftSelect.push_back(c);


        SharedTable C;
        C.mColumns.resize(selects.size());

        std::vector<SharedTable::ColRef> circuitOut;
        circuitOut.reserve(leftSelect.size() - 1);
        for (u64 j = 0; j < C.mColumns.size(); ++j)
        {
            C.mColumns[j].mType = selects[j].mCol.mType;
            C.mColumns[j].mName = selects[j].mCol.mName;
            C.mColumns[j].resize(B.rows(), selects[j].mCol.getBitCount());

            if (&selects[j].mTable == &A && &selects[j].mCol != &leftJoinCol.mCol)
            {
                circuitOut.push_back(C[C.mColumns[j].mName]);
            }
        }


        std::vector<SharedTable::ColRef> srcColumns; srcColumns.reserve(selects.size());
        for (u64 i = 0; i < selects.size(); ++i)
        {
            if (&selects[i].mTable == &B)
                srcColumns.push_back(B[selects[i].mCol.mName]);
            else if (&selects[i].mTable == &A &&&selects[i].mCol == &leftJoinCol.mCol)
                srcColumns.push_back(B[rightJoinCol.mCol.mName]);
            else if (&selects[i].mTable == &A)
                srcColumns.push_back(C[selects[i].mCol.mName]);
            else
                throw RTE_LOC;
        }


        aby3::Sh3::sPackedBin intersectionFlags;// (rightTable.rows(), 1);
        switch (mIdx)
        {
        case 0:
        {
            if (debug_print && ComPsiServer_debug)
            {
                ostreamLock o(std::cout);
                for (u64 i = 0; i < keys.rows(); ++i)
                    o << "A.key[" << i << "] = " << hexString((u8*)&keys(i, 0), 10) << std::endl;
            }

            auto cuckooTable = cuckooHash(leftSelect, keys);

            setTimePoint("union_cuckoo_hash");


            Matrix<u8> a2(B.rows() * 3, cuckooTable.cols());
            span<Matrix<u8>> A2{
                MatrixView<u8>(a2.data() + 0 * B.rows() * cuckooTable.cols(), B.rows(), cuckooTable.cols()),
                MatrixView<u8>(a2.data() + 1 * B.rows() * cuckooTable.cols(), B.rows(), cuckooTable.cols()),
                MatrixView<u8>(a2.data() + 2 * B.rows() * cuckooTable.cols(), B.rows(), cuckooTable.cols()),
            };

            selectCuckooPos(cuckooTable, A2);
            setTimePoint("union_select_cuckoo");


            if (ComPsiServer_debug)
                p0CheckSelect(cuckooTable, A2);

            intersectionFlags.reset(B.rows(), 1);
            compare(leftJoinCol, rightJoinCol, A2, circuitOut, intersectionFlags);

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

            auto cuckooTable = cuckooHashRecv(leftSelect);
            setTimePoint("union_cuckoo_hash");


            Matrix<u8> a2(B.rows() * 3, cuckooTable.cols());
            span<Matrix<u8>> A2{
                MatrixView<u8>(a2.data() + 0 * B.rows() * cuckooTable.cols(), B.rows(), cuckooTable.cols()),
                MatrixView<u8>(a2.data() + 1 * B.rows() * cuckooTable.cols(), B.rows(), cuckooTable.cols()),
                MatrixView<u8>(a2.data() + 2 * B.rows() * cuckooTable.cols(), B.rows(), cuckooTable.cols()),
            };

            selectCuckooPos(cuckooTable, A2, keys);

            setTimePoint("union_select_cuckoo");

            if (ComPsiServer_debug)
                p1CheckSelect(cuckooTable, A2, keys);

            intersectionFlags.reset(B.rows(), 1);
            compare(leftJoinCol, rightJoinCol, A2, circuitOut, intersectionFlags);

            break;
        }
        case 2:
        {
            auto cuckooParams = CuckooIndex<>::selectParams(A.rows(), ComPsiServer_ssp, 0, 3);
            cuckooHashSend(leftSelect, cuckooParams);
            setTimePoint("union_cuckoo_hash");

            selectCuckooPos(B.rows(), cuckooParams.numBins(), bytes);
            setTimePoint("union_select_cuckoo");

            intersectionFlags.reset(B.rows(), 1);
            compare(leftJoinCol, rightJoinCol, circuitOut, intersectionFlags);

            break;
        }
        default:
            throw std::runtime_error("");
        }

        setTimePoint("union_compare");

        aby3::Sh3::PackedBin plainFlags(B.rows(), 1);
        mEnc.revealAll(mRt.noDependencies(), intersectionFlags, plainFlags).get();
        setTimePoint("union_done");
        BitIterator iter((u8*)plainFlags.mData.data(), 0);


        auto size = 0;
        for (u64 i = 0; i < B.rows(); ++i)
        {

            //std::cout << mIdx << " " << i << " " << *iter << std::endl;;
            if (*iter)
            {
                for (u64 j = 0; j < srcColumns.size(); ++j)
                {
                    auto s0 = &srcColumns[j].mCol.mShares[0](i, 0);
                    auto s1 = &srcColumns[j].mCol.mShares[1](i, 0);
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
#endif
    }










    Matrix<u8> ComPsiServer::cuckooHash(span<SharedTable::ColRef> selects, aby3::Sh3::i64Matrix& keys)
    {
        if (mIdx != 0)
            throw std::runtime_error(LOCATION);

        auto& cuckoo = mCuckoo;
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
        //Matrix<u8> share2(cuckoo.mBins.size(), (leftTable.mKeys.bitCount() + 7) / 8);

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
        //    Matrix<u8> share1(cuckoo.mBins.size(), (leftTable.mKeys.bitCount() + 7) / 8);
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


    void ComPsiServer::cuckooHashSend(span<SharedTable::ColRef> selects, CuckooParam& cuckooParams)
    {
        if (mIdx != 2)
            throw std::runtime_error(LOCATION);


        u64 totalBytes = 0;
        std::vector<u64> strides(selects.size());
        for (u64 j = 0; j < selects.size(); ++j)
        {
            strides[j] = (selects[j].mCol.getBitCount() + 7) / 8;
            totalBytes += strides[j];
        }

        Matrix<u8> share1(cuckooParams.numBins(), totalBytes);

        //auto dest = share1.data();
        auto rows = selects[0].mTable.rows();
        for (u32 i = 0; i < rows; ++i)
        {

            u64 h = 0;
            for (u64 j = 0; j < strides.size(); ++j)
            {
                auto& t = selects[j].mCol;
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

        //mEnc.reveal(mRt.mComm, 0, leftTable.mKeys);
    }

    Matrix<u8> ComPsiServer::cuckooHashRecv(span<SharedTable::ColRef> selects)
    {

        if (mIdx != 1)
            throw std::runtime_error(LOCATION);

        u64 totalBytes = 0;
        std::vector<u64> strides(selects.size());
        for (u64 j = 0; j < selects.size(); ++j)
        {
            strides[j] = (selects[j].mCol.getBitCount() + 7) / 8;
            totalBytes += strides[j];
        }

        auto rows = selects[0].mTable.rows();
        auto cuckooParams = CuckooIndex<>::selectParams(rows, ComPsiServer_ssp, 0, 3);
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



        //mEnc.reveal(mRt.mComm, 0, leftTable.mKeys);

        //if (ComPsiServer_debug)
        //    mComm.mPrev.asyncSendCopy(share1.data(), share1.size());



        return std::move(share1);
    }

    std::array<Matrix<u8>, 3> ComPsiServer::selectCuckooPos(MatrixView<u8> cuckooHashTable, u64 rows)
    {
        if (mIdx != 0)
            throw std::runtime_error("");


        auto cols = cuckooHashTable.cols();
        std::array<Matrix<u8>, 3> dest;
        dest[0].resize(rows, cols, oc::AllocType::Uninitialized);
        dest[1].resize(rows, cols, oc::AllocType::Uninitialized);
        dest[2].resize(rows, cols, oc::AllocType::Uninitialized);

        OblvSwitchNet snet(std::to_string(mIdx));
        for (u64 h = 0; h < 3; ++h)
        {
            snet.sendRecv(mComm.mNext, mComm.mPrev, cuckooHashTable, dest[h]);
        }

        return std::move(dest);
    }

    void ComPsiServer::selectCuckooPos(u32 destRows, u32 srcRows, u32 bytes)
    {
        OblvSwitchNet snet(std::to_string(mIdx));
        for (u64 h = 0; h < 3; ++h)
        {
            snet.help(mComm.mPrev, mComm.mNext, mPrng, destRows, srcRows, bytes);
        }
    }

    std::array<Matrix<u8>,3> ComPsiServer::selectCuckooPos(
        MatrixView<u8> cuckooHashTable,
        u64 rows,
        aby3::Sh3::i64Matrix & keys)
    {
        if (mIdx != 1)
            throw std::runtime_error("");

        auto cuckooParams = CuckooIndex<>::selectParams(keys.rows(), ComPsiServer_ssp, 0, 3);
        auto numBins = cuckooParams.numBins();

        span<block> view((block*)keys.data(), keys.rows());

        auto cols = cuckooHashTable.cols();

        std::array<Matrix<u8>, 3> dest;
        dest[0].resize(rows, cols, oc::AllocType::Uninitialized);
        dest[1].resize(rows, cols, oc::AllocType::Uninitialized);
        dest[2].resize(rows, cols, oc::AllocType::Uninitialized);
        //if (dest[0].cols() != cuckooHashTable.cols())
        //    throw std::runtime_error("");

        //if (dest[0].rows() != keys.rows())
        //    throw std::runtime_error("");

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


        return std::move(dest);
    }

    void ComPsiServer::compare(
        SharedTable::ColRef leftJoinCol,
        SharedTable::ColRef rightJoinCol,
        span<Matrix<u8>> inShares, 
        span<SharedTable::ColRef> outColumns, 
        aby3::Sh3::sPackedBin& outFlags)
    {
        auto size = inShares[0].rows();
        
        //auto bitCount = std::accumulate(outColumns.begin(), outColumns.end(), leftJoinCol.mCol.getBitCount(), [](auto iter) { return iter->->mCol.getBitCount();  });
        auto byteCount = leftJoinCol.mCol.getByteCount();
        for (auto& c : outColumns)
            byteCount += c.mCol.getByteCount();

        std::array<aby3::Sh3::sPackedBin, 3> A;
        A[0].reset(size, byteCount * 8);
        A[1].reset(size, byteCount * 8);
        A[2].reset(size, byteCount * 8);

        auto t0 = mEnc.localPackedBinary(mRt.noDependencies(), inShares[0], A[0], true);
        auto t1 = mEnc.localPackedBinary(mRt.noDependencies(), inShares[1], A[1], true);
        auto t2 = mEnc.localPackedBinary(mRt.noDependencies(), inShares[2], A[2], true);
        t0.getRuntime().runOne();

        auto cir = getBasicCompare(leftJoinCol, outColumns);

        aby3::Sh3BinaryEvaluator eval;

        if (ComPsiServer_debug)
            eval.enableDebug(mIdx, mComm.mPrev, mComm.mNext);

        eval.setCir(&cir, size);
        eval.setInput(0, rightJoinCol.mCol);
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

        eval.getOutput(0, outFlags);
        
        for (u64 i = 0; i < outColumns.size(); ++i)
            eval.getOutput(i + 1, outColumns[i].mCol);

        if (ComPsiServer_debug)
        {

            aby3::Sh3::i64Matrix bb(rightJoinCol.mCol.rows(), rightJoinCol.mCol.i64Cols());
            mEnc.revealAll(mComm, rightJoinCol.mCol, bb);
            std::array<Matrix<u8>, 3> aa, select;
            aa[0].resize(inShares[0].rows(), inShares[0].cols());
            aa[1].resize(inShares[1].rows(), inShares[1].cols());
            aa[2].resize(inShares[2].rows(), inShares[2].cols());
            select[0].resize(inShares[0].rows(), inShares[0].cols());
            select[1].resize(inShares[1].rows(), inShares[1].cols());
            select[2].resize(inShares[2].rows(), inShares[2].cols());

            if (mIdx == 0)
            {
                mComm.mNext.asyncSend(inShares[0].data(), inShares[0].size());
                mComm.mNext.asyncSend(inShares[1].data(), inShares[1].size());
                mComm.mNext.asyncSend(inShares[2].data(), inShares[2].size());
                mComm.mNext.recv(aa[0].data(), aa[0].size());
                mComm.mNext.recv(aa[1].data(), aa[1].size());
                mComm.mNext.recv(aa[2].data(), aa[2].size());
            }
            else
            {
                mComm.mPrev.asyncSend(inShares[0].data(), inShares[0].size());
                mComm.mPrev.asyncSend(inShares[1].data(), inShares[1].size());
                mComm.mPrev.asyncSend(inShares[2].data(), inShares[2].size());
                mComm.mPrev.recv(aa[0].data(), aa[0].size());
                mComm.mPrev.recv(aa[1].data(), aa[1].size());
                mComm.mPrev.recv(aa[2].data(), aa[2].size());
            }
            std::vector<std::array<bool, 3>> exp(size);
            auto compareBytes = (leftJoinCol.mCol.getBitCount() + 7) / 8;

            {

                ostreamLock o(std::cout);

                for (auto i = 0; i < inShares[0].rows(); ++i)
                {
                    for (auto j = 0; j < inShares[0].cols(); ++j)
                    {
                        select[0](i, j) = aa[0](i, j) ^ inShares[0](i, j);
                        select[1](i, j) = aa[1](i, j) ^ inShares[1](i, j);
                        select[2](i, j) = aa[2](i, j) ^ inShares[2](i, j);
                    }

                    if (debug_print)
                        std::cout << "p " << mIdx << " select[0][" << i << "] = " << hexString(&select[0](i, 0), inShares[0].cols()) << " = " << hexString(&aa[0](i, 0), inShares[0].cols()) << " ^ " << hexString(&inShares[0](i, 0), inShares[0].cols()) << std::endl;
                }


                for (auto i = 0; i < size; ++i)
                {
                    if (debug_print)
                        std::cout << "select["<< mIdx<<"][" << i << "] "
                        << hexString(select[0][i].data(), compareBytes) << " "
                        << hexString(select[1][i].data(), compareBytes) << " "
                        << hexString(select[2][i].data(), compareBytes) << " vs "
                        << hexString((u8*)bb.row(i).data(), compareBytes) << std::endl;;

                    if (memcmp(select[0][i].data(), bb.row(i).data(), compareBytes) == 0)
                        exp[i][0] = true;
                    if (memcmp(select[1][i].data(), bb.row(i).data(), compareBytes) == 0)
                        exp[i][1] = true;
                    if (memcmp(select[2][i].data(), bb.row(i).data(), compareBytes) == 0)
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

            aby3::Sh3::sPackedBin r0(outFlags.shareCount(), outFlags.bitCount());
            aby3::Sh3::sPackedBin r1(outFlags.shareCount(), outFlags.bitCount());
            aby3::Sh3::sPackedBin r2(outFlags.shareCount(), outFlags.bitCount());
            r0.mShares[0].setZero();
            r0.mShares[1].setZero();
            r1.mShares[0].setZero();
            r1.mShares[1].setZero();
            r2.mShares[0].setZero();
            r2.mShares[1].setZero();
            eval.getOutput(cir.mOutputs.size() - 3, r0);
            eval.getOutput(cir.mOutputs.size() - 2, r1);
            eval.getOutput(cir.mOutputs.size() - 1, r2);

            aby3::Sh3::PackedBin iflag(r0.shareCount(), r0.bitCount());
            aby3::Sh3::PackedBin rr0(r0.shareCount(), r0.bitCount());
            aby3::Sh3::PackedBin rr1(r1.shareCount(), r1.bitCount());
            aby3::Sh3::PackedBin rr2(r2.shareCount(), r2.bitCount());

            mEnc.revealAll(mRt.noDependencies(), outFlags, iflag);
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
                    << " b  " <<  hexString((u8*)bb.row(i).data(), compareBytes) << "\n"
                    << " a0 " <<  hexString((u8*)select[0][i].data(), compareBytes) << "\n"
                    << " a1 " <<  hexString((u8*)select[1][i].data(), compareBytes) << "\n"
                    << " a2 " <<  hexString((u8*)select[2][i].data(), compareBytes) << "\n"
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

    void ComPsiServer::compare(
        SharedTable::ColRef leftJoinCol,
        SharedTable::ColRef rightJoinCol,
        span<SharedTable::ColRef> outColumns,
        aby3::Sh3::sPackedBin & outFlags)
    {
        auto size = rightJoinCol.mTable.rows();

        auto byteCount = leftJoinCol.mCol.getByteCount();
        for (auto& c : outColumns)
            byteCount += c.mCol.getByteCount();

        aby3::Sh3::sPackedBin
            A0(size, byteCount * 8),
            A1(size, byteCount * 8),
            A2(size, byteCount * 8);

        auto t0 = mEnc.remotePackedBinary(mRt.noDependencies(), A0);
        auto t1 = mEnc.remotePackedBinary(mRt.noDependencies(), A1);
        auto t2 = mEnc.remotePackedBinary(mRt.noDependencies(), A2);
        t0.getRuntime().runOne();

        auto cir = getBasicCompare(leftJoinCol, outColumns);

        aby3::Sh3BinaryEvaluator eval;
        if (ComPsiServer_debug)
            eval.enableDebug(mIdx, mComm.mPrev, mComm.mNext);
        eval.setCir(&cir, size);
        eval.setInput(0, rightJoinCol.mCol);
        t0.get();
        eval.setInput(1, A0);
        t1.get();
        eval.setInput(2, A1);
        t2.get();
        eval.setInput(3, A2);


        eval.distributeInputs();

        mRt.runAll();

        eval.asyncEvaluate(mRt.noDependencies()).get();

        eval.getOutput(0, outFlags);
        for (u64 i = 0; i < outColumns.size(); ++i)
            eval.getOutput(i + 1, outColumns[i].mCol);

        if (ComPsiServer_debug)
        {

            aby3::Sh3::i64Matrix bb(rightJoinCol.mCol.rows(), rightJoinCol.mCol.i64Cols());
            mEnc.revealAll(mComm, rightJoinCol.mCol, bb);

            aby3::Sh3::sPackedBin a0(outFlags.shareCount(), outFlags.bitCount());
            aby3::Sh3::sPackedBin a1(outFlags.shareCount(), outFlags.bitCount());
            aby3::Sh3::sPackedBin a2(outFlags.shareCount(), outFlags.bitCount());

            a0.mShares[0].setZero();
            a0.mShares[1].setZero();
            a1.mShares[0].setZero();
            a1.mShares[1].setZero();
            a2.mShares[0].setZero();
            a2.mShares[1].setZero();
            eval.getOutput(cir.mOutputs.size() - 3, a0);
            eval.getOutput(cir.mOutputs.size() - 2, a1);
            eval.getOutput(cir.mOutputs.size() - 1, a2);

            aby3::Sh3::PackedBin iflag(a0.shareCount(), a0.bitCount());
            aby3::Sh3::PackedBin rr0(a0.shareCount(), a0.bitCount());
            aby3::Sh3::PackedBin rr1(a0.shareCount(), a0.bitCount());
            aby3::Sh3::PackedBin rr2(a0.shareCount(), a0.bitCount());


            mEnc.revealAll(mRt.noDependencies(), outFlags, iflag);
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
                    o << "circuit[" << mIdx << "][" << i << "] -> " << int(ff) << " = (" << int(ii0) << " " << int(ii1) << " " << int(ii2) << ") " << hexString((u8*)bb.row(i).data(), bb.cols() * sizeof(i64)) << std::endl;
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

    BetaCircuit ComPsiServer::getBasicCompare(
        SharedTable::ColRef leftJoinCol,
        span<SharedTable::ColRef> cols)
    {
        BetaCircuit r;

        u64 compareBitCount = leftJoinCol.mCol.getBitCount();
        u64 totalBitCount = compareBitCount;
        for (auto& c : cols)
            totalBitCount += c.mCol.getBitCount();

        u64 selectStart = compareBitCount;
        u64 selectEnd = totalBitCount;

        BetaBundle a0(totalBitCount), a1(totalBitCount), a2(totalBitCount), b(compareBitCount);
        BetaBundle out(1), c0(1), c1(1), c2(1);
        r.addInputBundle(b);
        r.addInputBundle(a0);
        r.addInputBundle(a1);
        r.addInputBundle(a2);
        r.addOutputBundle(out);

        std::vector<BetaBundle> selectOut(cols.size());
        auto iterW = selectOut.begin();
        auto iterC = cols.begin();

        while (iterW != selectOut.end())
        {
            iterW->mWires.resize(iterC++->mCol.getBitCount());
            r.addOutputBundle(*iterW++);
        }

        if (ComPsiServer_debug)
        {
            r.addOutputBundle(c0);
            r.addOutputBundle(c1);
            r.addOutputBundle(c2);
        }

        BetaBundle t0(compareBitCount), t1(compareBitCount), t2(compareBitCount);
        r.addTempWireBundle(t0);
        r.addTempWireBundle(t1);
        r.addTempWireBundle(t2);

        // compute a0 = a0 ^ b ^ 1
        // compute a1 = a1 ^ b ^ 1
        // compute a2 = a2 ^ b ^ 1
        for (auto i = 0; i < compareBitCount; ++i)
        {
            r.addGate(a0[i], b[i], GateType::Nxor, t0[i]); 
            r.addGate(a1[i], b[i], GateType::Nxor, t1[i]);
            r.addGate(a2[i], b[i], GateType::Nxor, t2[i]);
        }

        // check if a0,a1, or a2 are all ones, meaning ai == b
        while (compareBitCount != 1)
        {
            for (u64 i = 0, j = compareBitCount - 1; i < j; ++i, --j)
            {
                r.addGate(t0[i], t0[j], GateType::And, t0[i]);
                r.addGate(t1[i], t1[j], GateType::And, t1[i]);
                r.addGate(t2[i], t2[j], GateType::And, t2[i]);
            }
            compareBitCount = (compareBitCount + 1) / 2;
        }


        // return the parity if the three eq tests. 
        // this will be 1 if a single items matchs. 
        // We should never have 2 matches so this is 
        // effectively the same as using GateType::Or
        r.addGate(t0[0], t1[0], GateType::Xor, out[0]);
        r.addGate(t2[0], out[0], GateType::Xor, out[0]);


        // compute the select strings by and with the match bit (t0,t1,t2) and then
        // XORing the results to get the select strings.
        for (auto& out : selectOut)
        {
            for (u64 i = 0; i < out.size(); ++i)
            {
                // set the input string to zero if tis not a match
                r.addGate(a0[selectStart], t0[0], GateType::And, a0[selectStart]);
                r.addGate(a1[selectStart], t1[0], GateType::And, a1[selectStart]);
                r.addGate(a2[selectStart], t2[0], GateType::And, a2[selectStart]);

                // only one will be a match, therefore we can XOR the masked strings
                // as a way to only get the matching string, if there was one (zero otherwise).
                r.addGate(a0[selectStart], a1[selectStart], GateType::Xor, out[i]);
                r.addGate(a2[selectStart], out[i], GateType::Xor, out[i]);

                ++selectStart;
            }
        }

        // output the match bits if we are debugging.
        if (ComPsiServer_debug)
        {
            r.addCopy(t0[0], c0[0]);
            r.addCopy(t1[0], c1[0]);
            r.addCopy(t2[0], c2[0]);
        }

        return r;
    }

    u64 SharedTable::rows()
    {
        return mColumns.size() ? mColumns[0].rows() : 0;
    }
}

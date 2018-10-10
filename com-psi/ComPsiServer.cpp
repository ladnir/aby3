#include "ComPsiServer.h"
#include "OblvPermutation.h"
#include <iomanip>
#include "OblvSwitchNet.h"
#include "aby3/Engines/sh3/Sh3Converter.h"

namespace osuCrypto
{
    int ComPsiServer_ssp = 40;
    bool ComPsiServer_debug = false;
    bool debug_print = false;
    void ComPsiServer::init(u64 idx, Session & prev, Session & next)
    {
        mIdx = idx;

        //mComm = { prev.addChannel(), next.addChannel() };

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

		mRt.mComm.mNext.asyncSendCopy(rows);
		mRt.mComm.mPrev.asyncSendCopy(rows);

        std::vector<std::array<u64, 2>> sizes(t.mColumns.size());
        for (u64 i = 0; i < t.mColumns.size(); ++i)
            sizes[i] = { t.mColumns[i].getBitCount(), (u64)t.mColumns[i].getTypeID() };

		mRt.mComm.mNext.asyncSendCopy(sizes);
		mRt.mComm.mPrev.asyncSend(std::move(sizes));

        for (u64 i = 0; i < t.mColumns.size(); ++i)
        {
            ret.mColumns[i].mType = t.mColumns[i].mType;
            ret.mColumns[i].mName = t.mColumns[i].mName;
            ret.mColumns[i].resize(rows, t.mColumns[i].getBitCount());

			mRt.mComm.mNext.asyncSendCopy(t.mColumns[i].mName);
			mRt.mComm.mPrev.asyncSendCopy(t.mColumns[i].mName);

            aby3::Sh3::sbMatrix& a1 = ret.mColumns[i];

            mEnc.localBinMatrix(mRt.mComm, t.mColumns[i].mData, a1);
        }

        return ret;
    }

    SharedTable ComPsiServer::remoteInput(u64 partyIdx)
    {
        SharedTable ret;
        auto chl = ((mIdx + 1) % 3 == partyIdx) ? mRt.mComm.mNext : mRt.mComm.mPrev;

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
        mRt.mComm.mNext.send(cuckooTable.data(), cuckooTable.size());
        mRt.mComm.mNext.send(a2[0].data(), a2[0].size());
        mRt.mComm.mNext.send(a2[1].data(), a2[1].size());
        mRt.mComm.mNext.send(a2[2].data(), a2[2].size());
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

        mRt.mComm.mPrev.recv(c2.data(), c2.size());
        mRt.mComm.mPrev.recv(a3[0].data(), a3[0].size());
        mRt.mComm.mPrev.recv(a3[1].data(), a3[1].size());
        mRt.mComm.mPrev.recv(a3[2].data(), a3[2].size());


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

	// join on leftJoinCol == rightJoinCol and select the select values.

	SharedTable ComPsiServer::join(SharedTable::ColRef leftJoinCol, SharedTable::ColRef rightJoinCol, std::vector<SharedTable::ColRef> selects)
	{
		SelectQuery query;
		//query.noReveal("nr");
		auto jc = query.joinOn(leftJoinCol, rightJoinCol);

		for (auto& s : selects)
		{
			if (&s.mCol == &leftJoinCol.mCol || &s.mCol == &rightJoinCol.mCol)
			{
				query.addOutput(s.mCol.mName, jc);
			}
			else
			{
				query.addOutput(s.mCol.mName, query.addInput(s));
			}
		}

		return joinImpl(query);
	}


    SharedTable ComPsiServer::joinImpl(
        const SelectQuery& query)
    {

        setTimePoint("intersect_start");
        auto& leftTable = *query.mLeftTable;
        auto& rightTable = *query.mRightTable;
        bool hideIntersection = query.mNoRevealName.size();

        // all of the columns out of the right table that need to be selected.
        SharedTable C;
        std::vector<SharedColumn*> leftCircuitInput, rightCircuitInput, circuitOutput;
        std::vector<std::array<SharedColumn*, 2>> leftPassthroughCols;
        
        constructOutTable(
            // outputs
            leftCircuitInput,
            rightCircuitInput,
            circuitOutput, 
            leftPassthroughCols,
            C,
            // inputs 
            query);




        std::vector<SharedTable::ColRef> srcColumns; srcColumns.reserve(query.mOutputs.size());
        for (u64 i = 0; i < query.mOutputs.size(); ++i)
        {
            auto& mem = query.mMem[query.mOutputs[i].mMemIdx];
            if (mem.isInput())
            {
                auto& colRef = query.mInputs[mem.mInputIdx].mCol;
                // if the select column is from the left table (which stays in place), we will simply copy the
                // shares out of the left table into the result table
                if (&colRef.mTable == &leftTable)
                    srcColumns.push_back(leftTable[colRef.mCol.mName]);

                // if the select column is from the right table (which gets permuted), but in the column that we are 
                // joining on, then observe that we can actaully use the left table's join column, since they will be equal
                // for all items in the intersection. This is more efficient since it does not need to go through the circuit.
                else if (&colRef.mTable == &rightTable &&&colRef.mCol == query.mRightCol)
                    srcColumns.push_back(leftTable[query.mLeftCol->mName]);

                // finally, if the select column is from the right table we will make the src column the output
                // column itself. Note that when we do the final copy, the src and dest columns will be the same, 
                // however, the copy will move the destRows around which is desired.
                else if (&colRef.mTable == &rightTable)
                    srcColumns.push_back(C[colRef.mCol.mName]);
                // throw if the select column is not from the left or right table.
                else
                    throw RTE_LOC;
            }
            else
                srcColumns.push_back(C[query.mOutputs[i].mName]);
        }

        setTimePoint("intersect_preamble");


        std::array<SharedTable::ColRef, 2> AB{ 
            leftTable[query.mLeftCol->mName],
            rightTable[query.mRightCol->mName]
        };
        std::array<u64, 2> reveals{ 1,0 };
        aby3::Sh3::i64Matrix keys = computeKeys(AB, reveals);
        setTimePoint("intersect_compute_keys");


        // construct a cuckoo table for the right table, then use the keys from left table to select out of the cuckoo table to 
        // get three shares of the cuckoo table entries, one for each cuckoo hash function. 
        std::array<Matrix<u8>, 3> circuitInputShare =
            mapRightTableToLeft(keys, rightCircuitInput, leftTable, rightTable);

        // now perform the comparison between the entry from the left table with the three possible matched 
        // which were selected out of the cuckoo table.
        aby3::Sh3::sPackedBin intersectionFlags =
            compare(leftCircuitInput, rightCircuitInput, circuitOutput, query, circuitInputShare);

        setTimePoint("intersect_compare");

        if (query.isNoReveal())
        {
            aby3::Sh3Converter convt;
            convt.toBinaryMatrix(intersectionFlags, C.mColumns.back());
            C.mColumns.back().mName = query.mNoRevealName;

            for (u64 j = 0; j < srcColumns.size(); ++j)
            {
                if (&srcColumns[j].mCol != &C.mColumns[j])
                {
                    C.mColumns[j].mShares = srcColumns[j].mCol.mShares;
                }
            }
        }
        else
        {
            aby3::Sh3::PackedBin plainFlags(leftTable.rows(), 1);
            mEnc.revealAll(mRt.noDependencies(), intersectionFlags, plainFlags).get();
            setTimePoint("intersect_done");
            BitIterator iter((u8*)plainFlags.mData.data(), 0);

            std::vector<u64> sizes(srcColumns.size());
            for (u64 i = 0; i < sizes.size(); ++i)
                sizes[i] = srcColumns[i].mCol.getByteCount();

            auto size = 0;
            for (u64 i = 0; i < leftTable.rows(); ++i)
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
        }

        return C;
    }



    SharedTable ComPsiServer::rightUnion(
        SharedTable::ColRef leftJoinCol,
        SharedTable::ColRef rightJoinCol,
        std::vector<SharedTable::ColRef> leftSelects,
        std::vector<SharedTable::ColRef> rightSelects)
    {
        setTimePoint("union_start");
        //throw RTE_LOC;

        auto numCols = leftSelects.size();

        if (leftSelects.size() != rightSelects.size())
            throw RTE_LOC;

        for (u64 i = 0; i < numCols; ++i)
        {
            if (leftSelects[i].mCol.getTypeID() != rightSelects[i].mCol.getTypeID() ||
                leftSelects[i].mCol.getBitCount() != rightSelects[i].mCol.getBitCount())
                throw RTE_LOC;
        }

        auto& leftTable = leftJoinCol.mTable;
        auto& rightTable = rightJoinCol.mTable;
        auto maxRows = leftTable.rows() + rightTable.rows();

        SelectQuery query;
        auto jc = query.joinOn(leftJoinCol, rightJoinCol);
        for (auto& c : leftSelects)
        {
            if (&leftJoinCol.mCol == &c.mCol)
                query.addOutput(c.mCol.mName, jc);
            else
                query.addOutput(c.mCol.mName, query.addInput(c));
        }

        query.isUnion(true);

        // all of the columns out of the right table that need to be selected.
        //std::vector<SharedTable::ColRef> circuitInputCols, circuitOutCols;
        //SharedTable C;
        // all of the columns out of the right table that need to be selected.
        SharedTable C;
        std::vector<SharedColumn*> leftCircuitInput, rightCircuitInput, circuitOutput;
        std::vector<std::array<SharedColumn*, 2>> leftPassthroughCols;

        constructOutTable(
            // outputs
            leftCircuitInput,
            rightCircuitInput,
            circuitOutput,
            leftPassthroughCols,
            C,
            // inputs 
            query);

        //constructOutTable(circuitInputCols, circuitOutCols, C, rightJoinCol, leftJoinCol, leftSelects, maxRows, false);

        std::array<SharedTable::ColRef, 2> AB{ leftJoinCol,rightJoinCol };
        std::array<u64, 2> reveals{ 1,0 };
        aby3::Sh3::i64Matrix keys = computeKeys(AB, reveals);
        setTimePoint("union_compute_keys");

        // construct a cuckoo table for the right table, then use the keys from left table to select out of the cuckoo table to 
        // get three shares of the cuckoo table entries, one for each cuckoo hash function. 
        std::array<Matrix<u8>, 3> circuitInputShare =
            mapRightTableToLeft(keys, rightCircuitInput, leftTable, rightTable);

        // now perform the comparison between the entry from the left table with the three possible matched 
        // which were selected out of the cuckoo table.
        aby3::Sh3::sPackedBin intersectionFlags =
            unionCompare(leftJoinCol, rightJoinCol, circuitInputShare);

        setTimePoint("union_compare");

        aby3::Sh3::PackedBin plainFlags(leftTable.rows(), 1);
        mEnc.revealAll(mRt.noDependencies(), intersectionFlags, plainFlags).get();
        setTimePoint("union_done");
        BitIterator iter((u8*)plainFlags.mData.data(), 0);

        std::vector<u64> sizes(numCols);
        for (u64 i = 0; i < sizes.size(); ++i)
            sizes[i] = leftSelects[i].mCol.getByteCount();

        auto size = rightSelects[0].mCol.rows();

        for (u64 j = 0; j < numCols; ++j)
        {
            for (auto b : { 0, 1 })
                memcpy(
                    C.mColumns[j].mShares[b].data(),
                    rightSelects[j].mCol.mShares[b].data(),
                    rightSelects[j].mCol.mShares[b].size() * sizeof(i64));
        }

        for (u64 i = 0; i < leftTable.rows(); ++i)
        {
            if (*iter == 0)
            {
                for (u64 j = 0; j < numCols; ++j)
                {
                    auto s0 = &leftSelects[j].mCol.mShares[0](i, 0);
                    auto s1 = &leftSelects[j].mCol.mShares[1](i, 0);
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




    std::array<Matrix<u8>, 3> ComPsiServer::mapRightTableToLeft(
        aby3::Sh3::i64Matrix& keys,
        span<SharedColumn*> circuitInputCols,
        SharedTable& leftTable,
        SharedTable& rightTable)
    {

        auto cuckooParams = CuckooIndex<>::selectParams(rightTable.rows(), ComPsiServer_ssp, 0, 3);

        std::array<Matrix<u8>, 3> circuitInputShare;
        switch (mIdx)
        {
        case 0:
        {
            if (keys.rows() != circuitInputCols[0]->rows())
                throw RTE_LOC;

            // place the right table's select columns into a cuckoo table using the keys as hash values.
            auto cuckooTable = cuckooHash(circuitInputCols, cuckooParams, keys);
            setTimePoint("intersect_cuckoo_hash");

            // based on the keys for the left table, select the corresponding entries out of
            // the cuckoo hash table. circuitInputShare will hold three shares per left table entry, 
            // one share for each of the three hash function.
            circuitInputShare = selectCuckooPos(cuckooTable, leftTable.rows());
            setTimePoint("intersect_select_cuckoo");

            // debug check to see if this is done correctly.
            if (ComPsiServer_debug)
                p0CheckSelect(cuckooTable, circuitInputShare);

            break;
        }
        case 1:
        {

            // place the right table's select columns into a cuckoo table using the keys as hash values.
            auto cuckooTable = cuckooHashRecv(circuitInputCols);
            setTimePoint("intersect_cuckoo_hash");

            // based on the keys for the left table, select the corresponding entries out of
            // the cuckoo hash table. circuitInputShare will hold three shares per left table entry, 
            // one share for each of the three hash function.
            circuitInputShare = selectCuckooPos(cuckooTable, leftTable.rows(), cuckooParams, keys);
            setTimePoint("intersect_select_cuckoo");

            // debug check to see if this is done correctly.
            if (ComPsiServer_debug)
                p1CheckSelect(cuckooTable, circuitInputShare, keys);
            break;
        }
        case 2:
        {
            u64 selectByteCount = 0;
            for (auto& c : circuitInputCols)
                selectByteCount += c->getByteCount();

            // place the right table's select columns into a cuckoo table using the keys as hash values.
            cuckooHashSend(circuitInputCols, cuckooParams);
            setTimePoint("intersect_cuckoo_hash");

            // based on the keys for the left table, select the corresponding entries out of
            // the cuckoo hash table. circuitInputShare will hold three shares per left table entry, 
            // one share for each of the three hash function.
            selectCuckooPos(leftTable.rows(), cuckooParams.numBins(), selectByteCount);
            setTimePoint("intersect_select_cuckoo");
            break;
        }
        default:
            throw std::runtime_error("");
        }

        return std::move(circuitInputShare);
    }


    void ComPsiServer::constructOutTable(
        std::vector<SharedColumn*> &leftCircuitInput,
        std::vector<SharedColumn*> &rightCircuitInput,
        std::vector<SharedColumn*> &circuitOutCols,
        std::vector<std::array<SharedColumn*,2>>& leftPassthroughCols,
        SharedTable &C,
        const SelectQuery& query)
    {
        auto& rightTable = *query.mRightTable;
        auto& leftTable = *query.mLeftTable;
        auto numRows = query.mLeftTable->rows() + query.mRightTable->rows() * query.isUnion();

        rightCircuitInput.reserve(rightTable.mColumns.size());
        leftCircuitInput.reserve(leftTable.mColumns.size());

        C.mColumns.resize(query.mOutputs.size() + query.isNoReveal());

        if (query.isNoReveal())
        {

            C.mColumns.back().mType = std::make_shared<IntType>(1);
            C.mColumns.back().mName = query.mNoRevealName;
            C.mColumns.back().resize(numRows, C.mColumns.back().mType->getBitCount());
            C.mColumns.back().mShares[0].setZero();
            C.mColumns.back().mShares[1].setZero();
        }

        for (u64 j = 0; j < query.mOutputs.size(); ++j)
        {
            C.mColumns[j].mType = query.mMem[query.mOutputs[j].mMemIdx].mType;
            C.mColumns[j].mName = query.mOutputs[j].mName;
            C.mColumns[j].resize(numRows, C.mColumns[j].mType->getBitCount());
            C.mColumns[j].mShares[0].setZero();
            C.mColumns[j].mShares[1].setZero();
            
            if (query.isLeftPassthrough(query.mOutputs[j]))
            {
                auto& mem = query.mMem[query.mOutputs[j].mMemIdx];
                leftPassthroughCols.push_back({
                    &query.mInputs[mem.mInputIdx].mCol.mCol,
                    &C.mColumns[j] });
            }
            else
            {
                circuitOutCols.push_back(&C.mColumns[j]);
            }
        }

        for (u64 j = 0; j < query.mInputs.size(); ++j)
        {
            if (query.isCircuitInput(query.mInputs[j]))
            {
                if(&query.mInputs[j].mCol.mTable == query.mRightTable)
                    rightCircuitInput.push_back(&query.mInputs[j].mCol.mCol);
                else
                    leftCircuitInput.push_back(&query.mInputs[j].mCol.mCol);
            }
        }
    }










    Matrix<u8> ComPsiServer::cuckooHash(span<SharedColumn*> selects, CuckooParam& params, aby3::Sh3::i64Matrix& keys)
    {
        if (mIdx != 0)
            throw std::runtime_error(LOCATION);

        CuckooIndex<CuckooTypes::NotThreadSafe> mCuckoo;
        auto& cuckoo = mCuckoo;
        cuckoo.init(params);


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
            strides[j] = selects[j]->getByteCount();
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
                    auto& t = *selects[j];

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
        oblvPerm.program(mRt.mComm.mNext, mRt.mComm.mPrev, std::move(perm), mPrng, share0, (std::to_string(mIdx)) + "_cuckoo_hash", OutputType::Additive);


        //{
        //    Matrix<u8> share1(cuckoo.mBins.size(), (leftTable.mKeys.bitCount() + 7) / 8);
        //    mComm.mNext.recv(share1.data(), share1.size());

        //    std::vector<u8> temp(share0.cols());

        //    for (u64 i = 0; i < share1.destRows(); ++i)
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


    void ComPsiServer::cuckooHashSend(span<SharedColumn*> selects, CuckooParam& cuckooParams)
    {
        if (mIdx != 2)
            throw std::runtime_error(LOCATION);


        u64 totalBytes = 0;
        std::vector<u64> strides(selects.size());
        for (u64 j = 0; j < selects.size(); ++j)
        {
            strides[j] = selects[j]->getByteCount();
            totalBytes += strides[j];
        }

        Matrix<u8> share1(cuckooParams.numBins(), totalBytes);

        //auto dest = share1.data();
        auto rows = selects[0]->rows();
        for (u32 i = 0; i < rows; ++i)
        {

            u64 h = 0;
            for (u64 j = 0; j < strides.size(); ++j)
            {
                auto& t = *selects[j];
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
        oblvPerm.send(mRt.mComm.mNext, mRt.mComm.mPrev, std::move(share1), (std::to_string(mIdx)) + "_cuckoo_hash_send");

        //mEnc.reveal(mRt.mComm, 0, leftTable.mKeys);
    }

    Matrix<u8> ComPsiServer::cuckooHashRecv(span<SharedColumn*> selects)
    {

        if (mIdx != 1)
            throw std::runtime_error(LOCATION);

        u64 totalBytes = 0;
        std::vector<u64> strides(selects.size());
        for (u64 j = 0; j < selects.size(); ++j)
        {
            strides[j] = selects[j]->getByteCount();
            totalBytes += strides[j];
        }

        auto rows = selects[0]->rows();
        auto cuckooParams = CuckooIndex<>::selectParams(rows, ComPsiServer_ssp, 0, 3);
        Matrix<u8> share1(cuckooParams.numBins(), totalBytes);

        share1.setZero();

        OblvPermutation oblvPerm;
        oblvPerm.recv(mRt.mComm.mPrev, mRt.mComm.mNext, share1, share1.rows(), (std::to_string(mIdx)) + "_cuckoo_hash_recv");


        //auto dest = share1.data();
        //for (u32 i = 0; i < share1.destRows(); ++i)
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
            snet.sendRecv(mRt.mComm.mNext, mRt.mComm.mPrev, cuckooHashTable, dest[h]);
        }

        return std::move(dest);
    }

    void ComPsiServer::selectCuckooPos(u32 destRows, u32 srcRows, u32 bytes)
    {
        OblvSwitchNet snet(std::to_string(mIdx));
        for (u64 h = 0; h < 3; ++h)
        {
            snet.help(mRt.mComm.mPrev, mRt.mComm.mNext, mPrng, destRows, srcRows, bytes);
        }
    }

    std::array<Matrix<u8>, 3> ComPsiServer::selectCuckooPos(
        MatrixView<u8> cuckooHashTable,
        u64 destRows,
        CuckooParam& cuckooParams,
        aby3::Sh3::i64Matrix & keys)
    {
        if (mIdx != 1)
            throw std::runtime_error("");

        //auto cuckooParams = CuckooIndex<>::selectParams(keys.rows(), ComPsiServer_ssp, 0, 3);
        auto numBins = cuckooParams.numBins();

        span<block> view((block*)keys.data(), keys.rows());

        auto cols = cuckooHashTable.cols();

        std::array<Matrix<u8>, 3> dest;
        dest[0].resize(destRows, cols, oc::AllocType::Uninitialized);
        dest[1].resize(destRows, cols, oc::AllocType::Uninitialized);
        dest[2].resize(destRows, cols, oc::AllocType::Uninitialized);
        //if (dest[0].cols() != cuckooHashTable.cols())
        //    throw std::runtime_error("");

        //if (dest[0].destRows() != keys.destRows())
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
            snet.program(mRt.mComm.mNext, mRt.mComm.mPrev, progs[h], mPrng, dest[h], OutputType::Additive);
        }


        return std::move(dest);
    }

    aby3::Sh3::sPackedBin ComPsiServer::compare(
        span<SharedColumn*> leftCircuitInput,
        span<SharedColumn*> rightCircuitInput,
        span<SharedColumn*> circuitOutput,
        const SelectQuery& query,
        span<Matrix<u8>> inShares)
    {
        auto size = query.mLeftTable->rows();

        //auto bitCount = std::accumulate(outColumns.begin(), outColumns.end(), leftJoinCol.mCol.getBitCount(), [](auto iter) { return iter->->mCol.getBitCount();  });
        auto byteCount = 0;
        for (auto& c : rightCircuitInput)
            byteCount += c->getByteCount();

        std::array<aby3::Sh3::sPackedBin, 3> A;
        A[0].reset(size, byteCount * 8);
        A[1].reset(size, byteCount * 8);
        A[2].reset(size, byteCount * 8);



        aby3::Sh3Task t0, t1, t2;
        if (inShares.size() && inShares[0].size())
        {

            if (A[0].shareCount() != inShares[0].rows())
                throw RTE_LOC;

            if (A[0].bitCount() != inShares[0].cols() * 8)
            {
                std::cout << A[0].bitCount() << " != " << inShares[0].cols() * 8 << std::endl;
                throw RTE_LOC;
            }

            t0 = mEnc.localPackedBinary(mRt.noDependencies(), inShares[0], A[0], true);
            t1 = mEnc.localPackedBinary(mRt.noDependencies(), inShares[1], A[1], true);
            t2 = mEnc.localPackedBinary(mRt.noDependencies(), inShares[2], A[2], true);
        }
        else
        {
            t0 = mEnc.remotePackedBinary(mRt.noDependencies(), A[0]);
            t1 = mEnc.remotePackedBinary(mRt.noDependencies(), A[1]);
            t2 = mEnc.remotePackedBinary(mRt.noDependencies(), A[2]);
        }

        mRt.runOne();
        auto cir = getQueryCircuit(leftCircuitInput, rightCircuitInput, circuitOutput, query);

        aby3::Sh3BinaryEvaluator eval;

        if (ComPsiServer_debug)
            eval.enableDebug(mIdx, mRt.mComm.mPrev, mRt.mComm.mNext);

        eval.setCir(&cir, size);

        u64 i = 0;
        for(; i < leftCircuitInput.size(); ++i)
            eval.setInput(i, *leftCircuitInput[i]);

        t0.get();
        eval.setInput(i++, A[0]);
        t1.get();
        eval.setInput(i++, A[1]);
        t2.get();
        eval.setInput(i++, A[2]);

        std::vector<std::vector<aby3::Sh3BinaryEvaluator::DEBUG_Triple>>plainWires;
        eval.distributeInputs();

        if (ComPsiServer_debug)
            plainWires = eval.mPlainWires_DEBUG;

        mRt.runAll();

        eval.asyncEvaluate(mRt.noDependencies()).get();

        aby3::Sh3::sPackedBin outFlags(size, 1);
        eval.getOutput(0, outFlags);

        for (u64 i = 0; i < circuitOutput.size(); ++i)
            eval.getOutput(i + 1, *circuitOutput[i]);

        //if (ComPsiServer_debug)
        //{



        //    aby3::Sh3::i64Matrix bb(rightJoinCol.mCol.rows(), rightJoinCol.mCol.i64Cols());
        //    mEnc.revealAll(mComm, rightJoinCol.mCol, bb);
        //    std::array<Matrix<u8>, 3> aa, select;
        //    aa[0].resize(inShares[0].rows(), inShares[0].cols());
        //    aa[1].resize(inShares[1].rows(), inShares[1].cols());
        //    aa[2].resize(inShares[2].rows(), inShares[2].cols());
        //    select[0].resize(inShares[0].rows(), inShares[0].cols());
        //    select[1].resize(inShares[1].rows(), inShares[1].cols());
        //    select[2].resize(inShares[2].rows(), inShares[2].cols());

        //    if (mIdx == 0)
        //    {
        //        mComm.mNext.asyncSend(inShares[0].data(), inShares[0].size());
        //        mComm.mNext.asyncSend(inShares[1].data(), inShares[1].size());
        //        mComm.mNext.asyncSend(inShares[2].data(), inShares[2].size());
        //        mComm.mNext.recv(aa[0].data(), aa[0].size());
        //        mComm.mNext.recv(aa[1].data(), aa[1].size());
        //        mComm.mNext.recv(aa[2].data(), aa[2].size());
        //    }
        //    else if (mIdx == 1)
        //    {
        //        mComm.mPrev.asyncSend(inShares[0].data(), inShares[0].size());
        //        mComm.mPrev.asyncSend(inShares[1].data(), inShares[1].size());
        //        mComm.mPrev.asyncSend(inShares[2].data(), inShares[2].size());
        //        mComm.mPrev.recv(aa[0].data(), aa[0].size());
        //        mComm.mPrev.recv(aa[1].data(), aa[1].size());
        //        mComm.mPrev.recv(aa[2].data(), aa[2].size());
        //    }

        //    std::vector<std::array<bool, 3>> exp(size);
        //    auto compareBytes = (leftJoinCol.mCol.getBitCount() + 7) / 8;

        //    if (mIdx != 2)
        //    {

        //        ostreamLock o(std::cout);

        //        for (auto i = 0; i < inShares[0].rows(); ++i)
        //        {
        //            for (auto j = 0; j < inShares[0].cols(); ++j)
        //            {
        //                select[0](i, j) = aa[0](i, j) ^ inShares[0](i, j);
        //                select[1](i, j) = aa[1](i, j) ^ inShares[1](i, j);
        //                select[2](i, j) = aa[2](i, j) ^ inShares[2](i, j);
        //            }

        //            if (debug_print)
        //                std::cout << "p " << mIdx << " select[0][" << i << "] = " << hexString(&select[0](i, 0), inShares[0].cols()) << " = " << hexString(&aa[0](i, 0), inShares[0].cols()) << " ^ " << hexString(&inShares[0](i, 0), inShares[0].cols()) << std::endl;
        //        }


        //        for (auto i = 0; i < size; ++i)
        //        {
        //            if (debug_print)
        //                std::cout << "select[" << mIdx << "][" << i << "] "
        //                << hexString(select[0][i].data(), compareBytes) << " "
        //                << hexString(select[1][i].data(), compareBytes) << " "
        //                << hexString(select[2][i].data(), compareBytes) << " vs "
        //                << hexString((u8*)bb.row(i).data(), compareBytes) << std::endl;;

        //            if (memcmp(select[0][i].data(), bb.row(i).data(), compareBytes) == 0)
        //                exp[i][0] = true;
        //            if (memcmp(select[1][i].data(), bb.row(i).data(), compareBytes) == 0)
        //                exp[i][1] = true;
        //            if (memcmp(select[2][i].data(), bb.row(i).data(), compareBytes) == 0)
        //                exp[i][2] = true;

        //            BitIterator iter0(select[0][i].data(), 0);
        //            BitIterator iter1(select[1][i].data(), 0);
        //            BitIterator iter2(select[2][i].data(), 0);

        //            for (u64 b = 0; b < cir.mInputs[0].size(); ++b)
        //            {
        //                if (*iter0++ != plainWires[i][cir.mInputs[1][b]].val())
        //                    throw RTE_LOC;
        //                if (*iter1++ != plainWires[i][cir.mInputs[2][b]].val())
        //                    throw RTE_LOC;
        //                if (*iter2++ != plainWires[i][cir.mInputs[3][b]].val())
        //                    throw RTE_LOC;
        //            }

        //        }
        //    }

        //    aby3::Sh3::sPackedBin r0(outFlags.shareCount(), outFlags.bitCount());
        //    aby3::Sh3::sPackedBin r1(outFlags.shareCount(), outFlags.bitCount());
        //    aby3::Sh3::sPackedBin r2(outFlags.shareCount(), outFlags.bitCount());
        //    r0.mShares[0].setZero();
        //    r0.mShares[1].setZero();
        //    r1.mShares[0].setZero();
        //    r1.mShares[1].setZero();
        //    r2.mShares[0].setZero();
        //    r2.mShares[1].setZero();
        //    eval.getOutput(cir.mOutputs.size() - 3, r0);
        //    eval.getOutput(cir.mOutputs.size() - 2, r1);
        //    eval.getOutput(cir.mOutputs.size() - 1, r2);

        //    aby3::Sh3::PackedBin iflag(r0.shareCount(), r0.bitCount());
        //    aby3::Sh3::PackedBin rr0(r0.shareCount(), r0.bitCount());
        //    aby3::Sh3::PackedBin rr1(r1.shareCount(), r1.bitCount());
        //    aby3::Sh3::PackedBin rr2(r2.shareCount(), r2.bitCount());

        //    mEnc.revealAll(mRt.noDependencies(), outFlags, iflag);
        //    mEnc.revealAll(mRt.noDependencies(), r0, rr0);
        //    mEnc.revealAll(mRt.noDependencies(), r1, rr1);
        //    mEnc.revealAll(mRt.noDependencies(), r2, rr2).get();

        //    BitIterator f((u8*)iflag.mData.data(), 0);
        //    BitIterator i0((u8*)rr0.mData.data(), 0);
        //    BitIterator i1((u8*)rr1.mData.data(), 0);
        //    BitIterator i2((u8*)rr2.mData.data(), 0);

        //    if (mIdx != 2)
        //    {
        //        ostreamLock o(std::cout);
        //        for (u64 i = 0; i < r0.shareCount(); ++i)
        //        {
        //            u8 ff = *f++;
        //            u8 ii0 = *i0++;
        //            u8 ii1 = *i1++;
        //            u8 ii2 = *i2++;

        //            auto t0 = ii0 != exp[i][0];
        //            auto t1 = ii1 != exp[i][1];
        //            auto t2 = ii2 != exp[i][2];

        //            if (debug_print || t0 || t1 || t2)
        //                o << "circuit[" << mIdx << "][" << i << "] "
        //                << " b  " << hexString((u8*)bb.row(i).data(), compareBytes) << "\n"
        //                << " a0 " << hexString((u8*)select[0][i].data(), compareBytes) << "\n"
        //                << " a1 " << hexString((u8*)select[1][i].data(), compareBytes) << "\n"
        //                << " a2 " << hexString((u8*)select[2][i].data(), compareBytes) << "\n"
        //                << " -> " << int(ff) << " = (" << int(ii0) << " " << int(ii1) << " " << int(ii2) << ")" << std::endl;

        //            if (t0)
        //                throw std::runtime_error("");

        //            if (t1)
        //                throw std::runtime_error("");

        //            if (t2)
        //                throw std::runtime_error("");

        //        }
        //    }
        //}


        return std::move(outFlags);
    }

    aby3::Sh3::sPackedBin ComPsiServer::unionCompare(
        SharedTable::ColRef leftJoinCol,
        SharedTable::ColRef rightJoinCol,
        span<Matrix<u8>> inShares)
    {

        auto size = leftJoinCol.mCol.rows();

        //auto bitCount = std::accumulate(outColumns.begin(), outColumns.end(), leftJoinCol.mCol.getBitCount(), [](auto iter) { return iter->->mCol.getBitCount();  });
        auto byteCount = leftJoinCol.mCol.getByteCount();

        std::array<aby3::Sh3::sPackedBin, 3> A;
        A[0].reset(size, byteCount * 8);
        A[1].reset(size, byteCount * 8);
        A[2].reset(size, byteCount * 8);

        aby3::Sh3Task t0, t1, t2;
        if (inShares.size() && inShares[0].size())
        {
            t0 = mEnc.localPackedBinary(mRt.noDependencies(), inShares[0], A[0], true);
            t1 = mEnc.localPackedBinary(mRt.noDependencies(), inShares[1], A[1], true);
            t2 = mEnc.localPackedBinary(mRt.noDependencies(), inShares[2], A[2], true);
        }
        else
        {
            t0 = mEnc.remotePackedBinary(mRt.noDependencies(), A[0]);
            t1 = mEnc.remotePackedBinary(mRt.noDependencies(), A[1]);
            t2 = mEnc.remotePackedBinary(mRt.noDependencies(), A[2]);
        }

        mRt.runOne();
        auto cir = getBasicCompareCircuit(leftJoinCol, {});

        aby3::Sh3BinaryEvaluator eval;

        if (ComPsiServer_debug)
            eval.enableDebug(mIdx, mRt.mComm.mPrev, mRt.mComm.mNext);

        eval.setCir(&cir, size);
        eval.setInput(0, leftJoinCol.mCol);
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

        aby3::Sh3::sPackedBin outFlags(size, 1);
        eval.getOutput(0, outFlags);

        return std::move(outFlags);
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

    BetaCircuit ComPsiServer::getQueryCircuit(
        span<SharedColumn*> leftCircuitInput,
        span<SharedColumn*> rightCircuitInput,
        span<SharedColumn*> circuitOutput,
        const SelectQuery& query)
    {
        BetaCircuit r;


        std::vector<std::pair<int, bool>> leftInputOrder(leftCircuitInput.size());
        std::vector<int> rightInputOrder(rightCircuitInput.size());
        auto leftIter = leftInputOrder.begin();
        auto rightIter = rightInputOrder.begin();

        for (int i = 0, j = 0; i < query.mInputs.size(); ++i)
        {
            if (query.isCircuitInput(query.mInputs[i]))
            {
                if (&query.mInputs[i].mCol.mTable == query.mLeftTable)
                    *leftIter++ = { j++, query.mMem[query.mInputs[i].mMemIdx].mUsed };
                else
                    *rightIter++ = j++;
            }
        }

        if (leftIter != leftInputOrder.end())
            throw RTE_LOC;
        if (rightIter != rightInputOrder.end())
            throw RTE_LOC;





        u64 totalByteCount = 0;
        for (auto& c : rightCircuitInput)
            totalByteCount += c->getByteCount();
        BetaBundle 
            a0(totalByteCount * 8), 
            a1(totalByteCount * 8), 
            a2(totalByteCount * 8),
            out(1);

        std::vector<BetaBundle> 
            leftBundles(leftCircuitInput.size()),
            outputBundles(circuitOutput.size()),
            midBundles(leftCircuitInput.size() + rightCircuitInput.size());


        for (u64 i = 0; i < leftBundles.size(); ++i)
        {
            leftBundles[i].mWires.resize(leftCircuitInput[i]->getBitCount());
            r.addInputBundle(leftBundles[i]);
        }

        r.addInputBundle(a0);
        r.addInputBundle(a1);
        r.addInputBundle(a2);


        r.addOutputBundle(out);
        for(u64 i =0; i <circuitOutput.size(); ++i)
        {
            outputBundles[i].mWires.resize(circuitOutput[i]->getBitCount());
            r.addOutputBundle(outputBundles[i]);
        }


        for (u64 i = 0; i < leftInputOrder.size(); ++i)
        {
            auto idx = leftInputOrder[i].first;
            auto used = leftInputOrder[i].second;

            if(used)
                midBundles[idx].mWires = leftBundles[i].mWires;
        }

        for (u64 i = 0; i < rightInputOrder.size(); ++i)
        {
            auto inputPos = rightInputOrder[i];
            auto input = query.mInputs[inputPos];
            auto& mem = query.mMem[input.mMemIdx];

            if (mem.isOutput())
            {
                midBundles[inputPos] =
                    outputBundles[query.mOutputs[mem.mOutputIdx].mPosition];
            }
            else if(mem.mUsed)
            {
                midBundles[inputPos].mWires.resize(query.mInputs[inputPos].mCol.mCol.getBitCount());
                r.addTempWireBundle(midBundles[inputPos]);
            }
        }


        u64 compareBitCount = leftCircuitInput[0]->getBitCount();
        BetaBundle t0(compareBitCount), t1(compareBitCount), t2(compareBitCount);
        r.addTempWireBundle(t0);
        r.addTempWireBundle(t1);
        r.addTempWireBundle(t2);

        // compute a0 = a0 ^ b ^ 1
        // compute a1 = a1 ^ b ^ 1
        // compute a2 = a2 ^ b ^ 1
        for (auto i = 0; i < compareBitCount; ++i)
        {
            r.addGate(a0[i], leftBundles[0][i], GateType::Nxor, t0[i]);
            r.addGate(a1[i], leftBundles[0][i], GateType::Nxor, t1[i]);
            r.addGate(a2[i], leftBundles[0][i], GateType::Nxor, t2[i]);
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






        u64 selectStart = leftCircuitInput[0]->getBitCount();
        // compute the select strings by and with the match bit (t0,t1,t2) and then
        // XORing the results to get the select strings.
        //for (auto& out : midBundles)

        for (u64 i = 0; i < rightInputOrder.size(); ++i)
        {
            selectStart = roundUpTo(selectStart, 8);

            auto& bundle = midBundles[rightInputOrder[i]];

            for (u64 i = 0; i < bundle.size(); ++i)
            {
                // set the input string to zero if tis not a match
                r.addGate(a0[selectStart], t0[0], GateType::And, a0[selectStart]);
                r.addGate(a1[selectStart], t1[0], GateType::And, a1[selectStart]);
                r.addGate(a2[selectStart], t2[0], GateType::And, a2[selectStart]);

                // only one will be a match, therefore we can XOR the masked strings
                // as a way to only get the matching string, if there was one (zero otherwise).
                r.addGate(a0[selectStart], a1[selectStart], GateType::Xor, bundle[i]);
                r.addGate(a2[selectStart], bundle[i], GateType::Xor, bundle[i]);

                ++selectStart;
            }
        }

        


        query.apply(r, midBundles, outputBundles);

        for (u64 i = 0; i < outputBundles.size(); ++i)
        {
            for (u64 j = 0; j < outputBundles[i].size(); ++j)
            {
                if(r.mWireFlags[outputBundles[i].mWires[j]] == BetaWireFlag::Uninitialized)
                    throw RTE_LOC;
            }
        }

        return r;
    }


    BetaCircuit ComPsiServer::getBasicCompareCircuit(
        SharedTable::ColRef leftJoinCol,
        span<SharedTable::ColRef> cols)
    {
        BetaCircuit r;

        u64 compareBitCount = leftJoinCol.mCol.getBitCount();
        u64 totalByteCount = leftJoinCol.mCol.getByteCount();;
        for (auto& c : cols)
            totalByteCount += c.mCol.getByteCount();

        u64 selectStart = compareBitCount;

        BetaBundle
            a0(totalByteCount * 8),
            a1(totalByteCount * 8),
            a2(totalByteCount * 8),
            b(compareBitCount);

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

            selectStart = roundUpTo(selectStart, 8);

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

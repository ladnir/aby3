#pragma once

#include <cryptoTools/Network/Channel.h>
#include <cryptoTools/Crypto/PRNG.h>
#include <cryptoTools/Common/MatrixView.h>
#include "aby3/Engines/sh3/Sh3BinaryEvaluator.h"
#include "aby3/Engines/sh3/Sh3Encryptor.h"
#include "aby3/Engines/sh3/Sh3Evaluator.h"
#include "LowMC.h"
#include <cryptoTools/Common/CuckooIndex.h>
#include <cryptoTools/Common/Timer.h>

#include "com-psi/Table.h"

namespace osuCrypto
{
    using CommPkg = aby3::Sh3::CommPkg;


    class ComPsiServer :public TimerAdapter
    {
    public:
        u64 mIdx, mKeyBitCount = 80;
        CommPkg mComm;
        PRNG mPrng;

        aby3::Sh3Runtime mRt;
        aby3::Sh3Encryptor mEnc;

        void init(u64 idx, Session& prev, Session& next);


        SharedTable localInput(Table& t);
        SharedTable remoteInput(u64 partyIdx);


        SharedTable intersect(SharedTable& A, SharedTable& B);


        // join on leftJoinCol == rightJoinCol and select the select values.
        SharedTable join(
            SharedTable::ColRef leftJoinCol,
            SharedTable::ColRef rightJoinCol,
            std::vector<SharedTable::ColRef> selects)
        {
            return joinImpl(leftJoinCol, rightJoinCol, selects, "");
        }


        SharedTable leftJoin(
            SharedTable::ColRef leftJoinCol,
            SharedTable::ColRef rightJoinCol,
            std::vector<SharedTable::ColRef> selects,
            std::string leftJoinColName)
        {
            return joinImpl(leftJoinCol, rightJoinCol, selects, leftJoinColName);
        }
            

        SharedTable joinImpl(
            SharedTable::ColRef leftJoinCol,
            SharedTable::ColRef rightJoinCol,
            std::vector<SharedTable::ColRef> selects,
            std::string leftJoinColName
        );

        // take all of the left table and any rows from the right table
        // where the rightJoinCol key is not in the left table. The returned
        // table will have the columns specified by leftSelects from the left table
        // and the rightSelects columns in the right table. Not that the data types
        // of leftSelects and rightSelects must match.
        SharedTable rightUnion(
            SharedTable::ColRef leftJoinCol,
            SharedTable::ColRef rightJoinCol,
            std::vector<SharedTable::ColRef> leftSelects,
            std::vector<SharedTable::ColRef> rightSelects);

        void constructOutTable(
            std::vector<SharedTable::ColRef> &circuitInputCols,
            std::vector<SharedTable::ColRef> &circuitOutCols,
            SharedTable &C,
            const SharedTable::ColRef &rightJoinCol,
            const SharedTable::ColRef &leftJoinCol,
            const span<SharedTable::ColRef> select,
            const size_t numRows,
            const bool leftJoin);


        std::array<Matrix<u8>, 3> mapRightTableToLeft(
            aby3::Sh3::i64Matrix& keys,
            span<SharedTable::ColRef> circuitInputCols,
            SharedTable& leftTable,
            SharedTable& rightTable);

        Matrix<u8> cuckooHashRecv(span<SharedTable::ColRef> selects);
        void cuckooHashSend(span<SharedTable::ColRef> selects, CuckooParam& cuckooParams);
        Matrix<u8> cuckooHash(span<SharedTable::ColRef> selects, CuckooParam& params, aby3::Sh3::i64Matrix& keys);


        std::array<Matrix<u8>, 3> selectCuckooPos(MatrixView<u8> cuckooHashTable, u64 destRows);
        std::array<Matrix<u8>, 3> selectCuckooPos(MatrixView<u8> cuckooHashTable, u64 destRows,
            CuckooParam& cuckooParams, aby3::Sh3::i64Matrix& keys);
        void selectCuckooPos(u32 destRows, u32 srcRows, u32 bytes);


        aby3::Sh3::sPackedBin compare(
            SharedTable::ColRef leftJoinCol,
            SharedTable::ColRef rightJoinCol,
            span<Matrix<u8>> leftInData, 
            span<SharedTable::ColRef> outcolumns);

        aby3::Sh3::sPackedBin unionCompare(
            SharedTable::ColRef leftJoinCol,
            SharedTable::ColRef rightJoinCol,
            span<Matrix<u8>> leftInData);

        aby3::Sh3::i64Matrix computeKeys(span<SharedTable::ColRef> tables, span<u64> reveals);


        BetaCircuit getBasicCompareCircuit(
            SharedTable::ColRef leftJoinCol,
            span<SharedTable::ColRef> cols);


        BetaCircuit mLowMCCir;




        void p0CheckSelect(MatrixView<u8> cuckoo, span<Matrix<u8>> a2);
        void p1CheckSelect(Matrix<u8> cuckoo, span<Matrix<u8>> a2, aby3::Sh3::i64Matrix& keys);
    };

}
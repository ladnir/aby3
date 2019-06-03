#pragma once
#include "Sh3Types.h"
#include "Sh3ShareGen.h"
#include "Sh3Runtime.h"
#include "Sh3FixedPoint.h"

namespace aby3
{

    struct TruncationPair
    {
        // the share that should be added before the value being trucnated is revealed.
        i64Matrix mR;

        // the share that thsould be subtracted after the value has been truncated.
        si64Matrix mRTrunc;
    };


    class Sh3Evaluator
    {
    public:

        void init(u64 partyIdx, block prevSeed, block nextSeed, u64 buffSize = 256) { mShareGen.init(prevSeed, nextSeed, buffSize); mPartyIdx = partyIdx; }
        void init(u64 partyIdx, CommPkg& comm, block& seed, u64 buffSize = 256) { mShareGen.init(comm, seed, buffSize); mPartyIdx = partyIdx; }


        void mul(
            CommPkg& comm,
            const si64Matrix & A,
            const si64Matrix & B,
            si64Matrix& C);

        //CompletionHandle asyncMul(
        //    CommPkg& comm,
        //    const si64Matrix& A,
        //    const si64Matrix& B,
        //    si64Matrix& C);

        Sh3Task asyncMul(
            Sh3Task& dependency,
            const si64Matrix& A,
            const si64Matrix& B,
            si64Matrix& C);



        template<Decimal D>
        Sh3Task asyncMul(
            Sh3Task& dependency,
            const sf64<D>& A,
            const sf64<D>& B,
            sf64<D>& C);

		template<Decimal D>
		Sh3Task asyncMul(
            Sh3Task& dependency,
            const sf64Matrix<D>& A,
            const sf64Matrix<D>& B,
            sf64Matrix<D>& C);

        //Matrix mulTruncate(
        //    const Matrix& A,
        //    const Matrix& B,
        //    u64 truncation);

        //Matrix arithmBinMul(
        //    const Matrix& ArithmaticShares,
        //    const Matrix& BinaryShares);

        //Lynx::CompletionHandle asyncArithBinMul(
        //    const Matrix& Arithmatic,
        //    const Matrix& Bin,
        //    Matrix& C);

        //Lynx::CompletionHandle asyncArithBinMul(
        //    const Word& Arithmatic,
        //    const Matrix& Bin,
        //    Matrix& C);

        //Lynx::CompletionHandle asyncConvertToPackedBinary(
        //    const Matrix& arithmetic,
        //    PackedBinMatrix& binaryShares,
        //    CommPackage commPkg);



        TruncationPair getTruncationTuple(u64 xSize, u64 ySize, Decimal d);

        u64 mPartyIdx = -1, mTruncationIdx = 0;
        Sh3ShareGen mShareGen;

    };




}
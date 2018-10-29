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
        Sh3::i64Matrix mLongShare;

        // the share that thsould be subtracted after the value has been truncated.
        Sh3::si64Matrix mShortShare;
    };


    class Sh3Evaluator
    {
    public:

        void init(u64 partyIdx, block prevSeed, block nextSeed, u64 buffSize = 256) { mShareGen.init(prevSeed, nextSeed, buffSize); mPartyIdx = partyIdx; }
        void init(u64 partyIdx, Sh3::CommPkg& comm, block& seed, u64 buffSize = 256) { mShareGen.init(comm, seed, buffSize); mPartyIdx = partyIdx; }


        void mul(
            Sh3::CommPkg& comm,
            const Sh3::si64Matrix & A,
            const Sh3::si64Matrix & B,
            Sh3::si64Matrix& C);

        //Sh3::CompletionHandle asyncMul(
        //    Sh3::CommPkg& comm,
        //    const Sh3::si64Matrix& A,
        //    const Sh3::si64Matrix& B,
        //    Sh3::si64Matrix& C);

        Sh3Task asyncMul(
            Sh3Task& dependency,
            const Sh3::si64Matrix& A,
            const Sh3::si64Matrix& B,
            Sh3::si64Matrix& C);



        template<Sh3::Decimal D>
        Sh3Task asyncMul(
            Sh3Task& dependency,
            const Sh3::sf64<D>& A,
            const Sh3::sf64<D>& B,
            Sh3::sf64<D>& C);

		template<Sh3::Decimal D>
		Sh3Task asyncMul(
            Sh3Task& dependency,
            const Sh3::sf64Matrix<D>& A,
            const Sh3::sf64Matrix<D>& B,
            Sh3::sf64Matrix<D>& C);

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



        TruncationPair getTruncationTuple(u64 xSize, u64 ySize, Sh3::Decimal d);

        u64 mPartyIdx = -1, mTruncationIdx = 0;
        Sh3ShareGen mShareGen;

    };




}
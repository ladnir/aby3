#pragma once
#include "Sh3Defines.h"
#include "Sh3ShareGen.h"
#include "Sh3Runtime.h"

namespace aby3
{




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

        Sh3::CompletionHandle asyncMul(
            Sh3::CommPkg& comm,
            const Sh3::si64Matrix& A,
            const Sh3::si64Matrix& B,
            Sh3::si64Matrix& C);

        Sh3Task asyncMul(
            Sh3Task& dependency,
            const Sh3::si64Matrix& A,
            const Sh3::si64Matrix& B,
            Sh3::si64Matrix& C);

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



        u64 mPartyIdx = -1;
        Sh3ShareGen mShareGen;

    };

}
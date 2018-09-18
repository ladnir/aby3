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
        //Sh3Task asyncMull(
        //    Sh3Task& dependency,
        //    const Sh3::sf64Matrix& A,
        //    const Sh3::sf64Matrix& B,
        //    Sh3::sf64Matrix& C);

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



        TruncationPair getTruncationTuple(u64 numShares, Sh3::Decimal d);

        u64 mPartyIdx = -1;
        Sh3ShareGen mShareGen;

    };



    template<Sh3::Decimal D>
    Sh3Task aby3::Sh3Evaluator::asyncMul(
        Sh3Task & dependency,
        const Sh3::sf64<D>& A,
        const Sh3::sf64<D>& B,
        Sh3::sf64<D>& C)
    {
        return dependency.then([&](Sh3::CommPkg& comm, Sh3Task& self) -> void
        {

            auto truncationTuple = getTruncationTuple(1, C.mDecimal);

            auto abMinusR
                = A.mShare.mData[0] * B.mShare.mData[0]
                + A.mShare.mData[0] * B.mShare.mData[1]
                + A.mShare.mData[1] * B.mShare.mData[0]
                - truncationTuple.mLongShare(0);

            C.mShare = truncationTuple.mShortShare(0);

            auto& rt = self.getRuntime();

            // reveal dependency.getRuntime().the value to party 0, 1
            auto next = (rt.mPartyIdx + 1) % 3;
            auto prev = (rt.mPartyIdx + 2) % 3;
            if (next < 2) comm.mNext.asyncSendCopy(abMinusR);
            if (prev < 2) comm.mPrev.asyncSendCopy(abMinusR);

            if (rt.mPartyIdx < 2)
            {
                // these will hold the three shares of r-xy
                std::unique_ptr<std::array<i64, 3>> shares(new std::array<i64, 3>);

                // perform the async receives
                auto fu0 = comm.mNext.asyncRecv((*shares)[0]).share();
                auto fu1 = comm.mPrev.asyncRecv((*shares)[1]).share();
                (*shares)[2] = abMinusR;

                // set the completion handle complete the computation
                self.nextRound([fu0, fu1, shares = std::move(shares), &C, this]
                (Sh3::CommPkg& comm, Sh3Task self) mutable
                {
                    fu0.get();
                    fu1.get();

                    // r-xy 
                    (*shares)[0] += (*shares)[1] + (*shares)[2];

                    // xy/2^ = (r/2^d) - ((r-xy) / 2^d)
                    C.mShare.mData[mPartyIdx] += (*shares)[0] / (1ull << C.mDecimal);
                });
            }
        });
    }

}
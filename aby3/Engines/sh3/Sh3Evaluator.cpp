#include "Sh3Evaluator.h"


namespace aby3
{


    void Sh3Evaluator::mul(
        Sh3::CommPkg& comm, 
        const Sh3::si64Matrix & A, 
        const Sh3::si64Matrix & B,
        Sh3::si64Matrix& C)
    {
        C.mShares[0]
            = A.mShares[0] * B.mShares[0]
            + A.mShares[0] * B.mShares[1]
            + A.mShares[1] * B.mShares[0];

        for (u64 i = 0; i < C.size(); ++i)
        {
            C.mShares[0](i) += mShareGen.getShare();
        }

        C.mShares[1].resizeLike(C.mShares[0]);

        comm.mNext.asyncSendCopy(C.mShares[0].data(), C.mShares[0].size());
        comm.mPrev.recv(C.mShares[1].data(), C.mShares[1].size());
    }

    //Sh3::CompletionHandle Sh3Evaluator::asyncMul(
    //    Sh3::CommPkg& comm, 
    //    const Sh3::si64Matrix & A, 
    //    const Sh3::si64Matrix & B, 
    //    Sh3::si64Matrix & C)
    //{
    //    C.mShares[0]
    //        = A.mShares[0] * B.mShares[0]
    //        + A.mShares[0] * B.mShares[1]
    //        + A.mShares[1] * B.mShares[0];

    //    for (u64 i = 0; i < C.size(); ++i)
    //    {
    //        C.mShares[0](i) += mShareGen.getShare();
    //    }

    //    C.mShares[1].resizeLike(C.mShares[0]);

    //    comm.mNext.asyncSendCopy(C.mShares[0].data(), C.mShares[0].size());
    //    auto fu = comm.mPrev.asyncRecv(C.mShares[1].data(), C.mShares[1].size()).share();

    //    return { [fu = std::move(fu)](){ fu.get(); } };
    //}

    Sh3Task Sh3Evaluator::asyncMul(Sh3Task & dependency, const Sh3::si64Matrix & A, const Sh3::si64Matrix & B, Sh3::si64Matrix & C)
    {
        return dependency.then([&](Sh3::CommPkg& comm, Sh3Task self) 
        {
            C.mShares[0]
                = A.mShares[0] * B.mShares[0]
                + A.mShares[0] * B.mShares[1]
                + A.mShares[1] * B.mShares[0];

            for (u64 i = 0; i < C.size(); ++i)
            {
                C.mShares[0](i) += mShareGen.getShare();
            }

            C.mShares[1].resizeLike(C.mShares[0]);

            comm.mNext.asyncSendCopy(C.mShares[0].data(), C.mShares[0].size());
            auto fu = comm.mPrev.asyncRecv(C.mShares[1].data(), C.mShares[1].size()).share();

            self.nextRound([fu = std::move(fu)](Sh3::CommPkg& comm, Sh3Task& self){
                fu.get(); 
            });
        });
    }

    TruncationPair Sh3Evaluator::getTruncationTuple(u64 numShares, Sh3::Decimal d)
    {
        TruncationPair r;
        
        r.mLongShare.resize(numShares, 1);
        r.mLongShare.setZero();

        r.mShortShare.resize(numShares, 1);
        r.mShortShare.mShares[0].setZero();
        r.mShortShare.mShares[1].setZero();

        return r;
    }



//#define INSTANTIATE_ASYNC_MUL_FIXED(D) \
//    template \
//    Sh3Task aby3::Sh3Evaluator::asyncMul( \
//        Sh3Task & dependency, \
//        const Sh3::sf64<D>& A,\
//        const Sh3::sf64<D>& B,\
//        Sh3::sf64<D>& C);
//
//    INSTANTIATE_ASYNC_MUL_FIXED(Sh3::Decimal::D8);
//    INSTANTIATE_ASYNC_MUL_FIXED(Sh3::Decimal::D16);
//    INSTANTIATE_ASYNC_MUL_FIXED(Sh3::Decimal::D32);
}
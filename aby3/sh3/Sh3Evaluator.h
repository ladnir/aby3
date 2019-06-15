#pragma once
#include "Sh3Types.h"
#include "Sh3ShareGen.h"
#include "Sh3Runtime.h"
#include "Sh3FixedPoint.h"
#include "aby3/OT/SharedOT.h"

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

		void init(u64 partyIdx, block prevSeed, block nextSeed, u64 buffSize = 256);
		void init(u64 partyIdx, CommPkg& comm, block seed, u64 buffSize = 256);

		bool DEBUG_disable_randomization = false;

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
            Sh3Task dependency,
            const si64Matrix& A,
            const si64Matrix& B,
            si64Matrix& C);


		Sh3Task asyncMul(
			Sh3Task dependency,
			const si64Matrix& A,
			const si64Matrix& B,
			si64Matrix& C,
			u64 shift);


		Sh3Task asyncMul(
			Sh3Task dependency,
			const si64& A,
			const si64& B,
			si64& C,
			u64 shift);

        template<Decimal D>
		Sh3Task asyncMul(
			Sh3Task dependency,
			const sf64<D>& A,
			const sf64<D>& B,
			sf64<D>& C)
		{
			return asyncMul(dependency, A.i64Cast(), B.i64Cast(), C.i64Cast(), D);
		}

		template<Decimal D>
		Sh3Task asyncMul(
			Sh3Task dependency,
			const sf64Matrix<D>& A,
			const sf64Matrix<D>& B,
			sf64Matrix<D>& C,
			u64 shift)
		{
			return asyncMul(dependency, A.i64Cast(), B.i64Cast(), C.i64Cast(), D + shift);
		}

		template<Decimal D>
		Sh3Task asyncMul(
			Sh3Task dependency,
			const sf64Matrix<D>& A,
			const sf64Matrix<D>& B,
			sf64Matrix<D>& C)
		{
			return asyncMul(dependency, A.i64Cast(), B.i64Cast(), C.i64Cast(), D);
		}


		Sh3Task asyncMul(
			Sh3Task dep,
			const si64Matrix& A,
			const sbMatrix& B,
			si64Matrix& C);

		Sh3Task asyncMul(
			Sh3Task dep,
			const i64& a,
			const sbMatrix& B,
			si64Matrix& C);

        TruncationPair getTruncationTuple(u64 xSize, u64 ySize, u64 d);

        u64 mPartyIdx = -1, mTruncationIdx = 0;
        Sh3ShareGen mShareGen;
		SharedOT mOtPrev, mOtNext;
    };




}
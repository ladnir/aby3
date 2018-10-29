#include "Sh3Evaluator.h"
#include <cryptoTools/Crypto/PRNG.h>
#include <iomanip>
#include <cryptoTools/Common/Log.h>

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

	//std::string prettyShare(int partyIdx, i64 v0, i64 v1 = -1, i64 v2 = -1)
	//{
	//	std::array<u64, 3> shares;
	//	shares[partyIdx] = v0;
	//	shares[(partyIdx + 2) % 3] = v1;
	//	shares[(partyIdx + 1) % 3] = v2;

	//	std::stringstream ss;
	//	ss << "(";
	//	if (shares[0] == -1) ss << "               _ ";
	//	else ss << std::hex << std::setw(16) << std::setfill('0') << shares[0] << " ";
	//	if (shares[1] == -1) ss << "               _ ";
	//	else ss << std::hex << std::setw(16) << std::setfill('0') << shares[1] << " ";
	//	if (shares[2] == -1) ss << "               _)";
	//	else ss << std::hex << std::setw(16) << std::setfill('0') << shares[2] << ")";

	//	return ss.str();
	//}

	TruncationPair Sh3Evaluator::getTruncationTuple(u64 xSize, u64 ySize, Sh3::Decimal d)
	{
		TruncationPair r;

		//r.mLongShare.setZero();

		//r.mShortShare.resize(numShares, 1);

		oc::PRNG prng(oc::toBlock(mTruncationIdx));
		std::array<Sh3::i64Matrix, 3> shares;
		shares[0].resize(xSize, ySize);
		shares[1].resize(xSize, ySize);

		r.mLongShare.resize(xSize, ySize);

		prng.get(r.mLongShare.data(), xSize* ySize); // r
		//r.mLongShare(0) >>= 2;
		//r.mLongShare.setZero();
		//r.mLongShare(0) = -5 << d;
		//r.mLongShare(0) &= (u64(-1) >> 34);

		//if (1)
		{
			prng.get(shares[0].data(), xSize* ySize); // share 0
			prng.get(shares[1].data(), xSize* ySize); // share 1
			shares[2] = -shares[0] - shares[1];// share 2 = -share 0 -share 1
		}
		//else
		//{
		//	shares[0].setZero();
		//	shares[1].setZero();

		//	shares[2].resize(numShares, 1);
		//	shares[2].setZero();
		//}

		//shares[0] += r.mLongShare >> d; // share0 += r >> d

		for (u64 i = 0; i < r.mLongShare.size(); ++i)
			shares[0](i) += r.mLongShare(i) >> d;

		//auto t = r.mLongShare;

		if (mPartyIdx) r.mLongShare.setZero();

		//if (t / (1ull << d) != shares[0] + shares[1] + shares[2])
		//	throw RTE_LOC;
		//

		if (mPartyIdx == 0)
		{
			r.mShortShare.mShares[0] = std::move(shares[0]);
			r.mShortShare.mShares[1] = std::move(shares[2]);
		}
		else if (mPartyIdx == 1)
		{
			r.mShortShare.mShares[0] = std::move(shares[1]);
			r.mShortShare.mShares[1] = std::move(shares[0]);
		}
		else
		{
			r.mShortShare.mShares[0] = std::move(shares[2]);
			r.mShortShare.mShares[1] = std::move(shares[1]);

		}

		//oc::ostreamLock(std::cout)<< "t"<< mPartyIdx<< " "  << prettyShare(mPartyIdx, r.mShortShare.mShares[0](0), r.mShortShare.mShares[1](0)) << std::endl;

		//r.mShortShare
		//r.mShortShare.mShares[0].setZero();
		//r.mShortShare.mShares[1].setZero();


		return r;
	}
	 
	template<Sh3::Decimal D>
	Sh3Task aby3::Sh3Evaluator::asyncMul(
		Sh3Task & dependency,
		const Sh3::sf64<D>& A,
		const Sh3::sf64<D>& B,
		Sh3::sf64<D>& C)
	{
		return dependency.then([&](Sh3::CommPkg& comm, Sh3Task& self) -> void
		{

			auto truncationTuple = getTruncationTuple(1,1, C.mDecimal);

			auto abMinusR
				= A.mShare.mData[0] * B.mShare.mData[0]
				+ A.mShare.mData[0] * B.mShare.mData[1]
				+ A.mShare.mData[1] * B.mShare.mData[0]
				+ mShareGen.getShare();


			//oc::ostreamLock(std::cout) << "ab " << mPartyIdx << ": " << abMinusR << " - "<< truncationTuple.mLongShare(0) << 
			//	" = " << abMinusR - truncationTuple.mLongShare(0) << std::endl;

			abMinusR -= truncationTuple.mLongShare(0);
			C.mShare = truncationTuple.mShortShare(0);

			//{
			//	comm.mPrev.asyncSend(truncationTuple.mLongShare(0));
			//	comm.mNext.asyncSend(truncationTuple.mLongShare(0));
			//	i64 l1, l2;
			//	comm.mPrev.recv(l1);
			//	comm.mNext.recv(l2);
			//	auto l = truncationTuple.mLongShare(0) + l1 + l2;
			//	auto ls = (l / (1ll << C.mDecimal));
			//	comm.mPrev.asyncSend(truncationTuple.mShortShare.mShares[0](0));
			//	i64 s2;
			//	comm.mNext.recv(s2);
			//	auto s = s2 + truncationTuple.mShortShare.mShares[0](0) + truncationTuple.mShortShare.mShares[1](0);
			//	oc::ostreamLock(std::cout) << "sss " << l << " " << s << " " << ls << " " << s - ls << std::endl;
			//}

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

					// xy-r
					(*shares)[0] += (*shares)[1] + (*shares)[2];


					// xy/2^d = (r/2^d) + ((xy-r) / 2^d)
					C.mShare.mData[mPartyIdx] += (*shares)[0] >> C.mDecimal;
				});
			}
		});
	}

	template
		Sh3Task aby3::Sh3Evaluator::asyncMul(
			Sh3Task & dependency,
			const Sh3::sf64<Sh3::Decimal::D8>& A,
			const Sh3::sf64<Sh3::Decimal::D8>& B,
			Sh3::sf64<Sh3::Decimal::D8>& C);
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




	template<Sh3::Decimal D>
	Sh3Task aby3::Sh3Evaluator::asyncMul(
		Sh3Task & dependency,
		const Sh3::sf64Matrix<D>& A,
		const Sh3::sf64Matrix<D>& B,
		Sh3::sf64Matrix<D>& C)
	{
		return dependency.then([&](Sh3::CommPkg& comm, Sh3Task& self) -> void
		{
			
			auto abMinusR
				=( A.mShares[0] * B.mShares[0]
				+ A.mShares[0] * B.mShares[1]
				+ A.mShares[1] * B.mShares[0]).eval();

			for (u64 i = 0; i < abMinusR.size(); ++i)
				abMinusR(i) += mShareGen.getShare();

			auto truncationTuple = getTruncationTuple(abMinusR.rows(), abMinusR.cols(), C.mDecimal);
			abMinusR -= truncationTuple.mLongShare;
			C.mShares = std::move(truncationTuple.mShortShare.mShares);

			auto& rt = self.getRuntime();

			// reveal dependency.getRuntime().the value to party 0, 1
			auto next = (rt.mPartyIdx + 1) % 3;
			auto prev = (rt.mPartyIdx + 2) % 3;
			if (next < 2) comm.mNext.asyncSendCopy(abMinusR.data(), abMinusR.size());
			if (prev < 2) comm.mPrev.asyncSendCopy(abMinusR.data(), abMinusR.size());

			if (rt.mPartyIdx < 2)
			{
				// these will hold the three shares of r-xy
				//std::unique_ptr<std::array<i64, 3>> shares(new std::array<i64, 3>);
				auto shares = std::make_unique<std::array<Sh3::i64Matrix, 3>>();
				
				//Sh3::i64Matrix& rr = (*shares)[0]);

				(*shares)[0].resize(abMinusR.rows(), abMinusR.cols());
				(*shares)[1].resize(abMinusR.rows(), abMinusR.cols());

				// perform the async receives
				auto fu0 = comm.mNext.asyncRecv((*shares)[0].data(), (*shares)[0].size()).share();
				auto fu1 = comm.mPrev.asyncRecv((*shares)[1].data(), (*shares)[1].size()).share();
				(*shares)[2] = std::move(abMinusR);

				// set the completion handle complete the computation
				self.nextRound([fu0, fu1, shares = std::move(shares), &C, this]
				(Sh3::CommPkg& comm, Sh3Task self) mutable
				{
					fu0.get();
					fu1.get();

					// xy-r
					(*shares)[0] += (*shares)[1] + (*shares)[2];

					// xy/2^d = (r/2^d) + ((xy-r) / 2^d)
					auto& v = C.mShares[mPartyIdx];
					auto& s = (*shares)[0];
					for(u64 i =0; i < v.size(); ++i)
						v(i) += s(i) >> C.mDecimal;
				});
			}
		});
	}




	template
		Sh3Task aby3::Sh3Evaluator::asyncMul(
			Sh3Task & dependency,
			const Sh3::sf64Matrix<Sh3::Decimal::D8>& A,
			const Sh3::sf64Matrix<Sh3::Decimal::D8>& B,
			Sh3::sf64Matrix<Sh3::Decimal::D8>& C);
}
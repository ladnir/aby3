#include "Sh3Evaluator.h"
#include <cryptoTools/Crypto/PRNG.h>
#include <iomanip>
#include <cryptoTools/Common/Log.h>
#include <cassert>
using namespace oc;
namespace aby3
{
	void Sh3Evaluator::init(u64 partyIdx, block prevSeed, block nextSeed, u64 buffSize)
	{
		mShareGen.init(prevSeed, nextSeed, buffSize);
		mPartyIdx = partyIdx;
		mOtPrevRecver.setSeed(mShareGen.mNextCommon.get<block>());
		mOtNextRecver.setSeed(mShareGen.mPrevCommon.get<block>());
	}

	void Sh3Evaluator::init(u64 partyIdx, CommPkg& comm, block seed, u64 buffSize)
	{
		mShareGen.init(comm, seed, buffSize);
		mPartyIdx = partyIdx;
		mOtPrevRecver.setSeed(mShareGen.mNextCommon.get<block>());
		mOtNextRecver.setSeed(mShareGen.mPrevCommon.get<block>());
	}

	//void Sh3Evaluator::mul(
	//	CommPkg& comm,
	//	const si64Matrix& A,
	//	const si64Matrix& B,
	//	si64Matrix& C)
	//{
	//	C.mShares[0]
	//		= A.mShares[0] * B.mShares[0]
	//		+ A.mShares[0] * B.mShares[1]
	//		+ A.mShares[1] * B.mShares[0];

	//	for (u64 i = 0; i < C.size(); ++i)
	//	{
	//		C.mShares[0](i) += mShareGen.getShare();
	//	}

	//	C.mShares[1].resizeLike(C.mShares[0]);

	//	comm.mNext.asyncSendCopy(C.mShares[0].data(), C.mShares[0].size());
	//	comm.mPrev.recv(C.mShares[1].data(), C.mShares[1].size());
	//}

	//CompletionHandle Sh3Evaluator::asyncMul(
	//    CommPkg& comm, 
	//    const si64Matrix & A, 
	//    const si64Matrix & B, 
	//    si64Matrix & C)
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


	Sh3Task Sh3Evaluator::asyncMul(Sh3Task dependency, const si64& A, const si64& B, si64& C)
	{
		return dependency.then([&](CommPkg& comm, Sh3Task self)
			{
				C[0]
					= A[0] * B[0]
					+ A[0] * B[1]
					+ A[1] * B[0]
					+ mShareGen.getShare();

				comm.mNext.asyncSendCopy(C[0]);
				auto fu = comm.mPrev.asyncRecv(C[1]).share();

				self.then([fu = std::move(fu)](CommPkg& comm, Sh3Task& self){
					fu.get();
				});
			}).getClosure();
	}


	Sh3Task Sh3Evaluator::asyncMul(Sh3Task dependency, const si64Matrix& A, const si64Matrix& B, si64Matrix& C)
	{
		return dependency.then([&](CommPkg& comm, Sh3Task self)
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

				self.then([fu = std::move(fu)](CommPkg& comm, Sh3Task& self){
					fu.get();
				});
			}).getClosure();
	}


	Sh3Task Sh3Evaluator::asyncMul(
		Sh3Task dep,
		const si64Matrix& A,
		const sbMatrix& B,
		si64Matrix& c)
	{
		return dep.then([&](CommPkg& comm, Sh3Task self) {
			c.resize(A.rows(), A.cols());
			assert(B.rows() == A.rows());
			assert(A.cols() == 1);
			assert(B.bitCount() == 1);

			switch (mPartyIdx)
			{
			case 0:
			{
				// this party will sends s0 which contains 
				//     m = b * (a0 + a2) - c0 - c2 - z
				std::vector<std::array<i64, 2>> s0(c.size());

				// We will use this as OT helper, will have share b0
				oc::BitVector b1(c.size());
				for (u64 i = 0; i < s0.size(); ++i)
				{
					// b0 + b2
					auto bb0 = B[0](i) ^ B[1](i);

					// b0
					auto bb1 = B[0](i);
					assert(bb0 == 0 || bb0 == 1);
					assert(bb1 == 0 || bb1 == 1);

					auto z = mShareGen.mPrevCommon.get<i64>();
					c[0](i) = mShareGen.mNextCommon.get<i64>();
					c[1](i) = mShareGen.mPrevCommon.get<i64>();

					b1[i] = bb1;

					z = -(c[0](i) + c[1](i)) - z;
					s0[i][bb0] = z;
					s0[i][bb0 ^ 1] = (A[0](i) + A[1](i)) + z;
				}

				// We are OT sender, p1 is receiver, p2 is helper.
				// receiver will obtain m = b * (a0 + a2) - c0 - c2 - z
				mOtNextRecver.send(comm.mNext, s0);

				// We are OT helper, p1 is receiver, p2 is sender.
				// receiver will obtain m = b * a1 + z
				mOtNextRecver.help(comm.mNext, b1);
				break;
			}
			case 1:
			{
				// receiver.
				oc::BitVector b0(c.size());
				oc::BitVector b1(c.size());

				for (u64 i = 0; i < b0.size(); ++i)
				{
					// b1
					auto bb0 = B[0](i);

					// b0
					auto bb1 = B[1](i);

					assert(bb0 == 0 || bb0 == 1);
					assert(bb1 == 0 || bb1 == 1);

					c[1](i) = mShareGen.mPrevCommon.get<i64>();

					b0[i] = bb0;
					b1[i] = bb1;
				}

				std::vector<i64> recv0(b0.size());
				oc::span<i64> recv1(c[0].data(), c[0].size());

				// obtain m = b * (a0 + a2) - c0 - c2 - z
				auto f0 = SharedOT::asyncRecv(comm.mPrev, comm.mNext, std::move(b0), recv0);

				// obtain m = b * a1 + z
				auto f1 = SharedOT::asyncRecv(comm.mNext, comm.mPrev, std::move(b1), recv1);

				self.then([&, recv0 = std::move(recv0), f0, f1](CommPkg& comm, Sh3Task self){
					f0.get();
					f1.get();
					for (u64 i = 0; i < recv0.size(); ++i)
						c[0](i) += recv0[i];
					comm.mNext.asyncSendCopy(c[0].data(), c[0].size());
				});
				break;
			}
			case 2:
			{
				// sender.
				oc::BitVector b0(c.size());

				// this party will sends s0 which contains 
				//     m = b * a1 + z
				std::vector<std::array<i64, 2>> s1(c.size());

				for (u64 i = 0; i < b0.size(); ++i)
				{
					// b1
					auto bb0 = B[1](i);

					// b1 + b2
					auto bb1 = B[0](i) ^ B[1](i);

					i64 a1 = A[1](i);

					assert(bb0 == 0 || bb0 == 1);
					assert(bb1 == 0 || bb1 == 1);

					auto z = mShareGen.mNextCommon.get<i64>();
					c[0](i) = mShareGen.mNextCommon.get<i64>();

					b0[i] = bb0;
					s1[i][bb1] = z;
					s1[i][bb1 ^ 1] = a1 + z;
				}

				// We are OT helper, p1 is receiver, p0 is sender.
				// receiver will obtain m = b * (a0 + a2) - c0 - c2 - z
				mOtPrevRecver.help(comm.mPrev, b0);

				// We are OT sender, p1 is receiver, p0 is helper.
				// receiver will obtain m = b * a1 + z
				mOtPrevRecver.send(comm.mPrev, s1);


				self.then([&](CommPkg& comm, Sh3Task self) {
					auto f = comm.mPrev.asyncRecv(c[1].data(), c[1].size()).share();
					self.then([&, f](CommPkg& comm, Sh3Task self) {
						f.get();
						});
					});
				break;
			}
			default:
				throw RTE_LOC;
			}
			}).getClosure();
	}

	//Sh3Task Sh3Evaluator::asyncMul(
	//	Sh3Task dep,
	//	const si64Matrix& a,
	//	const sbMatrix& b,
	//	si64Matrix& c)
	//{
	//	return dep.then([&](CommPkg& comm, Sh3Task self) {
	//		switch (mPartyIdx)
	//		{
	//		case 0:
	//		{
	//			std::vector<std::array<i64, 2>> s0(a.size());
	//			BitVector c1(a.size());
	//			for (u64 i = 0; i < s0.size(); ++i)
	//			{
	//				auto bb = b.mShares[0](i) ^ b.mShares[1](i);
	//				if (bb < 0 || bb > 1)
	//					throw std::runtime_error(LOCATION);

	//				auto zeroShare = mShareGen.getShare();

	//				s0[i][bb] = zeroShare;
	//				s0[i][bb ^ 1] = a.mShares[1](i) + zeroShare;

	//				//std::cout << "b=(" << b.mShares[0](i) << ",  , " << b.mShares[1](i) << ")" << std::endl;
	//				//std::cout << "s0[" << i << "] = " << bb * a.mShares[1](i) << std::endl;
	//				assert(b.mShares[1](i) < 2);
	//				c1[i] = static_cast<u8>(b.mShares[1](i));
	//			}
	//			std::cout << "p0 send_0 " << s0[0][0] << " " << s0[0][1] << " help_1 " << c1 << std::endl;
	//			// share 0: from p0 to p1,p2
	//			mOtNextRecver.send(comm.mNext, s0);
	//			mOtPrevRecver.send(comm.mPrev, s0);

	//			// share 1: from p1 to p0,p2 
	//			mOtPrevRecver.help(comm.mPrev, c1);



	//			auto fu1 = comm.mPrev.asyncRecv(c.mShares[0].data(), c.size()).share();
	//			oc::span<i64> dd(c.mShares[1].data(), c.mShares[1].size());
	//			auto fu2 = SharedOT::asyncRecv(comm.mNext, comm.mPrev, std::move(c1), dd).share();

	//			self.then([
	//				fu1 = std::move(fu1),
	//					fu2 = std::move(fu2)]
	//					(CommPkg& comm, Sh3Task self) mutable {
	//					fu1.get();
	//					fu2.get();
	//				});
	//			break;
	//		}
	//		case 1:
	//		{
	//			std::vector<std::array<i64, 2>> s1(a.size());
	//			BitVector c0(a.size());
	//			for (u64 i = 0; i < s1.size(); ++i)
	//			{
	//				auto bb = b.mShares[0](i) ^ b.mShares[1](i);
	//				if (bb < 0 || bb > 1)
	//					throw std::runtime_error(LOCATION);
	//				auto ss = mShareGen.getShare();

	//				s1[i][bb] = ss;
	//				s1[i][bb ^ 1] = (a.mShares[0](i) + a.mShares[1](i)) + ss;

	//				//std::cout << "b=(   ," << b.mShares[0](i) << ",   )" << "  " << (a.mShares[0](i) + a.mShares[1](i)) << std::endl;
	//				//std::cout << "s1[" << i << "] = " << bb * (a.mShares[0](i) + a.mShares[1](i)) << " = b *  (" <<a.mShares[0](i) <<" +  "<<a.mShares[1](i) <<")" << std::endl;

	//				assert(b.mShares[0](i) < 2);
	//				c0[i] = static_cast<u8>(b.mShares[0](i));
	//			}

	//			std::cout << "p1 send_1 " << s1[0][0] << " " << s1[0][1] << " help_0 " << c0 << std::endl;


	//			// share 0: from p0 to p1,p2
	//			mOtNextRecver.help(comm.mNext, c0);

	//			// share 1: from p1 to p0,p2 
	//			mOtNextRecver.send(comm.mNext, s1);
	//			mOtPrevRecver.send(comm.mPrev, s1);


	//			// share 0: from p0 to p1,p2
	//			oc::span<i64> dd(c.mShares[0].data(), c.size());
	//			auto fu1 = SharedOT::asyncRecv(comm.mPrev, comm.mNext, std::move(c0), dd).share();

	//			// share 1:
	//			auto fu2 = comm.mNext.asyncRecv(c.mShares[1].data(), c.size()).share();

	//			self.then([
	//				fu1 = std::move(fu1),
	//					fu2 = std::move(fu2),
	//					&c,
	//					_2 = std::move(c0)]
	//					(CommPkg& comm, Sh3Task self) mutable {
	//					fu1.get();
	//					fu2.get();
	//					//std::cout << "P1.get() " << c.mShares[0](0) << " " << c.mShares[1](0) << std::endl;
	//				});

	//			break;
	//		}
	//		case 2:
	//		{
	//			BitVector c0(a.size()), c1(a.size());
	//			std::vector<i64> s0(a.size()), s1(a.size());
	//			for (u64 i = 0; i < a.size(); ++i)
	//			{
	//				c0[i] = static_cast<u8>(b.mShares[1](i));
	//				c1[i] = static_cast<u8>(b.mShares[0](i));
	//				assert(b.mShares[0](i) < 2);
	//				assert(b.mShares[1](i) < 2);

	//				s0[i] = s1[i] = mShareGen.getShare();
	//			}

	//			// share 0: from p0 to p1,p2
	//			mOtPrevRecver.help(comm.mPrev, c0);
	//			comm.mNext.asyncSend(std::move(s0));

	//			// share 1: from p1 to p0,p2 
	//			mOtNextRecver.help(comm.mNext, c1);
	//			comm.mPrev.asyncSend(std::move(s1));

	//			// share 0: from p0 to p1,p2
	//			oc::span<i64> dd0(c.mShares[0].data(), c.size());
	//			auto fu1 = SharedOT::asyncRecv(comm.mNext, comm.mPrev, std::move(c0), dd0).share();

	//			// share 1: from p1 to p0,p2
	//			oc::span<i64> dd1(c.mShares[1].data(), c.size());
	//			auto fu2 = SharedOT::asyncRecv(comm.mPrev, comm.mNext, std::move(c1), dd1).share();

	//			self.then([
	//				fu1 = std::move(fu1),
	//					fu2 = std::move(fu2),
	//					&c]
	//					(CommPkg& comm, Sh3Task self) mutable {
	//					fu1.get();
	//					fu2.get();
	//					//std::cout << "P1.get() " << c.mShares[0](0) << " " << c.mShares[1](0) << std::endl;
	//				});
	//			break;
	//		}
	//		default:
	//			throw std::runtime_error(LOCATION);
	//		}
	//		}
	//	).getClosure();
	//}


	Sh3Task Sh3Evaluator::asyncMul(
		Sh3Task dep,
		const i64& a,
		const sbMatrix& b,
		si64Matrix& c)
	{
		return dep.then([&](CommPkg& comm, Sh3Task self) {
			if (b.bitCount() != 1)
				throw RTE_LOC;

			switch (mPartyIdx)
			{
			case 0:
			{
				std::vector<std::array<i64, 2>> s0(b.rows());
				for (u64 i = 0; i < s0.size(); ++i)
				{
					auto bb = b.mShares[0](i) ^ b.mShares[1](i);
					auto zeroShare = mShareGen.getShare();

					s0[i][bb] = zeroShare;
					s0[i][bb ^ 1] = a + zeroShare;
				}

				// share 0: from p0 to p1,p2
				mOtNextRecver.send(comm.mNext, s0);
				mOtPrevRecver.send(comm.mPrev, s0);

				auto fu1 = comm.mNext.asyncRecv(c.mShares[0].data(), c.size()).share();
				auto fu2 = comm.mPrev.asyncRecv(c.mShares[1].data(), c.size()).share();
				self.then([fu1 = std::move(fu1), fu2 = std::move(fu2)](CommPkg& _, Sh3Task __) mutable {
					fu1.get();
					fu2.get();
				});
				break;
			}
			case 1:
			{
				BitVector c0(b.rows());
				for (u64 i = 0; i < b.rows(); ++i)
				{
					c.mShares[1](i) = mShareGen.getShare();
					c0[i] = static_cast<u8>(b.mShares[0](i));
				}

				// share 0: from p0 to p1,p2
				mOtNextRecver.help(comm.mNext, c0);
				comm.mPrev.asyncSendCopy(c.mShares[1].data(), c.size());

				i64* dd = c.mShares[0].data();
				auto fu1 = SharedOT::asyncRecv(comm.mPrev, comm.mNext, std::move(c0), { dd, u64(c.size()) });
				self.then([fu1 = std::move(fu1)](CommPkg& _, Sh3Task __) mutable {
					fu1.get();
				});

				break;
			}
			case 2:
			{
				BitVector c0(b.rows());
				for (u64 i = 0; i < b.rows(); ++i)
				{
					c.mShares[0](i) = mShareGen.getShare();
					c0[i] = static_cast<u8>(b.mShares[1](i));
				}

				// share 0: from p0 to p1,p2
				mOtPrevRecver.help(comm.mPrev, c0);
				comm.mNext.asyncSendCopy(c.mShares[0].data(), c.size());

				i64* dd0 = c.mShares[1].data();
				auto fu1 = SharedOT::asyncRecv(comm.mNext, comm.mPrev, std::move(c0), { dd0, u64(c.size()) });

				self.then([fu1 = std::move(fu1)](CommPkg& _, Sh3Task __) mutable {
					fu1.get();
				});
				break;
			}
			default:
				throw std::runtime_error(LOCATION);
			}
			}
		).getClosure();
	}

	TruncationPair Sh3Evaluator::getTruncationTuple(u64 xSize, u64 ySize, u64 d)
	{
		TruncationPair pair;
		if (DEBUG_disable_randomization)
		{
			pair.mR.resize(xSize, ySize);
			pair.mR.setZero();

			pair.mRTrunc.mShares[0].resize(xSize, ySize);
			pair.mRTrunc.mShares[1].resize(xSize, ySize);
			pair.mRTrunc.mShares[0].setZero();
			pair.mRTrunc.mShares[1].setZero();

		}
		else
		{
			const auto d2 = d + 2;
			pair.mR.resize(xSize, ySize);
			pair.mRTrunc.resize(xSize, ySize);
			//const u64 mask = (~0ull) >> 1;
			//if (mPartyIdx == 0)
			{
				//mShareGen.mPrevCommon.get(pair.mR.data(), pair.mR.size());
				mShareGen.mNextCommon.get(pair.mRTrunc[0].data(), pair.mRTrunc[0].size());
				mShareGen.mPrevCommon.get(pair.mRTrunc[1].data(), pair.mRTrunc[1].size());
				for (u64 i = 0; i < (u64)pair.mR.size(); ++i)
				{
					auto& t0 = pair.mRTrunc[0](i);
					auto& t1 = pair.mRTrunc[1](i);
					auto& r0 = pair.mR(i);

					r0 = t0 >> 2;
					t0 >>= d2;
					t1 >>= d2;
				}
			}
			//else if (mPartyIdx == 1)
			//{
			//    mShareGen.mNextCommon.get(pair.mR.data(), pair.mR.size());
			//    mShareGen.mNextCommon.get(pair.mRTrunc[0].data(), pair.mRTrunc[0].size());
			//    pair.mRTrunc[1].setZero();
			//}
			//else
			//{
			//    i64Matrix R1(xSize, ySize);
			//    mShareGen.mNextCommon.get(pair.mR.data(), pair.mR.size());
			//    mShareGen.mPrevCommon.get(R1.data(), R1.size());

			//    mShareGen.mNextCommon.get(pair.mRTrunc[0].data(), pair.mRTrunc[0].size());
			//    mShareGen.mPrevCommon.get(pair.mRTrunc[1].data(), pair.mRTrunc[1].size());


			//    for (u64 i = 0; i < pair.mR.size(); i++)
			//    {
			//        auto rt = pair.mRTrunc[0](i) + pair.mRTrunc[1](i);


			//        pair.mR(i) = (-pair.mR(i) - R1(i))
			//         +(());
			//    }
			//}
		}
		return pair;
	}

	Sh3Task aby3::Sh3Evaluator::asyncMul(
		Sh3Task  dependency,
		const si64& A,
		const si64& B,
		si64& C,
		u64 shift)
	{
		return dependency.then([&, shift](CommPkg& comm, Sh3Task& self) -> void {

			auto truncationTuple = getTruncationTuple(1, 1, shift);

			auto abMinusR
				= A.mData[0] * B.mData[0]
				+ A.mData[0] * B.mData[1]
				+ A.mData[1] * B.mData[0];
			//+ mShareGen.getShare();


		//oc::ostreamLock(std::cout) << "ab " << mPartyIdx << ": " << abMinusR << " - "<< truncationTuple.mR(0) << 
		//	" = " << abMinusR - truncationTuple.mR(0) << std::endl;

			abMinusR -= truncationTuple.mR(0);
			C = truncationTuple.mRTrunc(0);


			//lout << mPartyIdx << " " << abMinusR << std::endl;
			//{
			//	comm.mPrev.asyncSend(truncationTuple.mR(0));
			//	comm.mNext.asyncSend(truncationTuple.mR(0));
			//	i64 l1, l2;
			//	comm.mPrev.recv(l1);
			//	comm.mNext.recv(l2);
			//	auto l = truncationTuple.mR(0) + l1 + l2;
			//	auto ls = (l / (1ll << C.mDecimal));
			//	comm.mPrev.asyncSend(truncationTuple.mRTrunc.mShares[0](0));
			//	i64 s2;
			//	comm.mNext.recv(s2);
			//	auto s = s2 + truncationTuple.mRTrunc.mShares[0](0) + truncationTuple.mRTrunc.mShares[1](0);
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
				self.then([fu0, fu1, shares = std::move(shares), &C, shift, this]
				(CommPkg& comm, Sh3Task self) mutable
				{
					fu0.get();
					fu1.get();

					// xy-r
					(*shares)[0] += (*shares)[1] + (*shares)[2];


					// xy/2^d = (r/2^d) + ((xy-r) / 2^d)
					C.mData[mPartyIdx] += (*shares)[0] >> shift;

					//lout << mPartyIdx << " " << C.mShare.mData[mPartyIdx] << std::endl
					//	<<  "* " << C.mShare.mData[1^mPartyIdx] << std::endl;
				});
			}

			}).getClosure();
	}



	Sh3Task aby3::Sh3Evaluator::asyncMul(
		Sh3Task dependency,
		const si64Matrix& A,
		const si64Matrix& B,
		si64Matrix& C,
		u64 shift)
	{
		return dependency.then([&, shift](CommPkg& comm, Sh3Task& self) -> void {

			//oc::lout << self.mRuntime->mPartyIdx << " mult Send" << std::endl;

			i64Matrix abMinusR
				= A.mShares[0] * B.mShares[0]
				+ A.mShares[0] * B.mShares[1]
				+ A.mShares[1] * B.mShares[0];

			auto truncationTuple = getTruncationTuple(abMinusR.rows(), abMinusR.cols(), shift);

			abMinusR -= truncationTuple.mR;
			C.mShares = std::move(truncationTuple.mRTrunc.mShares);

			//lout << "p" << mPartyIdx << " ab \n" << abMinusR << std::endl;
				//<< "p"<< mPartyIdx << " c \n" << C.mShares[0]<<",\n"<< C.mShares[1] << std::endl;

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
				auto shares = std::make_unique<std::array<i64Matrix, 3>>();

				//i64Matrix& rr = (*shares)[0]);

				(*shares)[0].resize(abMinusR.rows(), abMinusR.cols());
				(*shares)[1].resize(abMinusR.rows(), abMinusR.cols());

				// perform the async receives
				auto fu0 = comm.mNext.asyncRecv((*shares)[0].data(), (*shares)[0].size()).share();
				auto fu1 = comm.mPrev.asyncRecv((*shares)[1].data(), (*shares)[1].size()).share();
				(*shares)[2] = std::move(abMinusR);

				// set the completion handle complete the computation
				self.then([fu0, fu1, shares = std::move(shares), &C, shift, this]
				(CommPkg& comm, Sh3Task self) mutable
				{
					//oc::lout << self.mRuntime->mPartyIdx << " mult recv" << std::endl;
					fu0.get();
					fu1.get();

					//lout << "p" << mPartyIdx << " ab \n" << (*shares)[0] << ",\n" << (*shares)[1] << ",\n" << (*shares)[2] << std::endl;
					// xy-r
					(*shares)[0] += (*shares)[1] + (*shares)[2];

					// xy/2^d = (r/2^d) + ((xy-r) / 2^d)
					auto& v = C.mShares[mPartyIdx];
					auto& s = (*shares)[0];
					for (u64 i = 0; i < (u64)v.size(); ++i)
						v(i) += s(i) >> shift;


					//lout <<"p" << mPartyIdx << "\n" << v <<",\n" 						<<  C.mShares[1 ^ mPartyIdx] << std::endl;

				});
			}
			//else
			//{
			//	lout << "p " << mPartyIdx << "\n" << C.mShares[0] << ",\n" << C.mShares[1] << std::endl;
			//}
			}).getClosure();
	}

}
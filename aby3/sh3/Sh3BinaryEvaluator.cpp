

#define _ENABLE_EXTENDED_ALIGNED_STORAGE

#include "Sh3BinaryEvaluator.h"
#include <cryptoTools/Common/Matrix.h>
#include <cryptoTools/Common/Log.h>
#include <libOTe/Tools/Tools.h>
#include <cryptoTools/Crypto/RandomOracle.h>
#include "Sh3Converter.h"
#include <immintrin.h>
#include <iomanip>
#include <thread>
#include <chrono>
//std::ostream& operator<<(std::ostream& out, const __m256i& block);
//namespace osuCrypto
//{
//    using ::operator<<;
//}

inline bool eq(const __m256i& lhs, const __m256i& rhs)
{
	std::array<char, 64> l, r;
	void* ll = l.data(), * rr = r.data();
	size_t size = l.size();
	std::align(32, 32, ll, size);
	size = r.size();
	std::align(32, 32, rr, size);
	//if ((aby3::u64)ll.data() % 32)
	//    throw RTE_LOC;
	//if ((aby3::u64)rr.data() % 32)
	//    throw RTE_LOC;
	_mm256_store_si256((__m256i*) ll, lhs);
	_mm256_store_si256((__m256i*) rr, rhs);

	return memcmp(ll, rr, 32) == 0;
}

inline bool neq(const __m256i& lhs, const __m256i& rhs)
{
	return !eq(lhs, rhs);
}

#ifdef _MSC_VER

inline __m256i operator^(const __m256i& lhs, const __m256i& rhs)
{
	return _mm256_xor_si256(lhs, rhs);
}
inline __m256i operator&(const __m256i& lhs, const __m256i& rhs)
{
	return _mm256_and_si256(lhs, rhs);
}

#endif

#undef NDEBUG

namespace aby3
{



	using namespace oc;

	void Sh3BinaryEvaluator::setCir(BetaCircuit* cir, u64 width, block prevSeed, block nextSeed)
	{

		if (cir->mLevelCounts.size() == 0)
		{
			if (mDebug && cir->mPrints.size())
				cir->levelByAndDepth(oc::BetaCircuit::LevelizeType::NoReorder);
			else
			{
				mDebugHadPrints = cir->mPrints.size();
				cir->levelByAndDepth();
			}
		}

		mCir = cir;
		mPrintIter = mCir->mPrints.begin();

		// each row of mem corresponds to a wire. Each column of mem corresponds to 64 SIMD bits
		mMem.reset(width, mCir->mWireCount, 8);

		mShareIdx = 0;
		mShareAES[0].setKey(prevSeed);
		mShareAES[1].setKey(nextSeed);
		i32 simdWidth128 = static_cast<i32>(mMem.simdWidth());
		mShareBuff.resize(simdWidth128);

#ifdef BINARY_ENGINE_DEBUG
		if (mDebug)
		{
			mPlainWires_DEBUG.resize(width);
			for (auto& m : mPlainWires_DEBUG)
			{
				m.resize(cir->mWireCount);
				memset(m.data(), 0, m.size() * sizeof(DEBUG_Triple));
			}
		}
#endif
	}

	void Sh3BinaryEvaluator::setReplicatedInput(u64 idx, const sbMatrix& in)
	{
		mLevel = 0;
		auto& inWires = mCir->mInputs[idx].mWires;
		auto simdWidth = mMem.simdWidth();
		auto bitCount = in.bitCount();

		if (mCir == nullptr)
			throw std::runtime_error(LOCATION);
		if (idx >= mCir->mInputs.size())
			throw std::invalid_argument("input index out of bounds");
		if (bitCount != inWires.size())
			throw std::invalid_argument("input data wrong size");
		if (in.rows() != 1)
			throw std::invalid_argument("incorrect number of simd rows");


		std::array<char, 2> vals{ 0,~0 };

		for (u64 i = 0; i < 2; ++i)
		{
			auto& shares = mMem.mShares[i];
			BitIterator iter((u8*)in.mShares[i].data(), 0);

			for (u64 j = 0; j < inWires.size(); ++j, ++iter)
			{
				if (shares.rows() <= inWires[j])
					throw std::runtime_error(LOCATION);

				char v = vals[*iter];
				memset(shares.data() + simdWidth * inWires[j], v, simdWidth * sizeof(block_type));
			}
		}


#ifdef BINARY_ENGINE_DEBUG
		if (mDebug)
		{
			auto prevIdx = (mDebugPartyIdx + 2) % 3;
			auto bv0 = BitVector((u8*)in.mShares[0][0].data(), inWires.size());
			auto bv1 = BitVector((u8*)in.mShares[1][0].data(), inWires.size());

			for (u64 i = 0; i < inWires.size(); ++i)
			{
				for (u64 r = 0; r < mPlainWires_DEBUG.size(); ++r)
				{
					auto& m = mPlainWires_DEBUG[r];
					m[inWires[i]].mBits[mDebugPartyIdx] = bv0[i];
					m[inWires[i]].mBits[prevIdx] = bv1[i];
					m[inWires[i]].mIsSet = true;
				}

				validateWire(inWires[i]);
			}
		}
#endif
	}


	std::string hex(span<u8> view, u64 len = ~0ull)
	{
		std::stringstream ss;
		auto min = std::min<u64>(len, view.size());
		for (u64 i = 0; i < min; ++i)
		{
			ss << std::setw(2) << std::setfill('0') << std::hex << u64(view[i]);
		}
		if (min != view.size())
			ss << "...";
		return ss.str();
	}

	std::string hex(span<__m256i> view, u64 len = ~0ull)
	{
		span<u8> vv((u8*)view.data(), view.size() * sizeof(__m256i));
		return hex(vv, len);
	}


	std::string hex(span<block> view, u64 len = ~0ull)
	{
		span<u8> vv((u8*)view.data(), view.size() * sizeof(block));
		return hex(vv, len);
	}


	void Sh3BinaryEvaluator::setInput(u64 idx, const sbMatrix& in)
	{
		if (idx >= mCir->mInputs.size())
			throw std::invalid_argument("input index out of bounds");

		auto& inWires = mCir->mInputs[idx];
		setInput(inWires, in);
	}

	void Sh3BinaryEvaluator::setInput(const BetaBundle& inWires, const sbMatrix& in)
	{

		mLevel = 0;
		auto simdWidth = mMem.simdWidth();

		auto bitCount = in.bitCount();
		auto inByteLength = (bitCount + 7) / 8;
		auto inRows = in.rows();

		if (mCir == nullptr)
			throw std::runtime_error(LOCATION);
		if (bitCount != inWires.size())
			throw std::invalid_argument("input data wrong size");
		if (inRows != mMem.shareCount())
			throw std::invalid_argument("incorrect number of rows");

		for (u64 i = 0; i < inWires.size() - 1; ++i)
		{
			if (inWires[i] + 1 != inWires[i + 1])
				throw std::runtime_error("expecting contiguous input wires. " LOCATION);
		}

		for (u64 i = 0; i < 2; ++i)
		{
			auto& shares = mMem.mShares[i];

			MatrixView<u8> inView(
				(u8*)(in.mShares[i].data()),
				(u8*)(in.mShares[i].data() + in.mShares[i].size()),
				sizeof(i64) * in.mShares[i].cols());

			if (inWires.back() > shares.rows())
				throw std::runtime_error(LOCATION);

			MatrixView<u8> memView(
				(u8*)(shares.data() + simdWidth * inWires.front()),
				(u8*)(shares.data() + simdWidth * (inWires.back() + 1)),
				sizeof(block_type) * simdWidth);

			if (memView.data() + memView.size() > (u8*)(shares.data() + shares.size()))
				throw std::runtime_error(LOCATION);


			auto extra = memView.cols() - inByteLength;
			if (extra)
			{
				for (u64 ii = 0; ii < memView.rows(); ++ii)
				{
					memset(memView[ii].data() + inByteLength, 0, extra);
				}
			}
			transpose(inView, memView);
		}

#ifdef BINARY_ENGINE_DEBUG

		if (mDebug)
		{
			for (u64 r = 0; r < mPlainWires_DEBUG.size(); ++r)
			{
				auto prevIdx = (mDebugPartyIdx + 2) % 3;
				auto& m = mPlainWires_DEBUG[r];
				auto bv0 = BitVector((u8*)in.mShares[0][r].data(), inWires.size());
				auto bv1 = BitVector((u8*)in.mShares[1][r].data(), inWires.size());

				for (u64 i = 0; i < inWires.size(); ++i)
				{
					m[inWires[i]].mBits[mDebugPartyIdx] = bv0[i];
					m[inWires[i]].mBits[prevIdx] = bv1[i];
					m[inWires[i]].mIsSet = true;
				}
			}
		}
#endif

	}


	void Sh3BinaryEvaluator::setInput(u64 idx, const sPackedBin& in)
	{
		if (mCir == nullptr)
			throw std::runtime_error(LOCATION);
		if (idx >= mCir->mInputs.size())
			throw std::invalid_argument("input index out of bounds");
		if (in.shareCount() != mMem.shareCount())
			throw std::runtime_error(LOCATION);
		if (in.bitCount() != mCir->mInputs[idx].mWires.size())
			throw std::runtime_error(LOCATION);

		auto srcWidth = in.simdWidth() * sizeof(i64);
		auto dstWidth = mMem.simdWidth() * sizeof(block_type);

		auto& inWires = mCir->mInputs[idx];
		for (u64 i = 0; i < 2; ++i)
		{
			auto& share = mMem.mShares[i];
			for (u64 j = 0; j < inWires.size(); ++j)
			{
				auto dst = (u8*)(share.data() + mMem.simdWidth() * inWires[j]);
				auto src = (u8*)(in.mShares[i].data() + in.simdWidth() * j);

				memcpy(dst, src, srcWidth);

				if (dstWidth != srcWidth)
				{
					memset(dst + srcWidth, 0, dstWidth - srcWidth);
				}
			}
		}

#ifdef BINARY_ENGINE_DEBUG
		if (mDebug)
		{

			for (u64 r = 0; r < inWires.size(); ++r)
			{
				auto w = inWires[r];

				auto prevIdx = (mDebugPartyIdx + 2) % 3;
				auto bv0 = BitVector((u8*)in.mShares[0][r].data(), mPlainWires_DEBUG.size());
				auto bv1 = BitVector((u8*)in.mShares[1][r].data(), mPlainWires_DEBUG.size());

				for (u64 i = 0; i < mPlainWires_DEBUG.size(); ++i)
				{
					auto& m = mPlainWires_DEBUG[i];
					m[w].mBits[mDebugPartyIdx] = bv0[i];
					m[w].mBits[prevIdx] = bv1[i];
					m[w].mIsSet = true;
				}
			}
		}
#endif
	}

#ifdef BINARY_ENGINE_DEBUG
	void Sh3BinaryEvaluator::validateMemory()
	{
		if (mDebug)
		{
			for (u64 i = 0; i < mCir->mWireCount; ++i)
			{
				validateWire(i);
			}
		}
	}


	void Sh3BinaryEvaluator::validateWire(u64 wireIdx)
	{
		if (mDebug && mPlainWires_DEBUG[0][wireIdx].mIsSet)
		{
			auto shareCount = mPlainWires_DEBUG.size();
			auto prevIdx = (mDebugPartyIdx + 2) % 3;

			for (u64 r = 0; r < shareCount; ++r)
			{
				auto& triple = mPlainWires_DEBUG[r][wireIdx];

				auto bit0 = extractBitShare(r, wireIdx, 0);
				auto bit1 = extractBitShare(r, wireIdx, 1);
				if (triple.mBits[mDebugPartyIdx] != bit0)
				{
					std::cout << "party " << mDebugPartyIdx << " wire " << wireIdx << " row " << r << " s " << 0 << " ~~"
						<< " exp:" << int(triple.mBits[mDebugPartyIdx])
						<< " act:" << int(bit0) << std::endl;

					throw std::runtime_error(LOCATION);

				}
				if (triple.mBits[prevIdx] != bit1)
				{

					std::cout << "party " << mDebugPartyIdx << " wire " << wireIdx << " row " << r << " s " << 1 << " ~~"
						<< " exp:" << int(triple.mBits[prevIdx])
						<< " act:" << int(bit1) << std::endl;
					throw std::runtime_error(LOCATION);
				}
			}
		}
	}
	void Sh3BinaryEvaluator::distributeInputs()
	{

		if (mDebug)
		{
			auto prevIdx = (mDebugPartyIdx + 2) % 3;
			auto nextIdx = (mDebugPartyIdx + 1) % 3;
			for (u64 r = 0; r < mPlainWires_DEBUG.size(); ++r)
			{
				auto& m = mPlainWires_DEBUG[r];

				std::vector<u8> s0(m.size()), s1;
				for (u64 i = 0; i < s0.size(); ++i)
				{
					s0[i] = m[i].mBits[mDebugPartyIdx];
				}
				mDebugPrev.asyncSendCopy(s0);
				mDebugNext.asyncSendCopy(s0);
				mDebugPrev.recv(s0);
				mDebugNext.recv(s1);
				if (s0.size() != m.size())
					throw std::runtime_error(LOCATION);

				for (u64 i = 0; i < m.size(); ++i)
				{
					if (m[i].mBits[prevIdx] != s0[i])
					{
						std::cout << "parties " << mDebugPartyIdx << " & " << prevIdx << " have inconsistent shares at row " << r << " wire " << i << " share " << prevIdx << std::endl;
						throw std::runtime_error(LOCATION);
					}

					m[i].mBits[nextIdx] = s1[i];
				}
			}
		}
	}
	oc::block Sh3BinaryEvaluator::hashDebugState()
	{
		if (mDebug == false)
			return ZeroBlock;

		RandomOracle sha(sizeof(block));
		for (u64 r = 0; r < mPlainWires_DEBUG.size(); ++r)
		{
			auto& m = mPlainWires_DEBUG[r];
			sha.Update(m.data(), m.size());
		}

		block b;
		sha.Final(b);
		return b;
	}
#endif


	Sh3Task Sh3BinaryEvaluator::asyncEvaluate(
		Sh3Task dep,
		oc::BetaCircuit* cir,
		Sh3ShareGen& gen,
		std::vector<const sbMatrix*> inputs,
		std::vector<sbMatrix*> outputs)
	{
		if (cir->mInputs.size() != inputs.size())
			throw std::runtime_error(LOCATION);
		if (cir->mOutputs.size() != outputs.size())
			throw std::runtime_error(LOCATION);

		return dep.then([this, cir, &gen, inputs = std::move(inputs)](CommPkg& comm, Sh3Task& self)
		{
			auto width = inputs[0]->rows();
			setCir(cir, width, gen);

			for (u64 i = 0; i < inputs.size(); ++i)
			{
				if (inputs[i]->rows() != width)
					throw std::runtime_error(LOCATION);

				setInput(i, *inputs[i]);
			}

#ifdef BINARY_ENGINE_DEBUG
			validateMemory();
			distributeInputs();
#endif

			roundCallback(comm, self);

		}).getClosure().then([this, outputs = std::move(outputs)](Sh3Task& self)
		{
			for (u64 i = 0; i < outputs.size(); ++i)
			{
				getOutput(i, *outputs[i]);
			}
		});
	}

	Sh3Task Sh3BinaryEvaluator::asyncEvaluate(Sh3Task dependency)
	{
#ifdef BINARY_ENGINE_DEBUG
		validateMemory();
		distributeInputs();
#endif

		auto ret = dependency.then([this](CommPkg& comm, Sh3Task& self)
			{
				roundCallback(comm, self);
			}, "bin-eval-closure").getClosure();


			return ret;
	}

	u8 extractBit(i64* data, u64 rowIdx)
	{
		auto k = rowIdx / (sizeof(i64) * 8);
		auto j = rowIdx % (sizeof(i64) * 8);

		return (data[k] >> j) & 1;
	}

	u8 Sh3BinaryEvaluator::extractBitShare(u64 rowIdx, u64 wireIdx, u64 shareIdx)
	{
		if (rowIdx >= mMem.shareCount())
			throw std::runtime_error(LOCATION);

		auto row = (i64*)mMem.mShares[shareIdx][wireIdx].data();
		auto k = rowIdx / (sizeof(i64) * 8);
		auto j = rowIdx % (sizeof(i64) * 8);

		return (row[k] >> j) & 1;
	}

	std::ostream& operator<<(std::ostream& out, Sh3BinaryEvaluator::DEBUG_Triple& triple)
	{
		out << "(" << (int)triple.mBits[0] << " " << (int)triple.mBits[1] << " " << (int)triple.mBits[2] << ") = " << triple.val();

		return out;
	}

	std::string prettyShare(u64 partyIdx, i64 share0, i64 share1 = -1)
	{
		std::array<i64, 3> shares;
		shares[partyIdx] = share0;
		shares[(partyIdx + 2) % 3] = share1;
		shares[(partyIdx + 1) % 3] = -1;

		std::stringstream ss;
		ss << "(";
		if (shares[0] == -1) ss << "_ ";
		else ss << shares[0] << " ";
		if (shares[1] == -1) ss << "_ ";
		else ss << shares[1] << " ";
		if (shares[2] == -1) ss << "_)";
		else ss << shares[2] << ")";

		return ss.str();
	}

	void Sh3BinaryEvaluator::roundCallback(CommPkg& comm, Sh3Task task)
	{

		i32 shareCountDiv8 = static_cast<i32>((mMem.shareCount() + 7) / 8);
		i32 simdWidth128 = static_cast<i32>(mMem.simdWidth());

		block_type AllOneBlock;
		memset(&AllOneBlock, 0xFF, sizeof(block_type));

		if (mLevel > mCir->mLevelCounts.size())
		{
			throw std::runtime_error("evaluateRound() was called but no rounds remain... " LOCATION);
		}
		if (mCir->mLevelCounts.size() == 0 && mCir->mNonlinearGateCount)
			throw std::runtime_error("the level by and gate function must be called first." LOCATION);

		if (mLevel)
		{
			if (mRecvLocs.size())
			{
				for (auto& fu : mRecvFutr)
					fu.get();
				mRecvFutr.clear();

				auto iter = mRecvData.begin();

				for (u64 j = 0; j < mRecvLocs.size(); ++j)
				{
					auto out = mRecvLocs[j];
					mMem.mShares[1](out) = AllOneBlock;
					memcpy(&mMem.mShares[1](out), &*iter, shareCountDiv8);
					iter += shareCountDiv8;
				}
			}
		}
		else
		{
			mGateIter = mCir->mGates.begin();
#define NEW_SHARE
			block bb = oc::sysRandomSeed();
			if (mPrng.mBuffer.size())
			{
				auto ss = mPrng.get<block>();
				bb = bb ^ ss;
			}
			((block*)&mCheckBlock)[0] = toBlock(0x0102030405060);
			((block*)&mCheckBlock)[(sizeof(block_type) / sizeof(block)) - 1] = bb;
		}

		auto eq32 = [](void* l, void* r)
		{
			return memcmp(l, r, sizeof(block_type)) == 0;
		};


		auto ccCheck = [this, eq32]()
		{
			//auto k = 0;

			if (mGateIter >= mCir->mGates.end())
				throw RTE_LOC;

			auto gate = *mGateIter;
			std::array<BetaWire, 2> in{ gate.mInput[0], gate.mInput[1] };
			auto nw = mMem.mShares[1].rows();

			if (nw <= in[0] || nw <= in[1])
				throw RTE_LOC;


			for (u64 j = 0; j < 2; ++j)
			{
				auto x = mMem.mShares[1][in[0]].data();

				if (eq32(x, &mCheckBlock))
					throw RTE_LOC;
			}

			memcpy(
				mMem.mShares[1][gate.mOutput].data(),
				&mCheckBlock,
				sizeof(block_type));
		};

		auto print = [this]()
		{
			auto gateIdx = mGateIter - mCir->mGates.begin();
			while (mDebugPartyIdx == 0 &&
				mPrintIdx != ~0ull &&
				mPrintIter != mCir->mPrints.end() &&
				mPrintIter->mGateIdx <= (u64)gateIdx)
			{

				auto wireIdx = mPrintIter->mWire;
				auto invert = mPrintIter->mInvert;

				if (wireIdx != ~u32(0))
				{
					auto mem = mPlainWires_DEBUG[mPrintIdx][wireIdx].val();
					std::cout << (u64)(mem ^ (invert ? 1 : 0));
				}

				if (mPrintIter->mMsg.size())
					std::cout << mPrintIter->mMsg;

				++mPrintIter;
			}
		};

#ifdef BINARY_ENGINE_DEBUG
		const bool debug = mDebug;
#endif
		auto& shares = mMem.mShares;
		//const auto bufferSize = std::max<i32>(1 << 16, shareCountDiv8);

		std::array<block_type, 8> t0, t1, t2;

		if (mLevel < mCir->mLevelCounts.size())
		{
			auto andGateCount = mCir->mLevelAndCounts[mLevel];
			auto gateCount = mCir->mLevelCounts[mLevel];

			mRecvLocs.resize(andGateCount);
			auto updateIter = mRecvLocs.data();
			auto& sendBuff = mSendBuffs[mLevel & 1];

			sendBuff.resize(andGateCount * shareCountDiv8);
			mRecvData.resize(andGateCount * shareCountDiv8);
			auto writeIter = sendBuff.begin();
			//auto prevSendIdx = 0ull;
			//auto nextSendIdx = std::min<i32>(bufferSize, static_cast<i32>(sendBuff.size()));

			for (u64 j = 0; j < gateCount; ++j, ++mGateIter)
			{
				const auto& gate = *mGateIter;
				const auto& type = gate.mType;
				auto out = gate.mOutput * shares[0].stride();

				auto s0_Out = shares[0][gate.mOutput];
				auto s1_Out = shares[1][gate.mOutput];
				auto s0_in0 = shares[0][gate.mInput[0]];
				auto s0_in1 = shares[0][gate.mInput[1]];
				auto s1_in0 = shares[1][gate.mInput[0]];
				auto s1_in1 = shares[1][gate.mInput[1]];

				if (gate.mOutput == gate.mInput[0])
					throw RTE_LOC;
				if (gate.mOutput == gate.mInput[1])
					throw RTE_LOC;
				if (gate.mInput[0] == gate.mInput[1] && type != GateType::a)
					throw RTE_LOC;

				// GateType::a gates are copy gates and their second input is
				// how many wires starting at mInput[0] should be copied.
				// THerefore we just set si_in1 to be a dummy value (0) for GateType::a

				//auto x = s1_in0.data();
				block_type* z = nullptr;

				switch (gate.mType)
				{
				case GateType::Xor:


					if (eq32(s1_in0.data(), &mCheckBlock))
						throw RTE_LOC;
					if (eq32(s1_in1.data(), &mCheckBlock))
						throw RTE_LOC;

					for (i32 k = 0; k < simdWidth128; k += 8)
					{
						s0_Out[k + 0] = s0_in0[k + 0] ^ s0_in1[k + 0];
						s0_Out[k + 1] = s0_in0[k + 1] ^ s0_in1[k + 1];
						s0_Out[k + 2] = s0_in0[k + 2] ^ s0_in1[k + 2];
						s0_Out[k + 3] = s0_in0[k + 3] ^ s0_in1[k + 3];
						s0_Out[k + 4] = s0_in0[k + 4] ^ s0_in1[k + 4];
						s0_Out[k + 5] = s0_in0[k + 5] ^ s0_in1[k + 5];
						s0_Out[k + 6] = s0_in0[k + 6] ^ s0_in1[k + 6];
						s0_Out[k + 7] = s0_in0[k + 7] ^ s0_in1[k + 7];

						s1_Out[k + 0] = s1_in0[k + 0] ^ s1_in1[k + 0];
						s1_Out[k + 1] = s1_in0[k + 1] ^ s1_in1[k + 1];
						s1_Out[k + 2] = s1_in0[k + 2] ^ s1_in1[k + 2];
						s1_Out[k + 3] = s1_in0[k + 3] ^ s1_in1[k + 3];
						s1_Out[k + 4] = s1_in0[k + 4] ^ s1_in1[k + 4];
						s1_Out[k + 5] = s1_in0[k + 5] ^ s1_in1[k + 5];
						s1_Out[k + 6] = s1_in0[k + 6] ^ s1_in1[k + 6];
						s1_Out[k + 7] = s1_in0[k + 7] ^ s1_in1[k + 7];
					}
					break;
				case GateType::And:
					*updateIter++ = static_cast<i32>(out);
					z = getShares();
					for (i32 k = 0; k < simdWidth128; k += 8)
					{

						t0[0] = s0_in0[k + 0] & s0_in1[k + 0];
						t0[1] = s0_in0[k + 1] & s0_in1[k + 1];
						t0[2] = s0_in0[k + 2] & s0_in1[k + 2];
						t0[3] = s0_in0[k + 3] & s0_in1[k + 3];
						t0[4] = s0_in0[k + 4] & s0_in1[k + 4];
						t0[5] = s0_in0[k + 5] & s0_in1[k + 5];
						t0[6] = s0_in0[k + 6] & s0_in1[k + 6];
						t0[7] = s0_in0[k + 7] & s0_in1[k + 7];

						t1[0] = s0_in0[k + 0] & s1_in1[k + 0];
						t1[1] = s0_in0[k + 1] & s1_in1[k + 1];
						t1[2] = s0_in0[k + 2] & s1_in1[k + 2];
						t1[3] = s0_in0[k + 3] & s1_in1[k + 3];
						t1[4] = s0_in0[k + 4] & s1_in1[k + 4];
						t1[5] = s0_in0[k + 5] & s1_in1[k + 5];
						t1[6] = s0_in0[k + 6] & s1_in1[k + 6];
						t1[7] = s0_in0[k + 7] & s1_in1[k + 7];

						t0[0] = t0[0] ^ t1[0];
						t0[1] = t0[1] ^ t1[1];
						t0[2] = t0[2] ^ t1[2];
						t0[3] = t0[3] ^ t1[3];
						t0[4] = t0[4] ^ t1[4];
						t0[5] = t0[5] ^ t1[5];
						t0[6] = t0[6] ^ t1[6];
						t0[7] = t0[7] ^ t1[7];

						t1[0] = s1_in0[k + 0] & s0_in1[k + 0];
						t1[1] = s1_in0[k + 1] & s0_in1[k + 1];
						t1[2] = s1_in0[k + 2] & s0_in1[k + 2];
						t1[3] = s1_in0[k + 3] & s0_in1[k + 3];
						t1[4] = s1_in0[k + 4] & s0_in1[k + 4];
						t1[5] = s1_in0[k + 5] & s0_in1[k + 5];
						t1[6] = s1_in0[k + 6] & s0_in1[k + 6];
						t1[7] = s1_in0[k + 7] & s0_in1[k + 7];

						t0[0] = t0[0] ^ t1[0];
						t0[1] = t0[1] ^ t1[1];
						t0[2] = t0[2] ^ t1[2];
						t0[3] = t0[3] ^ t1[3];
						t0[4] = t0[4] ^ t1[4];
						t0[5] = t0[5] ^ t1[5];
						t0[6] = t0[6] ^ t1[6];
						t0[7] = t0[7] ^ t1[7];

						s0_Out[k + 0] = t0[0] ^ z[k + 0];
						s0_Out[k + 1] = t0[1] ^ z[k + 1];
						s0_Out[k + 2] = t0[2] ^ z[k + 2];
						s0_Out[k + 3] = t0[3] ^ z[k + 3];
						s0_Out[k + 4] = t0[4] ^ z[k + 4];
						s0_Out[k + 5] = t0[5] ^ z[k + 5];
						s0_Out[k + 6] = t0[6] ^ z[k + 6];
						s0_Out[k + 7] = t0[7] ^ z[k + 7];


					}

#ifndef NDEBUG
					ccCheck();
#endif
					memcpy(&*writeIter, s0_Out.data(), shareCountDiv8);
					writeIter += shareCountDiv8;

					break;
				case GateType::Nor:
					*updateIter++ = static_cast<i32>(out);
					z = getShares();
					for (i32 k = 0; k < simdWidth128; k += 8)
					{
						// t0 = mem00
						t0[0] = s0_in0[k + 0] ^ AllOneBlock;
						t0[1] = s0_in0[k + 1] ^ AllOneBlock;
						t0[2] = s0_in0[k + 2] ^ AllOneBlock;
						t0[3] = s0_in0[k + 3] ^ AllOneBlock;
						t0[4] = s0_in0[k + 4] ^ AllOneBlock;
						t0[5] = s0_in0[k + 5] ^ AllOneBlock;
						t0[6] = s0_in0[k + 6] ^ AllOneBlock;
						t0[7] = s0_in0[k + 7] ^ AllOneBlock;

						// t1 = mem01
						t1[0] = s0_in1[k + 0] ^ AllOneBlock;
						t1[1] = s0_in1[k + 1] ^ AllOneBlock;
						t1[2] = s0_in1[k + 2] ^ AllOneBlock;
						t1[3] = s0_in1[k + 3] ^ AllOneBlock;
						t1[4] = s0_in1[k + 4] ^ AllOneBlock;
						t1[5] = s0_in1[k + 5] ^ AllOneBlock;
						t1[6] = s0_in1[k + 6] ^ AllOneBlock;
						t1[7] = s0_in1[k + 7] ^ AllOneBlock;

						// t2 = mem10
						t2[0] = s1_in0[k + 0] ^ AllOneBlock;
						t2[1] = s1_in0[k + 1] ^ AllOneBlock;
						t2[2] = s1_in0[k + 2] ^ AllOneBlock;
						t2[3] = s1_in0[k + 3] ^ AllOneBlock;
						t2[4] = s1_in0[k + 4] ^ AllOneBlock;
						t2[5] = s1_in0[k + 5] ^ AllOneBlock;
						t2[6] = s1_in0[k + 6] ^ AllOneBlock;
						t2[7] = s1_in0[k + 7] ^ AllOneBlock;

						// out = mem11
						s0_Out[k + 0] = s1_in1[k + 0] ^ AllOneBlock;
						s0_Out[k + 1] = s1_in1[k + 1] ^ AllOneBlock;
						s0_Out[k + 2] = s1_in1[k + 2] ^ AllOneBlock;
						s0_Out[k + 3] = s1_in1[k + 3] ^ AllOneBlock;
						s0_Out[k + 4] = s1_in1[k + 4] ^ AllOneBlock;
						s0_Out[k + 5] = s1_in1[k + 5] ^ AllOneBlock;
						s0_Out[k + 6] = s1_in1[k + 6] ^ AllOneBlock;
						s0_Out[k + 7] = s1_in1[k + 7] ^ AllOneBlock;

						// t3 = mem11 & mem00
						s0_Out[k + 0] = s0_Out[k + 0] & t0[0];
						s0_Out[k + 1] = s0_Out[k + 1] & t0[1];
						s0_Out[k + 2] = s0_Out[k + 2] & t0[2];
						s0_Out[k + 3] = s0_Out[k + 3] & t0[3];
						s0_Out[k + 4] = s0_Out[k + 4] & t0[4];
						s0_Out[k + 5] = s0_Out[k + 5] & t0[5];
						s0_Out[k + 6] = s0_Out[k + 6] & t0[6];
						s0_Out[k + 7] = s0_Out[k + 7] & t0[7];

						// t2 = mem10 & mem01
						t2[0] = t2[0] & t1[0];
						t2[1] = t2[1] & t1[1];
						t2[2] = t2[2] & t1[2];
						t2[3] = t2[3] & t1[3];
						t2[4] = t2[4] & t1[4];
						t2[5] = t2[5] & t1[5];
						t2[6] = t2[6] & t1[6];
						t2[7] = t2[7] & t1[7];

						// out = mem11 & mem00 ^ mem10 & mem01
						s0_Out[k + 0] = s0_Out[k + 0] ^ t2[0];
						s0_Out[k + 1] = s0_Out[k + 1] ^ t2[1];
						s0_Out[k + 2] = s0_Out[k + 2] ^ t2[2];
						s0_Out[k + 3] = s0_Out[k + 3] ^ t2[3];
						s0_Out[k + 4] = s0_Out[k + 4] ^ t2[4];
						s0_Out[k + 5] = s0_Out[k + 5] ^ t2[5];
						s0_Out[k + 6] = s0_Out[k + 6] ^ t2[6];
						s0_Out[k + 7] = s0_Out[k + 7] ^ t2[7];

						// t1 = mem00 & mem01
						t1[0] = t0[0] & t1[0];
						t1[1] = t0[1] & t1[1];
						t1[2] = t0[2] & t1[2];
						t1[3] = t0[3] & t1[3];
						t1[4] = t0[4] & t1[4];
						t1[5] = t0[5] & t1[5];
						t1[6] = t0[6] & t1[6];
						t1[7] = t0[7] & t1[7];

						// out = mem11 & mem00 ^ mem10 & mem01 ^ mem00 & mem01
						s0_Out[k + 0] = s0_Out[k + 0] ^ t1[0];
						s0_Out[k + 1] = s0_Out[k + 1] ^ t1[1];
						s0_Out[k + 2] = s0_Out[k + 2] ^ t1[2];
						s0_Out[k + 3] = s0_Out[k + 3] ^ t1[3];
						s0_Out[k + 4] = s0_Out[k + 4] ^ t1[4];
						s0_Out[k + 5] = s0_Out[k + 5] ^ t1[5];
						s0_Out[k + 6] = s0_Out[k + 6] ^ t1[6];
						s0_Out[k + 7] = s0_Out[k + 7] ^ t1[7];

						// out = mem11 & mem00 ^ mem10 & mem01 ^ mem00 & mem01 ^ z0 ^ z1
						s0_Out[k + 0] = s0_Out[k + 0] ^ z[k + 0];
						s0_Out[k + 1] = s0_Out[k + 1] ^ z[k + 1];
						s0_Out[k + 2] = s0_Out[k + 2] ^ z[k + 2];
						s0_Out[k + 3] = s0_Out[k + 3] ^ z[k + 3];
						s0_Out[k + 4] = s0_Out[k + 4] ^ z[k + 4];
						s0_Out[k + 5] = s0_Out[k + 5] ^ z[k + 5];
						s0_Out[k + 6] = s0_Out[k + 6] ^ z[k + 6];
						s0_Out[k + 7] = s0_Out[k + 7] ^ z[k + 7];
					}

#ifndef NDEBUG
					ccCheck();
#endif

					memcpy(&*writeIter, s0_Out.data(), shareCountDiv8);
					writeIter += shareCountDiv8;
					break;
				case GateType::Or:
					*updateIter++ = static_cast<i32>(out);
					z = getShares();
					for (i32 k = 0; k < simdWidth128; k += 8)
					{
						// the following OR gate logic is: z = (x & y) ^ (x ^ y).
						// 1. first comput the and gate.
						t0[0] = s0_in0[k + 0] & s0_in1[k + 0];
						t0[1] = s0_in0[k + 1] & s0_in1[k + 1];
						t0[2] = s0_in0[k + 2] & s0_in1[k + 2];
						t0[3] = s0_in0[k + 3] & s0_in1[k + 3];
						t0[4] = s0_in0[k + 4] & s0_in1[k + 4];
						t0[5] = s0_in0[k + 5] & s0_in1[k + 5];
						t0[6] = s0_in0[k + 6] & s0_in1[k + 6];
						t0[7] = s0_in0[k + 7] & s0_in1[k + 7];

						t1[0] = s0_in0[k + 0] & s1_in1[k + 0];
						t1[1] = s0_in0[k + 1] & s1_in1[k + 1];
						t1[2] = s0_in0[k + 2] & s1_in1[k + 2];
						t1[3] = s0_in0[k + 3] & s1_in1[k + 3];
						t1[4] = s0_in0[k + 4] & s1_in1[k + 4];
						t1[5] = s0_in0[k + 5] & s1_in1[k + 5];
						t1[6] = s0_in0[k + 6] & s1_in1[k + 6];
						t1[7] = s0_in0[k + 7] & s1_in1[k + 7];

						t0[0] = t0[0] ^ t1[0];
						t0[1] = t0[1] ^ t1[1];
						t0[2] = t0[2] ^ t1[2];
						t0[3] = t0[3] ^ t1[3];
						t0[4] = t0[4] ^ t1[4];
						t0[5] = t0[5] ^ t1[5];
						t0[6] = t0[6] ^ t1[6];
						t0[7] = t0[7] ^ t1[7];

						t1[0] = s1_in0[k + 0] & s0_in1[k + 0];
						t1[1] = s1_in0[k + 1] & s0_in1[k + 1];
						t1[2] = s1_in0[k + 2] & s0_in1[k + 2];
						t1[3] = s1_in0[k + 3] & s0_in1[k + 3];
						t1[4] = s1_in0[k + 4] & s0_in1[k + 4];
						t1[5] = s1_in0[k + 5] & s0_in1[k + 5];
						t1[6] = s1_in0[k + 6] & s0_in1[k + 6];
						t1[7] = s1_in0[k + 7] & s0_in1[k + 7];

						t0[0] = t0[0] ^ t1[0];
						t0[1] = t0[1] ^ t1[1];
						t0[2] = t0[2] ^ t1[2];
						t0[3] = t0[3] ^ t1[3];
						t0[4] = t0[4] ^ t1[4];
						t0[5] = t0[5] ^ t1[5];
						t0[6] = t0[6] ^ t1[6];
						t0[7] = t0[7] ^ t1[7];

						// 2. then comput the xor gate.
						s0_Out[k + 0] = t0[0] ^ s0_in0[k + 0] ^ s0_in1[k + 0] ^ z[k + 0];
						s0_Out[k + 1] = t0[1] ^ s0_in0[k + 1] ^ s0_in1[k + 1] ^ z[k + 1];
						s0_Out[k + 2] = t0[2] ^ s0_in0[k + 2] ^ s0_in1[k + 2] ^ z[k + 2];
						s0_Out[k + 3] = t0[3] ^ s0_in0[k + 3] ^ s0_in1[k + 3] ^ z[k + 3];
						s0_Out[k + 4] = t0[4] ^ s0_in0[k + 4] ^ s0_in1[k + 4] ^ z[k + 4];
						s0_Out[k + 5] = t0[5] ^ s0_in0[k + 5] ^ s0_in1[k + 5] ^ z[k + 5];
						s0_Out[k + 6] = t0[6] ^ s0_in0[k + 6] ^ s0_in1[k + 6] ^ z[k + 6];
						s0_Out[k + 7] = t0[7] ^ s0_in0[k + 7] ^ s0_in1[k + 7] ^ z[k + 7];
					}

#ifndef NDEBUG
					ccCheck();
#endif

					memcpy(&*writeIter, s0_Out.data(), shareCountDiv8);
					writeIter += shareCountDiv8;
					break;
				case GateType::Nxor:
					for (i32 k = 0; k < simdWidth128; k += 8)
					{
						s0_Out[k + 0] = s0_in0[k + 0] ^ s0_in1[k + 0];
						s0_Out[k + 1] = s0_in0[k + 1] ^ s0_in1[k + 1];
						s0_Out[k + 2] = s0_in0[k + 2] ^ s0_in1[k + 2];
						s0_Out[k + 3] = s0_in0[k + 3] ^ s0_in1[k + 3];
						s0_Out[k + 4] = s0_in0[k + 4] ^ s0_in1[k + 4];
						s0_Out[k + 5] = s0_in0[k + 5] ^ s0_in1[k + 5];
						s0_Out[k + 6] = s0_in0[k + 6] ^ s0_in1[k + 6];
						s0_Out[k + 7] = s0_in0[k + 7] ^ s0_in1[k + 7];

						s1_Out[k + 0] = s1_in0[k + 0] ^ s1_in1[k + 0];
						s1_Out[k + 1] = s1_in0[k + 1] ^ s1_in1[k + 1];
						s1_Out[k + 2] = s1_in0[k + 2] ^ s1_in1[k + 2];
						s1_Out[k + 3] = s1_in0[k + 3] ^ s1_in1[k + 3];
						s1_Out[k + 4] = s1_in0[k + 4] ^ s1_in1[k + 4];
						s1_Out[k + 5] = s1_in0[k + 5] ^ s1_in1[k + 5];
						s1_Out[k + 6] = s1_in0[k + 6] ^ s1_in1[k + 6];
						s1_Out[k + 7] = s1_in0[k + 7] ^ s1_in1[k + 7];

						s0_Out[k + 0] = s0_Out[k + 0] ^ AllOneBlock;
						s0_Out[k + 1] = s0_Out[k + 1] ^ AllOneBlock;
						s0_Out[k + 2] = s0_Out[k + 2] ^ AllOneBlock;
						s0_Out[k + 3] = s0_Out[k + 3] ^ AllOneBlock;
						s0_Out[k + 4] = s0_Out[k + 4] ^ AllOneBlock;
						s0_Out[k + 5] = s0_Out[k + 5] ^ AllOneBlock;
						s0_Out[k + 6] = s0_Out[k + 6] ^ AllOneBlock;
						s0_Out[k + 7] = s0_Out[k + 7] ^ AllOneBlock;

						s1_Out[k + 0] = s1_Out[k + 0] ^ AllOneBlock;
						s1_Out[k + 1] = s1_Out[k + 1] ^ AllOneBlock;
						s1_Out[k + 2] = s1_Out[k + 2] ^ AllOneBlock;
						s1_Out[k + 3] = s1_Out[k + 3] ^ AllOneBlock;
						s1_Out[k + 4] = s1_Out[k + 4] ^ AllOneBlock;
						s1_Out[k + 5] = s1_Out[k + 5] ^ AllOneBlock;
						s1_Out[k + 6] = s1_Out[k + 6] ^ AllOneBlock;
						s1_Out[k + 7] = s1_Out[k + 7] ^ AllOneBlock;

					}
					break;
				case GateType::a:



					if (eq32(s1_in0.data(), &mCheckBlock))
						throw RTE_LOC;
					if (eq32(s1_in1.data(), &mCheckBlock))
						throw RTE_LOC;


					for (i32 k = 0; k < simdWidth128; ++k)
					{
#ifndef NDEBUG
						if (eq(s1_in0[k], mCheckBlock))
							throw std::runtime_error(LOCATION);
#endif
						TODO("vectorize");
						s0_Out[k] = s0_in0[k];
						s1_Out[k] = s1_in0[k];

					}
					break;
				case GateType::na_And:
					z = getShares();

					*updateIter++ = static_cast<i32>(out);
					for (i32 k = 0; k < simdWidth128; ++k)
					{
						TODO("vectorize");
						s0_Out[k]
							= ((AllOneBlock ^ s0_in0[k]) & s0_in1[k])
							^ ((AllOneBlock ^ s0_in0[k]) & s1_in1[k])
							^ ((AllOneBlock ^ s1_in0[k]) & s0_in1[k])
							^ z[k];
					}

#ifndef NDEBUG
					ccCheck();
#endif

					memcpy(&*writeIter, s0_Out.data(), shareCountDiv8);
					writeIter += shareCountDiv8;
					break;
				case GateType::Zero:
				case GateType::nb_And:
				case GateType::nb:
				case GateType::na:
				case GateType::Nand:
				case GateType::nb_Or:
				case GateType::b:
				case GateType::na_Or:
				case GateType::One:
				default:

					throw std::runtime_error("BinaryEngine unsupported GateType " LOCATION);
					break;
				}


#ifdef BINARY_ENGINE_DEBUG
				if (debug)
				{
					auto gIdx = mGateIter - mCir->mGates.begin();
					auto gIn0 = gate.mInput[0];
					auto gIn1 = gate.mInput[1];
					auto gOut = gate.mOutput;

					auto prevIdx = (mDebugPartyIdx + 2) % 3;
					for (u64 r = 0; r < mPlainWires_DEBUG.size(); ++r)
					{
						auto s0_out = extractBitShare(r, gOut, 0);
						auto s0_in0_val = extractBit((i64*)s0_in0.data(), r);
						auto s1_in0_val = extractBit((i64*)s1_in0.data(), r);
						auto s0_in1_val = extractBit((i64*)s0_in1.data(), r);
						auto s1_in1_val = extractBit((i64*)s1_in1.data(), r);

						auto& m = mPlainWires_DEBUG[r];
						auto inTriple0 = m[gIn0];
						auto inTriple1 = m[gIn1];

						m[gOut].assign(inTriple0, inTriple1, gate.mType);

						auto badOutput = s0_out != m[gOut].mBits[mDebugPartyIdx];
						auto badInput0 =
							s0_in0_val != inTriple0.mBits[mDebugPartyIdx] ||
							s1_in0_val != inTriple0.mBits[prevIdx];
						auto badInput1 =
							s0_in1_val != inTriple1.mBits[mDebugPartyIdx] ||
							s1_in1_val != inTriple1.mBits[prevIdx];

						if (gIn0 == gOut) {
							badInput0 = false;
							s0_in0_val = -1;
							s1_in0_val = -1;
						}

						if (gIn1 == gOut) {
							badInput1 = false;
							s0_in1_val = -1;
							s1_in1_val = -1;
						}

						auto bad = badOutput || badInput0 || badInput1;

						if (bad)
						{

							std::this_thread::sleep_for(std::chrono::milliseconds(10));

							ostreamLock(std::cout)
								<< "\np " << mDebugPartyIdx << " gate[" << gIdx << "] r" << r << ": "
								<< gIn0 << " " << gateToString(type) << " " << gIn1 << " -> " << int(gOut)
								<< " exp: " << m[gOut] << "\n"
								<< " act: " << prettyShare(mDebugPartyIdx, s0_out) << "\n"
								<< " in0: " << inTriple0 << "\n"
								<< "*in0: " << prettyShare(mDebugPartyIdx, s0_in0_val, s1_in0_val) << "\n"
								<< " in1: " << inTriple1 << "\n"
								<< "*in1: " << prettyShare(mDebugPartyIdx, s0_in1_val, s1_in1_val) << "\n"
								<< std::endl;

							if (bad)
							{

								std::this_thread::sleep_for(std::chrono::milliseconds(1000));
								throw std::runtime_error(LOCATION);
							}
						}
					}


					print();
				}
#endif
			}

			if (writeIter != sendBuff.end())
				throw std::runtime_error(LOCATION);

			auto size = sendBuff.size();
			if (size)
			{
				if (mLevel == mCir->mLevelCounts.size() - 1)
					comm.mNext.asyncSend(std::move(sendBuff));
				else
					comm.mNext.asyncSend(sendBuff.data(), sendBuff.size());

				mRecvData.resize(size);
				mRecvFutr.emplace_back(comm.mPrev.asyncRecv(mRecvData));
			}
		}

		mLevel++;
		if (mGateIter > mCir->mGates.end())
		{
			oc::lout << "LOGIC error, gate iter past end if gates" << std::endl;
			throw std::runtime_error(LOCATION);
		}


		if (mDebug)
			print();


		if (hasMoreRounds())
		{
			auto t = task.then([this](CommPkg& comm, Sh3Task& task)
				{
					roundCallback(comm, task);
				}
			);

			t.name() = "callback";
		}
	}

	void Sh3BinaryEvaluator::getOutput(u64 i, sbMatrix& out, bool allowUninitialized)
	{
		if (mCir->mOutputs.size() <= i) throw std::runtime_error(LOCATION);

		const auto& outWires = mCir->mOutputs[i].mWires;

		getOutput(outWires, out, allowUninitialized);
	}

	void Sh3BinaryEvaluator::getOutput(u64 idx, sPackedBin& out, bool allowUninitialized)
	{
		const auto& outWires = mCir->mOutputs[idx].mWires;
		getOutput(outWires, out, allowUninitialized);
	}

	void Sh3BinaryEvaluator::getOutput(const std::vector<oc::BetaWire>& outWires, sPackedBin& out, bool allowUninitialized)
	{

		out.reset(mMem.shareCount(), outWires.size());

		auto simdWidth128 = mMem.simdWidth();

		for (u64 j = 0; j < 2; ++j)
		{
			auto prevIdx = (mDebugPartyIdx + 2) % 3;
			auto jj = !j ? mDebugPartyIdx : prevIdx;
			auto dest = out.mShares[j].data();

			for (u64 wireIdx = 0; wireIdx < outWires.size(); ++wireIdx, dest += out.simdWidth())
			{
				auto wire = outWires[wireIdx];
				if (wire >= mMem.bitCount())
					throw RTE_LOC;

				auto flag = mCir->mWireFlags[outWires[wireIdx]];
				if (flag == oc::BetaWireFlag::Uninitialized)
				{
					if (allowUninitialized)
						continue;
					else
						throw std::runtime_error("Reading an output wire that was not initialized. " LOCATION);
				}

				auto md = mMem.mShares[j].data();
				auto src = md + wire * simdWidth128;
				auto src2 = &mMem.mShares[j](wire, 0);
				if (src != src2)
					throw RTE_LOC;

				auto size = out.simdWidth() * sizeof(i64);
				if (dest + out.simdWidth() > out.mShares[j].data() + out.mShares[j].size())
					throw RTE_LOC;


				if (flag == oc::BetaWireFlag::One)
					memset(dest, -1, size);
				else if (flag == oc::BetaWireFlag::Zero)
					memset(dest, 0, size);
				else
					memcpy(dest, src, size);

				// check if we need to ivert these output wires.
				if (mCir->isInvert(wire))
				{
					for (u64 k = 0; k < out.simdWidth(); ++k)
					{
						dest[k] = ~dest[k];
					}
				}

#ifdef BINARY_ENGINE_DEBUG
				if (mDebug && flag != oc::BetaWireFlag::Zero && flag != oc::BetaWireFlag::One)
				{
					for (u64 r = 0; r < mPlainWires_DEBUG.size(); ++r)
					{
						auto m = mPlainWires_DEBUG[r];
						auto bit0 = extractBitShare(r, wire, j);

						if (m[wire].mBits[jj] != bit0)
							throw std::runtime_error(LOCATION);
					}
				}
#endif
			}
		}
	}

	void Sh3BinaryEvaluator::getOutput(const std::vector<BetaWire>& outWires, sbMatrix& out, bool allowUninitialized)
	{

		using Word = i64;
		if (outWires.size() != out.bitCount()) throw std::runtime_error(LOCATION);
		//auto outCols = roundUpTo(outWires.size(), 8);

		block_type AllOneBlock;
		memset(&AllOneBlock, 0xFF, sizeof(block_type));
		auto simdWidth128 = mMem.simdWidth();
		//auto simdWidth64 = (mMem.shareCount() + 63) / 64;
		Eigen::Matrix<block_type, Eigen::Dynamic, Eigen::Dynamic> temp;
		temp.resize(outWires.size(), simdWidth128);

		for (u64 j = 0; j < 2; ++j)
		{
			auto dest = temp.data();

			for (u64 i = 0; i < outWires.size(); ++i, dest += simdWidth128)
			{
				auto flag = mCir->mWireFlags[outWires[i]];
				if (flag == oc::BetaWireFlag::Uninitialized)
				{
					if (allowUninitialized)
						continue;
					else
						throw std::runtime_error("Reading an output wire that was not initialized. " LOCATION);
				}

				auto md = mMem.mShares[j].data();
				auto ms = mMem.mShares[j].size();
				auto src = md + outWires[i] * simdWidth128;
				auto size = simdWidth128 * sizeof(block_type);

				if (src + simdWidth128 > md + ms)
					throw std::runtime_error(LOCATION);
				if (dest + simdWidth128 > temp.data() + temp.size())
					throw std::runtime_error(LOCATION);

				if (flag == oc::BetaWireFlag::One)
				{
					memset(dest, -1, size);
				}
				else if (flag == oc::BetaWireFlag::Zero)
					memset(dest, 0, size);
				else
					memcpy(dest, src, size);

				// check if we need to ivert these output wires.
				if (mCir->isInvert(outWires[i]))
				{
					for (u64 k = 0; k < simdWidth128; ++k)
					{
						dest[k] = dest[k] ^ AllOneBlock;
					}
				}


#ifdef BINARY_ENGINE_DEBUG
				if (mDebug && flag != oc::BetaWireFlag::Zero && flag != oc::BetaWireFlag::One)
				{
					auto prev = (mDebugPartyIdx + 2) % 3;
					auto jj = !j ? mDebugPartyIdx : prev;
					for (u64 r = 0; r < mPlainWires_DEBUG.size(); ++r)
					{
						auto m = mPlainWires_DEBUG[r];

						auto bit0 = extractBitShare(r, outWires[i], j);

						if (m[outWires[i]].mBits[jj] != bit0)
							throw std::runtime_error(LOCATION);
					}
				}
#endif
			}
			MatrixView<u8> in((u8*)temp.data(), (u8*)(temp.data() + temp.size()), simdWidth128 * sizeof(block_type));
			MatrixView<u8> oout((u8*)out.mShares[j].data(), (u8*)(out.mShares[j].data() + out.mShares[j].size()), sizeof(Word) * out.mShares[j].cols());
			//memset(oout.data(), 0, oout.size());
			//out.mShares[j].setZero();
			memset(out.mShares[j].data(), 0, out.mShares[j].size() * sizeof(i64));
			transpose(in, oout);

#ifdef BINARY_ENGINE_DEBUG
			if (mDebug)
			{

				auto prev = (mDebugPartyIdx + 2) % 3;
				auto jj = !j ? mDebugPartyIdx : prev;

				for (u64 i = 0; i < outWires.size(); ++i)
				{
					if (mCir->mWireFlags[outWires[i]] == oc::BetaWireFlag::Uninitialized)
						continue;

					for (u64 r = 0; r < mPlainWires_DEBUG.size(); ++r)
					{
						auto m = mPlainWires_DEBUG[r];
						auto k = i / (sizeof(Word) * 8);
						auto l = i % (sizeof(Word) * 8);
						auto outWord = out.mShares[j](r, k);
						auto plain = (outWord >> l) & 1;

						auto inv = mCir->isInvert(outWires[i]) & 1;
						auto xx = m[outWires[i]].mBits[jj] ^ inv;
						if (xx != plain)
							throw std::runtime_error(LOCATION);
					}
				}
				auto mod = (outWires.size() % 64);
				u64 mask = ~(mod ? (Word(1) << mod) - 1 : -1);
				for (u64 i = 0; i < out.rows(); ++i)
				{
					auto cols = out.mShares[j].cols();
					if (out.mShares[j][i][cols - 1] & mask)
						throw std::runtime_error(LOCATION);
				}
			}
#endif
		}
	}

	Sh3BinaryEvaluator::block_type*
		Sh3BinaryEvaluator::getShares()
	{
#ifdef NEW_SHARE
		std::array<block, 8> temp;

		auto rem = mShareBuff.size() * sizeof(block_type) / sizeof(block);
		auto dest = (block*)mShareBuff.data();

		if (rem % 8)
			throw RTE_LOC;

		while (rem)
		{
			mShareAES[0].ecbEncCounterMode(mShareIdx, 8, temp.data());
			mShareAES[1].ecbEncCounterMode(mShareIdx, 8, dest);
			dest[0] = dest[0] ^ temp[0];
			dest[1] = dest[1] ^ temp[1];
			dest[2] = dest[2] ^ temp[2];
			dest[3] = dest[3] ^ temp[3];
			dest[4] = dest[4] ^ temp[4];
			dest[5] = dest[5] ^ temp[5];
			dest[6] = dest[6] ^ temp[6];
			dest[7] = dest[7] ^ temp[7];

			mShareIdx += 8;
			dest += 8;
			rem -= 8;
		}
#ifdef BINARY_ENGINE_DEBUG
		if (mDebug)
		{
			memset(mShareBuff.data(), 0, mShareBuff.size() * sizeof(block_type));
		}
#endif

		return mShareBuff.data();

#else
		mShareGen.refillBuffer();
		auto* z0 = (block_type*)mShareGen.mShareBuff[0].data();
		auto* z1 = (block_type*)mShareGen.mShareBuff[1].data();

#ifdef BINARY_ENGINE_DEBUG
		if (mDebug)
		{
			memset(mShareGen.mShareBuff[0].data(), 0, mShareGen.mShareBuff[0].size() * sizeof(block));
			memset(mShareGen.mShareBuff[1].data(), 0, mShareGen.mShareBuff[0].size() * sizeof(block));
		}
#endif
		for (u64 i = 0; i < mShareGen.mShareBuff[0].size(); ++i)
		{
			mShareGen.mShareBuff[0][i] = mShareGen.mShareBuff[0][i] ^ mShareGen.mShareBuff[1][i];
		}

		return  z0;
#endif

	}


#ifdef BINARY_ENGINE_DEBUG

	void Sh3BinaryEvaluator::DEBUG_Triple::assign(
		const DEBUG_Triple& in0,
		const DEBUG_Triple& in1,
		GateType type)
	{

		if (in0.mIsSet == false)
			throw RTE_LOC;
		if (in1.mIsSet == false)
			throw RTE_LOC;
		mIsSet = true;
		auto vIn0 = int(in0.val());
		auto vIn1 = int(in1.val());

		if (vIn0 > 1) throw std::runtime_error(LOCATION);
		if (vIn1 > 1) throw std::runtime_error(LOCATION);

		u8 plain;
		if (type == GateType::Xor)
		{
			plain = vIn0 ^ vIn1;
			std::array<u8, 3> t;

			t[0] = in0.mBits[0] ^ in1.mBits[0];
			t[1] = in0.mBits[1] ^ in1.mBits[1];
			t[2] = in0.mBits[2] ^ in1.mBits[2];

			mBits = t;
		}
		else if (type == GateType::Nxor)
		{
			plain = vIn0 ^ vIn1 ^ 1;

			std::array<u8, 3> t;

			t[0] = in0.mBits[0] ^ in1.mBits[0] ^ 1;
			t[1] = in0.mBits[1] ^ in1.mBits[1] ^ 1;
			t[2] = in0.mBits[2] ^ in1.mBits[2] ^ 1;

			mBits = t;
		}
		else if (type == GateType::And)
		{
			plain = vIn0 & vIn1;
			std::array<u8, 3> t;
			for (u64 b = 0; b < 3; ++b)
			{

				auto bb = b ? (b - 1) : 2;

				if (bb != (b + 2) % 3) throw std::runtime_error(LOCATION);

				auto in00 = in0.mBits[b];
				auto in01 = in0.mBits[bb];
				auto in10 = in1.mBits[b];
				auto in11 = in1.mBits[bb];

				t[b]
					= (in00 & in10)
					^ (in00 & in11)
					^ (in01 & in10);
			}

			mBits = t;
		}

		else if (type == GateType::na_And)
		{
			plain = (1 ^ vIn0) & vIn1;
			std::array<u8, 3> t;
			for (u64 b = 0; b < 3; ++b)
			{

				auto bb = b ? (b - 1) : 2;

				if (bb != (b + 2) % 3) throw std::runtime_error(LOCATION);

				auto in00 = 1 ^ in0.mBits[b];
				auto in01 = 1 ^ in0.mBits[bb];
				auto in10 = in1.mBits[b];
				auto in11 = in1.mBits[bb];

				t[b]
					= (in00 & in10)
					^ (in00 & in11)
					^ (in01 & in10);
			}

			mBits = t;
		}
		else if (type == GateType::Nor)
		{
			plain = (vIn0 | vIn1) ^ 1;


			std::array<u8, 3> t;
			for (u64 b = 0; b < 3; ++b)
			{
				auto bb = b ? (b - 1) : 2;
				auto in00 = 1 ^ in0.mBits[b];
				auto in01 = 1 ^ in0.mBits[bb];
				auto in10 = 1 ^ in1.mBits[b];
				auto in11 = 1 ^ in1.mBits[bb];

				t[b]
					= (in00 & in10)
					^ (in00 & in11)
					^ (in01 & in10);
			}

			mBits = t;
		}
		else if (type == GateType::a)
		{
			plain = vIn0;
			mBits = in0.mBits;
		}
		else
			throw std::runtime_error(LOCATION);

		if (plain != (mBits[0] ^ mBits[1] ^ mBits[2]))
		{
			//ostreamLock(std::cout) /*<< " g" << (mGateIter - mCir->mGates.data())
			//<< " act: " << plain << "   exp: " << (int)m[gate.mOutput] << std::endl*/

			//	<< vIn0 << " " << gateToString(type) << " " << vIn1 << " -> " << int(m[gOut]) << std::endl
			//	<< gIn0 << " " << gateToString(type) << " " << gIn1 << " -> " << int(gOut) << std::endl;
			//std::this_thread::sleep_for(std::chrono::milliseconds(100));
			throw std::runtime_error(LOCATION);
		}
	}
	#endif
}

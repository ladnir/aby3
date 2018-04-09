#pragma once
#include "LynxDefines.h"

using namespace oc;

namespace Lynx
{

	class BinaryEngine
	{
	public:

#define BINARY_ENGINE_DEBUG
#ifdef BINARY_ENGINE_DEBUG

        bool mDebug = false;

		struct DEBUG_Triple
		{
			std::array<u8, 3> mBits;
			u16 val()const { return mBits[0] ^ mBits[1] ^ mBits[2]; }

			void assign(const DEBUG_Triple& in0, const DEBUG_Triple& in1, GateType type);
		};
		std::vector<std::vector<DEBUG_Triple>> mPlainWires_DEBUG;
#endif

		Channel mPrev, mNext;
		BetaCircuit* mCir;
		BetaGate* mGateIter;
		u64 mLevel, mPartyIdx, mNextIdx, mPrevIdx;
		std::vector<u64> mUpdateLocations;
		//std::array<Eigen::Matrix<Word, Eigen::Dynamic, Eigen::Dynamic>, 2> mMem;
        PackedBinMatrix mMem;

        BinaryEngine() = default;
		BinaryEngine(u64 partyIdx, Channel& prev, Channel& next)
		{
            init(partyIdx, prev, next);
        }

        void init(u64 partyIdx, Channel& prev, Channel& next)
        {
            mPrev = prev;
            mNext = next; 
            mPartyIdx = partyIdx;
            mNextIdx = (partyIdx + 1) % 3;
            mPrevIdx = (partyIdx + 2) % 3;
        }

		void setCir(BetaCircuit* cir, u64 width);

        void setReplicatedInput(u64 i, const Matrix& in);
        void setInput(u64 i, const Matrix& in);
        void setInput(u64 i, const PackedBinMatrix& in);

		void evaluate();
		void evaluateRound();
        
        void getOutput(u64 i, Matrix& out);
        void getOutput(u64 i, PackedBinMatrix& out);
		void getOutput(const std::vector<BetaWire>& wires, Matrix& out);

		bool hasMoreRounds() const {
			return mLevel <= mCir->mLevelCounts.size();
		}
	};

}
#pragma once
#include <cryptoTools/Common/Defines.h> 
#ifdef ENABLE_CIRCUITS


#include "Gate.h"
#include <cryptoTools/Common/BitVector.h>
#include <array>
#include <sstream>
#include <functional>


namespace osuCrypto
{

	typedef u32 BetaWire;

	enum class BetaWireFlag
	{
		Zero,
		One,
		Wire,
		InvWire,
		Uninitialized
	};

	struct BetaGate
	{
		BetaGate() = default;
		BetaGate(const BetaWire& in0, const BetaWire& in1, const GateType& gt, const BetaWire& out)
			: mInput({ {in0, in1} })
			, mOutput(out)
			, mType(gt)
			, mAAlpha(gt == GateType::Nor || gt == GateType::na_And || gt == GateType::nb_Or || gt == GateType::Or)
			, mBAlpha(gt == GateType::Nor || gt == GateType::nb_And || gt == GateType::na_Or || gt == GateType::Or)
			, mCAlpha(gt == GateType::Nand || gt == GateType::nb_Or || gt == GateType::na_Or || gt == GateType::Or)
		{}

		void setType(osuCrypto::GateType gt)
		{
			mType = gt;
			// compute the gate modifier variables
			mAAlpha = (gt == GateType::Nor || gt == GateType::na_And || gt == GateType::nb_Or || gt == GateType::Or);
			mBAlpha = (gt == GateType::Nor || gt == GateType::nb_And || gt == GateType::na_Or || gt == GateType::Or);
			mCAlpha = (gt == GateType::Nand || gt == GateType::nb_Or || gt == GateType::na_Or || gt == GateType::Or);
		}

		std::array<BetaWire, 2> mInput;
		BetaWire mOutput;
		GateType mType;
		u8 mAAlpha, mBAlpha, mCAlpha;

		bool operator!=(const BetaGate& r) const {
			return mInput[0] != r.mInput[0]
				|| mInput[1] != r.mInput[1]
				|| mOutput != r.mOutput
				|| mType != r.mType;
		}
	};

	static_assert(sizeof(GateType) == 1, "");
	static_assert(sizeof(BetaGate) == 16, "");

	struct BetaBundle
	{
		std::vector<BetaWire> mWires;
		using iterator = std::vector<BetaWire>::iterator;
		using const_iterator = std::vector<BetaWire>::const_iterator;

		BetaBundle() {}
		BetaBundle(u64 s) :mWires(s, -1) {}

		BetaWire& operator[](u64 i) { return  mWires[i]; }
		const BetaWire& operator[](u64 i) const { return  mWires[i]; }

		u64 size() const { return mWires.size(); }
		void resize(u64 n) { mWires.resize(n, -1); }
		auto begin() { return mWires.begin(); }
		auto begin() const { return mWires.begin(); }
		auto end() { return mWires.end(); }
		auto end() const { return mWires.end(); }

		auto& front() { return mWires.front(); }
		auto& front() const { return mWires.front(); }
		auto& back() { return mWires.back(); }
		auto& back() const { return mWires.back(); }


		template<typename T>
		auto push_back(T&& t) { mWires.push_back(std::forward<T>(t)); }

		template<typename I>
		auto insert(iterator where, I begin, I end) { mWires.insert(where, begin, end); }

		auto insert(iterator where, u64 count, BetaWire val) { mWires.insert(where, count, val); }
	};


	class BetaCircuit
	{
	public:
		BetaCircuit();
		~BetaCircuit();


		std::string mName;
		u64 mNonlinearGateCount;
		BetaWire mWireCount;
		std::vector<BetaGate> mGates;


		struct Print
		{
			u64 mGateIdx;
			BetaWire mWire;
			std::string mMsg;
			bool mInvert;

			std::vector<BetaWire> mWires;
			std::vector<bool> mInvs;
			std::function<std::string(const BitVector& b)> mFn;


			Print() = default;
			Print(const Print&) = default;
			Print(Print&&) = default;
			Print& operator=(const Print&) = default;
			Print& operator=(Print&&) = default;

			Print(u64 g, BetaWire w, std::string m, bool inv)
				: mGateIdx(g)
				, mWire(w)
				, mMsg(m)
				, mInvert(inv)
			{}

			bool operator==(const Print& p) const
			{
				return
					mGateIdx == p.mGateIdx &&
					mWire == p.mWire &&
					mMsg == p.mMsg &&
					mInvert == p.mInvert;
			}
			bool operator!=(const Print& p) const
			{
				return !(*this == p);
			}
		};

		std::vector<Print> mPrints;
		using PrintIter = std::vector<Print>::iterator;

		std::vector<BetaWireFlag> mWireFlags;

		std::vector<BetaBundle> mInputs, mOutputs;
		std::vector<u64> mLevelCounts, mLevelAndCounts;

		void addTempWire(BetaWire& in);
		void addTempWireBundle(BetaBundle& in);
		void addInputBundle(BetaBundle& in);
		void addOutputBundle(BetaBundle& in);
		void addConstBundle(BetaBundle& in, const BitVector& val);

		void addGate(BetaWire in0, BetaWire in2, GateType gt, BetaWire out);
		void addConst(BetaWire wire, u8 val);
		void addInvert(BetaWire wire);
		void addInvert(BetaWire src, BetaWire dest);
		void addCopy(BetaWire src, BetaWire dest);
		void addCopy(const BetaBundle& src, const  BetaBundle& dest);

		bool isConst(BetaWire wire)const;
		bool isInvert(BetaWire wire)const;
		u8 constVal(BetaWire wire);

		void addPrint(BetaBundle in);
		void addPrint(BetaBundle in, std::function<std::string(const BitVector& b)> formatter);
		void addPrint(BetaWire wire);
		void addPrint(std::string);

		void evaluate(span<BitVector> input, span<BitVector> output, bool print = true);

		enum LevelizeType
		{
			Reorder,
			NoReorder,
			SingleNoReorder
		};
		void levelByAndDepth();
		void levelByAndDepth(LevelizeType type);

#ifdef USE_JSON
		void writeJson(std::ostream& out);
		void readJson(std::istream& in);
#endif

		void writeBin(std::ostream& out);
		void readBin(std::istream& in);


		//BetaCircuit toBristol() const;
		void writeBristol(std::ostream& out)const;
		void readBristol(std::istream& in);

		bool operator==(const BetaCircuit& rhs)const {
			return *this != rhs;
		}
		bool operator!=(const BetaCircuit& rhs)const;

		block hash() const;

		template<typename T>
		BetaCircuit& operator<<(const T& s)
		{
			std::stringstream ss;
			ss << s;
			addPrint(ss.str());
			return *this;
		}
		BetaCircuit& operator<<(const BetaWire& s)
		{
			addPrint(s);
			return *this;
		}
		BetaCircuit& operator<<(const BetaBundle& s)
		{
			addPrint(s);
			return *this;
		}

	};

}
#endif
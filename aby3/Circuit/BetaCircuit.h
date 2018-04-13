#pragma once


#include "aby3/Circuit/Gate.h"
#include <cryptoTools/Common/Defines.h>
#include <cryptoTools/Common/BitVector.h>
#include <array>


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
			: mInput({in0, in1})
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
        BetaBundle() {}
		BetaBundle(u64 s) :mWires(s, -1) {}
		std::vector<BetaWire> mWires;

        u64 size() const { return mWires.size(); }
		BetaWire& operator[](u64 i) { return  mWires[i]; }
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
        std::vector<std::tuple<u64, BetaWire, std::string, bool>> mPrints;
        std::vector<BetaWireFlag> mWireFlags;

        std::vector<BetaBundle> mInputs, mOutputs;
		std::vector<u64> mLevelCounts, mLevelAndCounts;

		void addTempWireBundle(BetaBundle& in);
		void addInputBundle(BetaBundle& in);
        void addOutputBundle(BetaBundle& in);
        void addConstBundle(BetaBundle& in, const BitVector& val);

		void addGate(BetaWire in0, BetaWire in2, GateType gt, BetaWire out);
        void addConst(BetaWire wire, u8 val);
		void addInvert(BetaWire wire);
		void addInvert(BetaWire src, BetaWire dest);
        void addCopy(BetaWire src, BetaWire dest);
        void addCopy(BetaBundle& src, BetaBundle& dest);

        bool isConst(BetaWire wire);
        bool isInvert(BetaWire wire);
        u8 constVal(BetaWire wire);

        void addPrint(BetaBundle in);
        void addPrint(BetaWire wire);
        void addPrint(std::string);

		void evaluate(span<BitVector> input, span<BitVector> output, bool print = true);


		void levelByAndDepth();


        void writeJson(std::ostream& out);
        void readJson(std::istream& in);

        void writeBin(std::ostream& out);
        void readBin(std::istream& in);

        bool operator==(const BetaCircuit& rhs)const {
            return *this != rhs;
        }
        bool operator!=(const BetaCircuit& rhs)const;

        block hash() const;

	};

}
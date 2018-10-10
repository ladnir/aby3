#include "Table.h"

namespace osuCrypto
{
    SelectBundle SelectQuery::addInput(SharedTable::ColRef column)
    {
        if (mLeftTable == nullptr)
            throw std::runtime_error("call joinOn(...) first. " LOCATION);

        mMem.emplace_back();
        auto& mem = mMem.back();
        mInputs.emplace_back(mMem.size() - 1, column, int(mInputs.size()));

        mem.mType = column.mCol.mType;
        mem.mInputIdx = mInputs.size() - 1;
        mem.mIdx = mMem.size() - 1;

        if (&column.mTable == mLeftTable)
            mLeftInputs.push_back(&mInputs.back());
        else if (&column.mTable == mRightTable)
            mRightInputs.push_back(&mInputs.back());
        else
            throw RTE_LOC;


        return { *this, mem.mIdx };
    }

    SelectBundle SelectQuery::joinOn(SharedTable::ColRef left, SharedTable::ColRef right)
    {
        if (mLeftTable)
            throw RTE_LOC;

        mLeftTable = &left.mTable;
        mRightTable = &right.mTable;
        mLeftCol = &left.mCol;
        mRightCol = &right.mCol;
        auto r = addInput(left);
        mMem.back().mUsed = true;

        addInput(right);

        return r;
    }

    int SelectQuery::addOp(selectDetails::SelectOp op, int wire1, int wire2)
    {
        if (mLeftTable == nullptr)
            throw std::runtime_error("call joinOn(...) first. " LOCATION);

        if (wire1 >= mMem.size() ||
            wire2 >= mMem.size())
            throw RTE_LOC;

        selectDetails::Mem mem;

        switch (op) {
        case selectDetails::BitwiseOr:
        case selectDetails::BitwiseAnd:
            if (mMem[wire1].mType->getBitCount() != mMem[wire2].mType->getBitCount())
                throw RTE_LOC;
            mem.mType = mMem[wire1].mType;
            break;
        case selectDetails::LessThan:
            mem.mType = std::make_shared<IntType>(1);
            break;
        case selectDetails::Multiply:
        case selectDetails::Add:
            mem.mType =
                mMem[wire1].mType->getBitCount() > mMem[wire2].mType->getBitCount() ?
                mMem[wire1].mType :
                mMem[wire1].mType;
            break;
        default:
            throw RTE_LOC;
        }

        mMem[wire1].mUsed = true;
        mMem[wire2].mUsed = true;

        selectDetails::Gate gate;
        gate.op = op;
        gate.mIn1 = wire1;
        gate.mIn2 = wire2;
        gate.mOut = mMem.size();

        mGates.push_back(gate);

        mem.mGate = &mGates.back();
        mem.mIdx = mMem.size();

        mMem.push_back(mem);

        return mem.mIdx;
    }

    int SelectQuery::addOp(selectDetails::SelectOp op, int wire1)
    {
        if (mLeftTable == nullptr)
            throw std::runtime_error("call joinOn(...) first. " LOCATION);

        if (op != selectDetails::Inverse ||
            mMem.size() <= wire1)
            throw RTE_LOC;

        mMem[wire1].mUsed = true;

        selectDetails::Gate gate;
        gate.op = op;
        gate.mIn1 = wire1;
        gate.mOut = mMem.size();
        mGates.push_back(gate);


        selectDetails::Mem mem;
        mem.mType = mMem[wire1].mType;
        mem.mGate = &mGates.back();
        mem.mIdx = mMem.size();
        mMem.push_back(mem);

        return mem.mIdx;
    }

    void SelectQuery::addOutput(std::string name, const SelectBundle & column)
    {
        if (mLeftTable == nullptr)
            throw std::runtime_error("call joinOn(...) first. " LOCATION);

        mOutputs.emplace_back(mMem[column.mMemIdx].mIdx, name, -1);
        mMem[column.mMemIdx].mOutputIdx = mOutputs.size() - 1;


        auto maxPos = -1;
        if (isLeftPassthrough(mOutputs.back()) == false)
        {
            for (auto& o : mOutputs)
            {
                if (isLeftPassthrough(o) == false)
                    maxPos = std::max(maxPos, o.mPosition);
            }
            maxPos = maxPos + 1;
        }
        mOutputs.back().mPosition = maxPos;
    }

    void SelectQuery::apply(BetaCircuit & cir, span<BetaBundle> inputs, span<BetaBundle> outputs) const
    {
        BetaLibrary lib;

        std::vector<BetaBundle> mem(mMem.size());
        for (u64 i = 0; i < inputs.size(); ++i)
            mem[mInputs[i].mMemIdx] = inputs[i];


        int outIdx = 0;
        for (auto& gate : mGates)
        {
            //BetaBundle wires(mMem[gate.mOut].mType->getBitCount());

            if (mMem[gate.mOut].isOutput() == false)
            {
                mem[gate.mOut].mWires.resize(mMem[gate.mOut].mType->getBitCount());
                cir.addTempWireBundle(mem[gate.mOut]);
            }
            else
            {
                auto out = mOutputs[mMem[gate.mOut].mOutputIdx].mPosition;
                mem[gate.mOut] = outputs[out];
            }


            switch (gate.op)
            {
            case selectDetails::BitwiseOr:
                lib.int_int_bitwiseOr_build(cir, mem[gate.mIn1], mem[gate.mIn2], mem[gate.mOut]);
                break;
            case selectDetails::BitwiseAnd:
                lib.int_int_bitwiseAnd_build(cir, mem[gate.mIn1], mem[gate.mIn2], mem[gate.mOut]);
                break;
            case selectDetails::LessThan:
                lib.int_int_lt_build(cir, mem[gate.mIn1], mem[gate.mIn2], mem[gate.mOut]);
                break;
            case selectDetails::Inverse:
                lib.int_bitInvert_build(cir, mem[gate.mIn1], mem[gate.mOut]);
                break;
            case selectDetails::Multiply:

                lib.int_int_mult_build(cir, mem[gate.mIn1], mem[gate.mIn2], mem[gate.mOut]);
                break;
            case selectDetails::Add:
                lib.int_int_add_build_do(cir, mem[gate.mIn1], mem[gate.mIn2], mem[gate.mOut]);
                break;
            default:
                throw RTE_LOC;
            }
        }
    }
    bool SelectQuery::isLeftPassthrough(selectDetails::Output output) const
    {
        return
            mMem[output.mMemIdx].isInput() &&
            &mInputs[mMem[output.mMemIdx].mInputIdx].mCol.mTable == mLeftTable;
    }

    bool SelectQuery::isRightPassthrough(selectDetails::Output output) const
    {
        return
            mMem[output.mMemIdx].isInput() &&
            &mInputs[mMem[output.mMemIdx].mInputIdx].mCol.mTable == mRightTable;
    }

    bool SelectQuery::isCircuitInput(selectDetails::Input input) const
    {
        return
            &input.mCol.mTable == mRightTable ||
            mMem[input.mMemIdx].mUsed;
    }

    SelectBundle SelectBundle::operator|(const SelectBundle& r) const
    {
        return SelectBundle{
            mSelect,
            mSelect.addOp(selectDetails::BitwiseOr, mMemIdx, r.mMemIdx) };
    }
    SelectBundle SelectBundle::operator&(const SelectBundle& r) const
    {
        return SelectBundle{
            mSelect,
            mSelect.addOp(selectDetails::BitwiseAnd, mMemIdx, r.mMemIdx) };
    }
    SelectBundle SelectBundle::operator<(const SelectBundle&r) const

    {
        return SelectBundle{
            mSelect,
            mSelect.addOp(selectDetails::LessThan, mMemIdx, r.mMemIdx) };
    }
    SelectBundle SelectBundle::operator!() const
    {
        return SelectBundle{
            mSelect,
            mSelect.addOp(selectDetails::Inverse, mMemIdx) };
    }
    SelectBundle SelectBundle::operator*(const SelectBundle& r) const
    {
        return SelectBundle{
            mSelect,
            mSelect.addOp(selectDetails::Multiply, mMemIdx, r.mMemIdx) };
    }
    SelectBundle SelectBundle::operator+(const SelectBundle& r) const
    {
        return SelectBundle{
            mSelect,
            mSelect.addOp(selectDetails::Add, mMemIdx, r.mMemIdx) };
    }
}

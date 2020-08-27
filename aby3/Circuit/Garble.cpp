#include "aby3/Circuit/Garble.h"


namespace osuCrypto
{

	const std::array<block, 2> Garble::mPublicLabels{ toBlock(0,0), toBlock(~0,~0) };


	bool Garble::isConstLabel(const block & b)
	{
		return eq(mPublicLabels[0], b) || eq(mPublicLabels[1], b);
	}











	// return values
	// 0: 0
	// 1: not in[constB]
	// 2:     in[constB]
	// 3: 1
	u8 subGate(bool constB, bool aa, bool bb, GateType gt)
	{


		u8 g = static_cast<u8>(gt);
		auto g1 = (aa) * (((g & 2) >> 1) | ((g & 8) >> 2))
			+ (1 ^ aa) * ((g & 1) | ((g & 4) >> 1));
		auto g2 = u8(gt) >> (2 * bb) & 3;
		auto ret = ((constB ^ 1) * g1 | constB * g2);
		{
			u8 subgate;

			if (constB) {
				subgate = u8(gt) >> (2 * bb) & 3;
			}
			else {
				u8 g = static_cast<u8>(gt);

				auto val = aa;
				subgate = val
					? ((g & 2) >> 1) | ((g & 8) >> 2)
					: (g & 1) | ((g & 4) >> 1);
			}
			if (subgate != ret)
				throw std::runtime_error(LOCATION);
		}
		return ret;
	}


	block Garble::garbleConstGate(bool constA, bool constB, const std::array<block, 2>& in, const GateType& gt, const block& xorOffset)
	{
		auto aa = PermuteBit(in[0]);
		auto bb = PermuteBit(in[1]);

		if (constA && constB) {
			return Garble::mPublicLabels[GateEval(gt, aa, bb)];
		}
		else {
			auto v = subGate(constB, aa, bb, gt);
			return (Garble::mPublicLabels[v / 3] | (in[constA] & zeroAndAllOne[v > 0])) ^ (zeroAndAllOne[v == 1] & xorOffset);
		}
	}

	block Garble::evaluateConstGate(bool constA, bool constB, const std::array<block, 2>& in, const GateType& gt)
	{
		auto aa = PermuteBit(in[0]);
		auto bb = PermuteBit(in[1]);
		if (constA && constB) {
			return Garble::mPublicLabels[GateEval(gt, aa, bb)];
		}
		else {
			auto v = subGate(constB, aa, bb, gt);
			return _mm_or_si128(Garble::mPublicLabels[v / 3], (in[constA] & zeroAndAllOne[v > 0]));
		}
	}



	void Garble::evaluate(
		const BetaCircuit & cir,
		const span<block>& wires,
		std::array<block, 2>& tweaks,
		const span<GarbledGate<2>>& garbledGates,
		const std::function<bool()>& getAuxilaryBit,
		block* DEBUG_labels)
	{
		u64 i = 0;
		auto garbledGateIter = garbledGates.begin();
		std::array<block, 2> in;
		//std::cout  << IoStream::lock;

		//u64 i = 0;

		block hashs[2], temp[2],
			zeroAndGarbledTable[2][2]
		{ { ZeroBlock,ZeroBlock },{ ZeroBlock,ZeroBlock } };

		for (const auto& gate : cir.mGates)
		{

			auto& gt = gate.mType;




			if (GSL_LIKELY(gt != GateType::a))
			{
				auto a = wires[gate.mInput[0]];
				auto b = wires[gate.mInput[1]];
				auto& c = wires[gate.mOutput];
				auto constA = isConstLabel(a);
				auto constB = isConstLabel(b);
				auto constAB = constA || constB;

				if (GSL_LIKELY(!constAB))
				{

					if (GSL_LIKELY(gt == GateType::Xor || gt == GateType::Nxor))
					{

						if (GSL_LIKELY(neq(a, b)))
						{
							c = a ^ b;
						}
						else
						{
							c = mPublicLabels[getAuxilaryBit()];
						}
#ifndef  NDEBUG
						if (DEBUG_labels) DEBUG_labels[i++] = c;
#endif
					}
					else
					{
						// compute the hashs
						hashs[0] = (a<< 1) ^ tweaks[0];
						hashs[1] = (b<< 1) ^ tweaks[1];
						mAesFixedKey.ecbEncTwoBlocks(hashs, temp);
						hashs[0] = temp[0] ^ hashs[0];
						hashs[1] = temp[1] ^ hashs[1];

						// increment the tweaks
						tweaks[0] = tweaks[0] + OneBlock;
						tweaks[1] = tweaks[1] + OneBlock;

						auto& garbledTable = garbledGateIter++->mGarbledTable;
						zeroAndGarbledTable[1][0] = garbledTable[0];
						zeroAndGarbledTable[1][1] = garbledTable[1] ^ a;

						// compute the output wire label
						c = hashs[0] ^
							hashs[1] ^
							zeroAndGarbledTable[PermuteBit(a)][0] ^
							zeroAndGarbledTable[PermuteBit(b)][1];

						//std::cout  << "e " << i++ << gateToString(gate.mType) << std::endl <<
						//    " gt  " << garbledTable[0] << "  " << garbledTable[1] << std::endl <<
						//    " t   " << tweaks[0] << "  " << tweaks[1] << std::endl <<
						//    " a   " << a << std::endl <<
						//    " b   " << b << std::endl <<
						//    " c   " << c << std::endl;
#ifndef  NDEBUG
						if (DEBUG_labels) DEBUG_labels[i++] = c;
#endif
					}

					//if (i == 2 && gt != GateType::a)
					//{
					//    std::cout << "e a " << a << " b " << b << " " << gateToString(gt) << " " << c << std::endl;

					//}
				}
				else
				{
					in[0] = a; in[1] = b;
					c = evaluateConstGate(constA, constB, in, gt);

#ifndef  NDEBUG
					auto ab = constA ? b : a;
					if (isConstLabel(c) == false &&
						neq(c, ab))
						throw std::runtime_error(LOCATION);
					if (DEBUG_labels) DEBUG_labels[i++] = c;
#endif
				}
			}
			else
			{
				u64 src = gate.mInput[0];
				u64 len = gate.mInput[1];
				u64 dest = gate.mOutput;

				memcpy(&*(wires.begin() + dest), &*(wires.begin() + src), i32(len * sizeof(block)));
			}
		}

		//std::cout  << IoStream::unlock;
		for (u64 i = 0; i < cir.mOutputs.size(); ++i)
		{
			auto& out = cir.mOutputs[i].mWires;

			for (u64 j = 0; j < out.size(); ++j)
			{
				if (cir.mWireFlags[out[j]] == BetaWireFlag::InvWire)
				{
					if (isConstLabel(wires[out[j]]))
						wires[out[j]] = wires[out[j]] ^ mPublicLabels[1];
				}
			}
		}
	}

	void Garble::garble(
		const BetaCircuit& cir,
		const span<block>& wires,
		std::array<block, 2>& tweaks,
		const span<GarbledGate<2>>& gates,
		const std::array<block, 2>& mZeroAndGlobalOffset,
		std::vector<u8>& auxilaryBits,
		block* DEBUG_labels)
	{
		//auto s = DEBUG_labels;
		u64 i = 0;
		auto gateIter = gates.begin();
		//std::cout  << IoStream::lock;
		std::array<block, 2> in;
		//u64 i = 0;
		auto& mGlobalOffset = mZeroAndGlobalOffset[1];
		//std::cout << mZeroAndGlobalOffset[0] << " " << mZeroAndGlobalOffset[1] << std::endl;

		u8 aPermuteBit, bPermuteBit, bAlphaBPermute, cPermuteBit;
		block hash[4], temp[4];

		for (const auto& gate : cir.mGates)
		{


			auto& gt = gate.mType;


			if (GSL_LIKELY(gt != GateType::a))
			{
				auto a = wires[gate.mInput[0]];
				auto b = wires[gate.mInput[1]];
				auto bNot = b ^ mGlobalOffset;

				auto& c = wires[gate.mOutput];
				auto constA = isConstLabel(a);
				auto constB = isConstLabel(b);
				auto constAB = constA || constB;

				if (GSL_LIKELY(!constAB))
				{
					if (GSL_LIKELY(gt == GateType::Xor || gt == GateType::Nxor))
					{
						// is a == b^1
						auto oneEq = eq(a, bNot);
						if (GSL_LIKELY(!(eq(a, b) || oneEq)))
						{
							c = a ^ b ^ mZeroAndGlobalOffset[(u8)gt & 1];
						}
						else
						{
							u8 bit = oneEq ^ ((u8)gt & 1);
							c = mPublicLabels[bit];

							// must tell the evaluator what the bit is.
							auxilaryBits.push_back(bit);
						}
#ifndef  NDEBUG
						if (DEBUG_labels) DEBUG_labels[i++] = c;
#endif
					}
					else
					{
#ifndef  NDEBUG
						Expects(!(gt == GateType::a ||
							gt == GateType::b ||
							gt == GateType::na ||
							gt == GateType::nb ||
							gt == GateType::One ||
							gt == GateType::Zero));
#endif // ! NDEBUG

						// compute the gate modifier variables
						auto& aAlpha = gate.mAAlpha;
						auto& bAlpha = gate.mBAlpha;
						auto& cAlpha = gate.mCAlpha;

						//signal bits of wire 0 of input0 and wire 0 of input1
						aPermuteBit = PermuteBit(a);
						bPermuteBit = PermuteBit(b);
						bAlphaBPermute = bAlpha ^ bPermuteBit;
						cPermuteBit = ((aPermuteBit ^ aAlpha) && (bAlphaBPermute)) ^ cAlpha;

						// compute the hashs of the wires as H(x) = AES_f( x * 2 ^ tweak) ^ (x * 2 ^ tweak)
						hash[0] = (a<< 1) ^ tweaks[0];
						hash[1] = ((a ^ mGlobalOffset)<< 1) ^ tweaks[0];
						hash[2] = (b<< 1) ^ tweaks[1];
						hash[3] = ((bNot)<< 1) ^ tweaks[1];
						mAesFixedKey.ecbEncFourBlocks(hash, temp);
						hash[0] = hash[0] ^ temp[0]; // H( a0 )
						hash[1] = hash[1] ^ temp[1]; // H( a1 )
						hash[2] = hash[2] ^ temp[2]; // H( b0 )
						hash[3] = hash[3] ^ temp[3]; // H( b1 )

													 // increment the tweaks
						tweaks[0] = tweaks[0] + OneBlock;
						tweaks[1] = tweaks[1] + OneBlock;

						// generate the garbled table
						auto& garbledTable = gateIter++->mGarbledTable;

						// compute the table entries
						garbledTable[0] = hash[0] ^ hash[1] ^ mZeroAndGlobalOffset[bAlphaBPermute];
						garbledTable[1] = hash[2] ^ hash[3] ^ a ^ mZeroAndGlobalOffset[aAlpha];

						//std::cout  << "g "<<i<<" " << garbledTable[0] << " " << garbledTable[1] << std::endl;


						// compute the out wire
						c = hash[aPermuteBit] ^
							hash[2 ^ bPermuteBit] ^
							mZeroAndGlobalOffset[cPermuteBit];

						//std::cout  << "g " << i++  << gateToString(gate.mType) << std::endl <<
						//    " gt  " << garbledTable[0] << "  " << garbledTable[1] << std::endl <<
						//    " t   " << tweaks[0] << "  " << tweaks[1] << std::endl <<
						//    " a   " << a << "  " << (a ^ mGlobalOffset) << std::endl <<
						//    " b   " << b << "  " << (b ^ mGlobalOffset) << std::endl <<
						//    " c   " << c << "  " << (c ^ mGlobalOffset) << std::endl;
#ifndef  NDEBUG
						if (DEBUG_labels) DEBUG_labels[i++] = c;
#endif
					}
				}
				else
				{
					auto ab = constA ? b : a;

					in[0] = a; in[1] = b;
					c = garbleConstGate(constA, constB, in, gt, mGlobalOffset);

#ifndef  NDEBUG
					if (isConstLabel(c) == false &&
						neq(c, ab) &&
						neq(c, ab ^ mGlobalOffset))
						throw std::runtime_error(LOCATION);

					if (DEBUG_labels) DEBUG_labels[i++] = c;
#endif

				}

				//if (i == 2 && gt != GateType::a)
				//{
				//    std::cout << "g a " << a << " b " << b << " " << gateToString(gt) << " " << c << " " << (c ^ mGlobalOffset) << std::endl;

				//}
			}
			else
			{
				u64 src = gate.mInput[0];
				u64 len = gate.mInput[1];
				u64 dest = gate.mOutput;

				memcpy(&*(wires.begin() + dest), &*(wires.begin() + src), u32(len * sizeof(block)));
			}

		}

		for (u64 i = 0; i < cir.mOutputs.size(); ++i)
		{
			auto& out = cir.mOutputs[i].mWires;

			for (u64 j = 0; j < out.size(); ++j)
			{
				if (cir.mWireFlags[out[j]] == BetaWireFlag::InvWire)
				{
					if (isConstLabel(wires[out[j]]))
						wires[out[j]] = wires[out[j]] ^ mPublicLabels[1];
					else
						wires[out[j]] = wires[out[j]] ^ mGlobalOffset;
				}
			}
		}
		//std::cout  << IoStream::unlock;

	}


}
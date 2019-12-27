#pragma once
#include <cryptoTools/Circuit/BetaCircuit.h>
#include <vector>
#include <array>
#include <functional>

namespace osuCrypto
{

	class Garble
	{
	public:

		static const std::array<block, 2> mPublicLabels;

		static bool isConstLabel(const block& b);

		//std::queue<CircuitItem> mWorkQueue;
		//boost::lockfree::spsc_queue<CircuitItem*> mWorkQueue;

		static block evaluateConstGate(bool constA, bool constB, const std::array<block, 2>& in, const GateType& gt);
		static block garbleConstGate(bool constA, bool constB, const std::array<block, 2>& in, const GateType& gt, const block& xorOffset);


		static void evaluate(
			const BetaCircuit& cir,
			const span<block>& memory,
			std::array<block, 2>& tweaks,
			const span<GarbledGate<2>>& garbledGates,
			const std::function<bool()>& getAuxilaryBit,
			block* DEBUG_labels = nullptr);


		static void garble(
			const BetaCircuit& cir,
			const span<block>& memory,
			std::array<block, 2>& tweaks,
			const span<GarbledGate<2>>&  garbledGateIter,
			const std::array<block, 2>& zeroAndGlobalOffset,
			std::vector<u8>& auxilaryBits,
			block* DEBUG_labels = nullptr
		);
	};

}

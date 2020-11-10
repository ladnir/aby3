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

#ifdef OC_ENABLE_PUBLIC_WIRE_LABELS
		static const std::array<block, 2> mPublicLabels;
		static bool isConstLabel(const block& b);
		static block evaluateConstGate(bool constA, bool constB, const std::array<block, 2>& in, const GateType& gt);
		static block garbleConstGate(bool constA, bool constB, const std::array<block, 2>& in, const GateType& gt, const block& xorOffset);
#endif // OC_ENABLE_PUBLIC_WIRE_LABELS



		static void evaluate(
			const BetaCircuit& cir,
			span<block> memory,
			span<GarbledGate<2>> garbledGates,
			block& tweaks,
#ifdef OC_ENABLE_PUBLIC_WIRE_LABELS
			const std::function<bool()>& getAuxilaryBit,
#endif
			span<block> DEBUG_labels = {});


		static void garble(
			const BetaCircuit& cir,
			span<block> memory,
			span<GarbledGate<2>>  garbledGates,
			block& tweak,
			block& freeXorOffset,
#ifdef OC_ENABLE_PUBLIC_WIRE_LABELS
			std::vector<u8>& auxilaryBits,
#endif
			span<block> DEBUG_labels = {});
	};

}

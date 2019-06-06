#pragma once
#include <cryptoTools/Circuit/BetaCircuit.h>
#include <cryptoTools/Common/Defines.h>
#include <unordered_map>
#include "aby3/Common/Defines.h"
#include <cryptoTools/Circuit/BetaLibrary.h>

namespace aby3
{
    class CircuitLibrary : public oc::BetaLibrary
    {
    public:
		using BetaCircuit = oc::BetaCircuit;
		using BetaBundle = oc::BetaBundle;

		//enum class Optimized
		//{
		//	Size,
		//	Depth
		//};

  //      std::unordered_map<std::string, std::unique_ptr<BetaCircuit>> mCirMap;


		BetaCircuit* int_Sh3Piecewise_helper(u64 aSize, u64 numThesholds);


		static void int_Sh3Piecewise_build_do(
			BetaCircuit& cd,
			span<const BetaBundle> aa,
			const BetaBundle & b,
			span<const BetaBundle> c);

		static void Preproc_build(BetaCircuit& cd, u64 dec);
		static void argMax_build(BetaCircuit& cd, u64 dec, u64 numArgs);
    };

}
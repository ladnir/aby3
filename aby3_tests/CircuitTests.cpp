#include "CircuitTests.h"
#include "aby3/Circuit/CircuitLibrary.h"
#include "aby3/Engines/Lynx/LynxPiecewise.h"

#include <cryptoTools/Crypto/PRNG.h>
#include <cryptoTools/Common/Log.h>
#include <random>
#include <fstream>

using namespace oc;
using namespace aby3;


extern i64 signExtend(i64 v, u64 b, bool print = false);

void BetaCircuit_int_Sh3Piecewise_Test()
{


	CircuitLibrary lib;


	PRNG prng(ZeroBlock);
	u64 tries = 200;

	u64 numThresholds = 3;
	u64 decimal = 16;
	u64 size = 64;

	auto* cir = lib.int_Sh3Piecewise_helper(size, numThresholds);

	//u64 max = 1000;
	//std::exponential_distribution<> d(1);
	std::vector<double> dd;
	std::normal_distribution<> d(0.0, 3);

	for (u64 i = 0; i < tries; ++i)
	{
		std::vector<BitVector> in(numThresholds + 1), out(numThresholds + 1);

		dd.clear();
		for(u64 t =0 ; t < numThresholds; ++t)
			dd.emplace_back(d(prng));

		std::sort(dd.rbegin(), dd.rend());

		i64 b = prng.get<i64>();
		u64 exp = numThresholds;

		for (u64 t = 0; t < numThresholds; ++t)
		{
			if (exp == numThresholds && dd[t] <= 0)
				exp = t;

			i64 val = static_cast<i64>(dd[t] * (1 << decimal));

			val -= b;

			in[t].append((u8*)& val, size);

			//std::cout<< "v["<<t<<"]  " << dd[t] << std::endl;
		}

		in[numThresholds].append((u8*)&b, size);

		for (auto& o : out)
			o.resize(1);

		cir->evaluate(in, out);

		for (u64 t = 0; t < numThresholds + 1; ++t)
		{
			//std::cout << "o[" << t << "]  " << out[t][0] << std::endl;
			if (out[t][0] != (t == exp))
			{
				throw std::runtime_error(LOCATION);
			}
		}

	}
}

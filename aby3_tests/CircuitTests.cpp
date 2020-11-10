#include "CircuitTests.h"
#include "aby3/Circuit/CircuitLibrary.h"
#include "aby3/Circuit/Garble.h"

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
		for (u64 t = 0; t < numThresholds; ++t)
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

			in[t].append((u8*)&val, size);

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


void garble_Test()
{


	CircuitLibrary lib;
	u64 bitCount = 64;
	auto cir = lib.int_int_mult(bitCount, bitCount, bitCount);


	// garbler
	// ----------------------------------------------------------

	Garble garb;

	PRNG prng(sysRandomSeed());
	std::vector<block> zeroWireLabels(cir->mWireCount);

	// set the free xor key.
	block freeXorOffset = prng.get();

	// the tweak is important. Each time you set freeXorOffset, 
	// you should intialize the tweak to zero. After that, the 
	// garble(...), evalaute(...) functions will update this value. 
	// This allows you to keep the same freeXorOffset between 
	// several calls to garble(...) while maintaining security. 
	block gTweak = oc::ZeroBlock;

	// randomly pick the zero labels for the inputs
	for (auto in : cir->mInputs)
	{
		for (auto i : in.mWires)
		{
			zeroWireLabels[i] = prng.get();
		}
	}

	// pick the garbler's input values.
	BitVector garbPlainInput(bitCount);
	garbPlainInput.randomize(prng);

	// construct the garbler's garbled input
	std::vector<block> garblersInput(cir->mInputs[0].size());
	for (u64 i = 0; i < cir->mInputs[0].mWires.size(); ++i)
	{
		garblersInput[i] = zeroWireLabels[cir->mInputs[0].mWires[i]];
		if (garbPlainInput[i])
			garblersInput[i] = garblersInput[i] ^ freeXorOffset;
	}

	// construct the possible wire labels that evalutor will pick from.
	std::vector<std::array<block,2>> evalsLabels(cir->mInputs[1].size());
	for (u64 i = 0; i < cir->mInputs[1].mWires.size(); ++i)
	{
		evalsLabels[i][0] = zeroWireLabels[cir->mInputs[1].mWires[i]];
		evalsLabels[i][1] = zeroWireLabels[cir->mInputs[1].mWires[i]] ^ freeXorOffset;
	}

	// somehow to the OTs one the evaluator's input.


	// the garbled circuit.
	std::vector<GarbledGate<2>> garbledGates(cir->mNonlinearGateCount);

	// This updates zeroWireLabels, garbledGates, gTweak.
#ifdef GARBLE_DEBUG
	std::vector<block> G_DEBUG_LABELS(cir->mGates.size());
	garb.garble(*cir, zeroWireLabels, garbledGates, gTweak, freeXorOffset, G_DEBUG_LABELS);
#else
	garb.garble(*cir, zeroWireLabels, garbledGates, gTweak, freeXorOffset);
#endif


	// optionally construct the decoding information.
	BitVector decoding(bitCount);
	for (u64 i = 0; i < cir->mOutputs[0].size(); ++i)
	{
		decoding[i] = PermuteBit(zeroWireLabels[cir->mOutputs[0].mWires[i]]);
	}

	// send garbledGates,garblersInput to the evaluator...

	// Optionally send the decoding.





	// evaluator
	// -------------------------------------------------------------------
	Garble eval;

	std::vector<block> activeWireLabels(cir->mWireCount);

	// copy in the garbler's input labels.
	for (u64 i = 0; i < cir->mInputs[0].mWires.size(); ++i)
	{
		activeWireLabels[cir->mInputs[0].mWires[i]] = garblersInput[i];
	}


	// pick some random inputs for the evalutor.
	BitVector evalsPlainInput(bitCount);
	evalsPlainInput.randomize(prng);

	// perform OT to get the evalutor's input. This is fakes.
	std::vector<block> evalsInput(cir->mInputs[1].size());
	for (u64 i = 0; i < cir->mInputs[1].mWires.size(); ++i)
	{
		activeWireLabels[cir->mInputs[1].mWires[i]] = evalsLabels[i][evalsPlainInput[i]];
	}

	// evaluator also needs to set their own tweak.
	block eTweak = oc::ZeroBlock;

#ifdef GARBLE_DEBUG
	std::vector<block> E_DEBUG_LABELS(cir->mGates.size());
	garb.evaluate(*cir, activeWireLabels, garbledGates, eTweak, E_DEBUG_LABELS);
#else
	garb.evaluate(*cir, activeWireLabels, garbledGates, eTweak);
#endif 

	// decode the output.
	BitVector output(bitCount);
	for (u64 i = 0; i < cir->mOutputs[0].size(); ++i)
	{
		output[i] = decoding[i] ^ PermuteBit(activeWireLabels[cir->mOutputs[0].mWires[i]]);
	}


	// test the result
	// ------------------------------------------------------------

#ifdef GARBLE_DEBUG
	for (u64 i = 0; i < E_DEBUG_LABELS.size(); ++i)
	{
		if (E_DEBUG_LABELS[i] != G_DEBUG_LABELS[i] &&
			E_DEBUG_LABELS[i] != (G_DEBUG_LABELS[i] ^ freeXorOffset))
		{
			throw std::runtime_error(LOCATION);
		}
	}
#endif

	std::vector<BitVector> plainInputs{ garbPlainInput, evalsPlainInput };
	std::vector<BitVector> plainOutputs(1);
	plainOutputs[0].resize(bitCount);

	cir->evaluate(plainInputs, plainOutputs);

	if (plainOutputs[0] != output)
	{
		std::cout << "exp " << plainOutputs[0] << std::endl;
		std::cout << "act " << output << std::endl;

		throw std::runtime_error(LOCATION);
	}
}
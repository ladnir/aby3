#include "CircuitTests.h"
#include "aby3/Circuit/BetaLibrary.h"
#include "aby3/Engines/Lynx/LynxPiecewise.h"

#include <cryptoTools/Crypto/PRNG.h>
#include <cryptoTools/Common/Log.h>
#include <random>

using namespace oc;


i64 signExtend(i64 v, u64 b, bool print = false)
{

	if (b != 64)
	{

		i64 loc = (i64(1) << (b - 1));
		i64 sign = v & loc;

		if (sign)
		{
			i64 mask = i64(-1) << (b);
			auto ret = v | mask;
			if (print)
			{

				std::cout << "sign: " << BitVector((u8*)&sign, 64) << std::endl;;
				std::cout << "mask: " << BitVector((u8*)&mask, 64) << std::endl;;
				std::cout << "v   : " << BitVector((u8*)&v, 64) << std::endl;;
				std::cout << "ret : " << BitVector((u8*)&ret, 64) << std::endl;;

			}
			return ret;
		}
		else
		{
			i64 mask = (i64(1) << b) - 1;
			auto ret = v & mask;
			if (print)
			{

				std::cout << "sign: " << BitVector((u8*)&loc, 64) << std::endl;;
				std::cout << "mask: " << BitVector((u8*)&mask, 64) << std::endl;;
				std::cout << "v   : " << BitVector((u8*)&v, 64) << std::endl;;
				std::cout << "ret : " << BitVector((u8*)&ret, 64) << std::endl;;

			}
			return ret;
		}
	}

	return v;
}

void BetaCircuit_int_Adder_Test()
{
	setThreadName("CP_Test_Thread");


	BetaLibrary lib;


	PRNG prng(ZeroBlock);
	u64 tries = 1000;


	u64 size = 64;

	auto* msb = lib.int_int_add_msb(size);
	auto* cir = lib.int_int_add(size, size, size, BetaLibrary::Optimized::Depth);

	//msb->levelByAndDepth();
	cir->levelByAndDepth();

	for (u64 i = 0; i < tries; ++i)
	{
		i64 a = signExtend(prng.get<i64>(), size);
		i64 b = signExtend(prng.get<i64>(), size);
		i64 c = signExtend((a + b), size);

		std::vector<BitVector> inputs(2), output1(1), output2(1), output3(1);
		inputs[0].append((u8*)&a, size);
		inputs[1].append((u8*)&b, size);
		output1[0].resize(size);
		output2[0].resize(1);
		output3[0].resize(size);

		cir->evaluate(inputs, output1);
		//cir->levelEvaluate(inputs, output3);
		msb->evaluate(inputs, output2);

		i64 cc = 0;
		memcpy(&cc, output1[0].data(), output1[0].sizeBytes());

		cc = signExtend(cc, size);
		if (cc != c)
		{
			std::cout << "i " << i << std::endl;

			BitVector cExp;
			cExp.append((u8*)&c, size);
			std::cout << "a  : " << inputs[0] << std::endl;
			std::cout << "b  : " << inputs[1] << std::endl;
			std::cout << "exp: " << cExp << std::endl;
			std::cout << "act: " << output1[0] << std::endl;

			throw std::runtime_error(LOCATION);
		}


		if (output2.back().back() != output1.back().back())
		{
			std::cout << "exp: " << output1.back().back() << std::endl;
			std::cout << "act: " << output2.back().back() << std::endl;
			throw std::runtime_error(LOCATION);
		}

		//if (output3.back().back() != output1.back().back())
		//{
		//	std::cout << "exp: " << output1.back().back() << std::endl;
		//	std::cout << "act: " << output3.back().back() << std::endl;
		//	throw std::runtime_error(LOCATION);
		//}
	}
}

u8 msb(i64 v)
{
	return (v >> 63) & 1;
}

void BetaCircuit_int_piecewise_Test()
{


	BetaLibrary lib;


	PRNG prng(ZeroBlock);
	u64 tries = 200;

	u64 numThresholds = 3;
	u64 decimal = 16;
	u64 size = 64;

	auto* cir = lib.int_Piecewise_helper(size, numThresholds);

	//msb->levelByAndDepth();
	//cir->levelByAndDepth();

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

			i64 val = dd[t] * (1 << decimal);

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

		/*for(u64 t = 0; t < numThresholds; ++t)*/


		//auto aa = (prng.getBit() * 2 - 1) * std::min<double>(d(prng), max);

		//auto bb1 = (aa  + 1.0 / 2) < 0;
		//auto bb2 = (aa  - 1.0 / 2) < 0;
		//auto bb3 = !bb2;
		//auto bb4 = (bb2 && !bb1);

		//std::vector<BitVector> in(4), out(2);


		//const auto one = (1 << (decimal - 1));
		//const auto half = one / 2;
		//i64 a = aa  * one;

		//auto a_plus = a + half;
		//auto a_minus = a - half;

		//auto a1_plus = prng.get<i64>();;
		//auto a2_plus = a_plus - a1_plus;

		//auto a1_minus = prng.get<i64>();;
		//auto a2_minus = a_minus - a1_minus;

		//auto expB1 = msb(a_plus);
		//auto expB2 = msb(a_minus);
		//auto expB3 = expB2 ^ 1;
		//auto expB4 = (expB2 & (expB1 ^ 1));

		//if (bb1 != expB1)
		//	throw std::runtime_error(LOCATION);
		//if (bb2 != expB2) throw std::runtime_error(LOCATION);
		//if (bb3 != expB3) throw std::runtime_error(LOCATION);
		//if (bb4 != expB4) throw std::runtime_error(LOCATION);

		//std::vector<BitVector> inputs(4), output1(2);
		//inputs[0].append((u8*)&a1_plus, size);
		//inputs[1].append((u8*)&a1_minus, size);
		//inputs[2].append((u8*)&a2_plus, size);
		//inputs[3].append((u8*)&a2_minus, size);

		//output1[0].resize(1);
		//output1[1].resize(1);

		//cir->evaluate(inputs, output1);

		//i64 b3 = 0, b4 = 0;
		//memcpy(&b3, output1[0].data(), output1[0].sizeBytes());
		//memcpy(&b4, output1[1].data(), output1[1].sizeBytes());


		//if (b3 != expB3)
		//{
		//	std::cout << "b3 i " << i << std::endl;


		//	std::cout << "a  : " << inputs[0] << std::endl;
		//	std::cout << "b  : " << inputs[1] << std::endl;
		//	std::cout << "exp: " << expB3 << std::endl;
		//	std::cout << "act: " << b3 << std::endl;

		//	//throw std::runtime_error(LOCATION);
		//}

		//if (b4 != expB4)
		//{
		//	std::cout << "b4 i " << i << std::endl;


		//	std::cout << "a  : " << inputs[0] << std::endl;
		//	std::cout << "b  : " << inputs[1] << std::endl;
		//	std::cout << "exp: " << expB4 << std::endl;
		//	std::cout << "act: " << b4 << std::endl;

		//	//throw std::runtime_error(LOCATION);
		//}
	}
}

//
//void BetaCircuit_toFormula_Test()
//{
//	BetaCircuit cd;
//
//	std::cout << std::endl;
//	u64 size = 64;
//	BetaBundle aa(size), cc(1), tt(100);
//
//	cd.addInputBundle(aa);
//	cd.addOutputBundle(cc);
//	cd.addTempWireBundle(tt);
//
//	auto& a = aa.mWires;
//	auto& c = cc.mWires;
//	auto& t = tt.mWires;
//
//
//	for (i64 j = size - 1; j > 0; --j)
//	{
//		auto& x = [&]() {return j == size - 1 ? a : t; }();
//		auto& y = [&]() {return j == 1 ? c : t; }();
//
//		for (i64 i = 0; i < j; ++i)
//		{
//			cd.addGate(x[i], x[i + 1], GateType::And, y[i]);
//		}
//	}
//
//	cd.toFormula();
//}
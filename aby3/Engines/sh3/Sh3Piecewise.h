#pragma once

#include "aby3/Engines/sh3/Sh3Runtime.h"
#include "aby3/Engines/sh3/Sh3Encryptor.h"
#include "aby3/Engines/sh3/Sh3Evaluator.h"
#include "aby3/Engines/sh3/Sh3BinaryEvaluator.h"
#include <aby3/Engines/sh3/Sh3Types.h>
#include <aby3/Engines/sh3/Sh3FixedPoint.h>
#include <cryptoTools/Common/Matrix.h>
#include <aby3/Circuit/CircuitLibrary.h>

namespace aby3
{

	class Sh3Piecewise
	{
	public:

		struct Coef
		{
			Coef() = default;
			Coef(const int& i) { *this = i64(i); }
			Coef(const i64& i) { *this = i; }
			Coef(const double& d) { *this = d; }

			bool mIsInteger = false;
			i64 mInt = 0;
			double mDouble = 0;

			void operator=(const i64& i)
			{
				mIsInteger = true;
				mInt = i;
			}
			void operator=(const int& i)
			{
				mIsInteger = true;
				mInt = i;
			}

			void operator=(const double& d)
			{
				mIsInteger = false;
				mDouble = d;
			}

			double getDouble(const u64& decPlace)
			{
				if (mIsInteger)
					return static_cast<double>(mInt);
				else
					return mDouble;
			}

			i64 getFixedPoint(const u64& decPlace)
			{
				if (mIsInteger)
					return mInt * (1ull << decPlace);
				else
					return static_cast<i64>(mDouble * (1ull << decPlace));
			}

			i64 getInteger()
			{
				if (mIsInteger == false)
					throw std::runtime_error(LOCATION);

				return mInt;
			}
		};




		std::vector<Coef> mThresholds;
		std::vector<std::vector<Coef>> mCoefficients;

		void eval(
			const eMatrix<double> & inputs,
			eMatrix<double>& outputs,
			u64 D,
			bool print = false);

		template<Decimal D>
		void eval(
			const f64Matrix<D>& inputs,
			f64Matrix<D>& outputs,
			bool print = false)
		{
			eval(inputs.i64Cast(), outputs.i64Cast(), D, print);
		}

		void eval(
			const i64Matrix& inputs,
			i64Matrix& outputs,
			u64 D,
			bool print = false);

		Sh3Task eval(
			Sh3Task dep,
			const si64Matrix& input,
			si64Matrix& output,
			u64 D,
			Sh3Evaluator& evaluator,
			bool print = false);


		template<Decimal D>
		Sh3Task eval(
			Sh3Task dep,
			const sf64Matrix<D>& inputs,
			sf64Matrix<D>& outputs,
			Sh3Evaluator& evaluator,
			bool print = false)
		{
			return eval(dep, inputs.i64Cast(), outputs.i64Cast(), D, evaluator, print);
		}

		std::vector<sbMatrix> mInputRegions;
		std::vector<sbMatrix> circuitInput0;
		sbMatrix circuitInput1;
		Sh3BinaryEvaluator binEng;
		CircuitLibrary lib;
		std::vector<si64Matrix>functionOutputs;



		Sh3Encryptor DebugEnc;
		Sh3Runtime DebugRt;
		//void getInputThresholds(const LynxEngine::Matrix & inputs, LynxEngine & eng, LynxEngine::Matrix &inputThresholds);

		Sh3Task getInputRegions(
			const si64Matrix& inputThresholds, u64 decimal,
			CommPkg& comm, Sh3Task& task,
			bool print = false);

		oc::Matrix<u8> getInputRegions(const i64Matrix& inputs,u64);


		Sh3Task getFunctionValues(
			const si64Matrix& inputs,
			CommPkg& comm,
			Sh3Task self,
			u64 decimal,
			span<si64Matrix> outputs);
	};

}
#pragma once

#include "aby3/Lagecy/LynxEngine.h"
#include <exception>
#include <cryptoTools/Common/Matrix.h>

namespace Lynx
{

	class Piecewise
	{
	public:
		//LynxEngine& mEng;

		struct Coef
		{
			Coef() = default;
			Coef(const int& i) { *this = i64(i); }
			Coef(const i64& i) { *this = i; }
			Coef(const double& d) { *this = d; }

			bool mIsInteger;
			i64 mInt;
			double mDouble;

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

			Lynx::Engine::Word getFixedPoint(const u64& decPlace)
			{
				if (mIsInteger)
					return mInt * (1ull << decPlace);
				else
					return static_cast<Lynx::Engine::Word>(mDouble * (1ull << decPlace));
			}

			Lynx::Engine::Word getInteger()
			{
				if (mIsInteger == false)
					throw std::runtime_error(LOCATION);

				return mInt;
			}
		};

		u64 mDecimal;

		Piecewise() = default;
		Piecewise(u64 decimal);



		std::vector<Coef> mThresholds;
		std::vector<std::vector<Coef>> mCoefficients;

		void eval(
			const Lynx::Engine::value_type_matrix& inputs,
			Lynx::Engine::value_type_matrix& outputs,
			bool print = false);

		void eval(
			const Lynx::word_matrix& inputs,
			Lynx::word_matrix& outputs,
			bool print = false);


		void eval(
			Lynx::Engine& eng,
			const Lynx::Engine::Matrix& input,
			Lynx::Engine::Matrix& output,
			bool print = false);

		//void getInputThresholds(const LynxEngine::Matrix & inputs, LynxEngine & eng, LynxEngine::Matrix &inputThresholds);
		void getInputRegions(
			const Lynx::Engine::Matrix & inputThresholds,
			Lynx::Engine & eng,
			span<Lynx::Engine::Matrix> inputRegions, 
            bool print = false);

        oc::Matrix<u8> getInputRegions(const word_matrix& inputs);


		void getFunctionValues(
			const Lynx::Engine::Matrix & inputs,
			Lynx::Engine & eng,
			span<Lynx::Engine::Matrix> outputs);
	};

}
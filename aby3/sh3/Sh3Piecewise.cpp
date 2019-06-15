#include "Sh3Piecewise.h"


#include "cryptoTools/Common/Matrix.h"
#include "cryptoTools/Common/Log.h"
//#include "aby3/Lagecy/Lynx/LynxBinaryEngine.h"
#include "aby3/sh3/Sh3Evaluator.h"
#include "aby3/sh3/Sh3Encryptor.h"

namespace aby3
{


	i64 mul(i64 op1, i64 op2, i64 dec)
	{

		uint64_t u1 = (op1 & 0xffffffff);
		uint64_t v1 = (op2 & 0xffffffff);
		uint64_t t = (u1 * v1);
		uint64_t w3 = (t & 0xffffffff);
		uint64_t k = (t >> 32);

		op1 >>= 32;
		t = (op1 * v1) + k;
		k = (t & 0xffffffff);
		uint64_t w1 = (t >> 32);

		op2 >>= 32;
		t = (u1 * op2) + k;
		k = (t >> 32);

		auto hi = (op1 * op2) + w1 + k;
		auto lo = (t << 32) + w3;

		return  (lo >> dec) + (hi << (64 - dec));
	}


	//void LynxSh3Piecewise::eval(LynxEngine::value_type input, LynxEngine::i64 & out)
	//{
	//	eval({ &input, 1 }, {&out , 1});
	//}

	void Sh3Piecewise::eval(
		const eMatrix<double> & inputs,
		eMatrix<double> & outputs,
		u64 D,
		bool print)
	{
		i64Matrix in, out;
		in.resizeLike(inputs);
		out.resizeLike(inputs);
		for (u64 i = 0; i < in.size(); ++i)
			in(i) = static_cast<i64>(inputs(i) * (1ull << D));

		eval(in, out, D, print);

		for (u64 i = 0; i < in.size(); ++i)
			outputs(i) = out(i) / double(1ull << D);
	}

	oc::Matrix<u8> Sh3Piecewise::getInputRegions(
		const i64Matrix & inputs,
		u64 decimal)
	{
		oc::Matrix<u8> inputRegions(inputs.rows(), mThresholds.size() + 1);
		std::vector<u8> thresholds(mThresholds.size());
		for (u64 i = 0; i < inputs.rows(); ++i)
		{
			auto& in = inputs(i);

			for (u64 t = 0; t < mThresholds.size(); ++t)
			{
				thresholds[t] = in < mThresholds[t].getFixedPoint(decimal) ? 1 : 0;
				//std::cout << in << " < " << mThresholds[t].getFixedPoint(decimal) << " -> " << thresholds[t] << std::endl;
			}

			inputRegions(i, 0) = thresholds[0];
			for (u64 t = 1; t < mThresholds.size(); ++t)
			{
				inputRegions(i, t) = (1 ^ thresholds[t - 1]) * thresholds[t];
			}

			inputRegions(i, mThresholds.size()) = (1 ^ thresholds.back());
		}

		return inputRegions;
	}

	void Sh3Piecewise::eval(
		const i64Matrix & inputs,
		i64Matrix & outputs,
		u64 decimal,
		bool print)
	{

		if (inputs.cols() != 1 || outputs.cols() != 1)
			throw std::runtime_error(LOCATION);

		if (outputs.size() != inputs.size())
			throw std::runtime_error(LOCATION);

		if (mThresholds.size() == 0)
			throw std::runtime_error(LOCATION);

		if (mCoefficients.size() != mThresholds.size() + 1)
			throw std::runtime_error(LOCATION);

		if (print)
		{
			std::cout << "thresholds: ";
			for (size_t i = 0; i < mThresholds.size(); i++)
			{
				std::cout << mThresholds[i].getFixedPoint(decimal) << "  ";
			}
			std::cout << std::endl;
		}


		//auto decShift = std::pow(2.0, decimal);
		//Matrix<bool> inputThresholds(inputs.size(), mThresholds.size());
		oc::Matrix<u8> inputRegions = getInputRegions(inputs, decimal);

		if (print)
		{
			for (u64 i = 0; i < inputs.rows(); ++i)
			{
				std::cout << i << ":  ";
				for (u64 t = 0; t < mCoefficients.size(); ++t)
				{
					std::cout << u32(inputRegions(i, t)) << ", ";
				}

				std::cout << "  ~~  " << inputs(i) << " (" << (inputs(i) / double(1ull << decimal)) << ")  ~~  ";
				for (u64 t = 0; t < mThresholds.size(); ++t)
				{
					std::cout << mThresholds[t].getFixedPoint(decimal) << ", ";
				}

				std::cout << std::endl;
			}
		}

		for (u64 i = 0; i < inputs.rows(); ++i)
		{
			auto in = inputs(i);
			auto& out = outputs(i);
			out = 0;

			if (print)
				std::cout << i << ":  " << in << "\n";

			for (u64 t = 0; t < mCoefficients.size(); ++t)
			{
				i64 ft = 0;
				i64 inPower = (1ll << decimal);
				if (print)
					std::cout << "   " << int(inputRegions(i, t)) << " * (";

				for (u64 c = 0; c < mCoefficients[t].size(); ++c)
				{
					auto coef = mCoefficients[t][c].getFixedPoint(decimal);

					if (print)
						std::cout << coef << " * " << inPower << " + ";

					ft += mul(coef, inPower, decimal);

					inPower = mul(in, inPower, decimal);
				}


				if (print)
				{
					std::cout << ")" << std::endl;
				}

				out += inputRegions(i, t) * ft;
			}
		}

	}
#define UPDATE 

	Sh3Task Sh3Piecewise::eval(
		Sh3Task dep,
		const si64Matrix & inputs,
		si64Matrix & outputs,
		u64 D,
		Sh3Evaluator& evaluator,
		bool print)
	{
		UPDATE;
		//auto ret = dep.then([&inputs, &outputs, D, &evaluator, print, this](CommPkg & comm, Sh3Task self) {

			if (inputs.cols() != 1 || outputs.cols() != 1)
				throw std::runtime_error(LOCATION);

			if (outputs.size() != inputs.size())
				throw std::runtime_error(LOCATION);

			if (mThresholds.size() == 0)
				throw std::runtime_error(LOCATION);

			if (mCoefficients.size() != mThresholds.size() + 1)
				throw std::runtime_error(LOCATION);


			mInputRegions.resize(mCoefficients.size());
			UPDATE;
			//auto rangeTestTask = getInputRegions(inputs, D, comm, self, print);
			auto rangeTestTask = getInputRegions(inputs, D, dep.getRuntime().mComm, dep, print);



			//TODO("!!!!!!!!!!!!!! REMOVE THIS !!!!!!!!!!!!!! ");
//#define Sh3Piecewise_DEBUG
#ifdef Sh3Piecewise_DEBUG
			//rangeTestTask.then([&](Sh3Task self){
			
					i64Matrix plain_inputs(inputs.rows(), inputs.cols());
					DebugEnc.revealAll(DebugRt.noDependencies(), inputs, plain_inputs).get();

					i64Matrix plain_outputs(outputs.rows(), outputs.cols());
					eval(plain_inputs, plain_outputs, D);

					auto true_inputRegions = getInputRegions(plain_inputs, D);

					std::vector<i64Matrix> inputRegions__(mCoefficients.size());
					for (u64 i = 0; i < mCoefficients.size(); ++i)
					{
						//inputRegions__[i] = 
						inputRegions__[i].resize(mInputRegions[i].rows(), 1);
						DebugEnc.revealAll(DebugRt.noDependencies(), mInputRegions[i], inputRegions__[i]).get();
						//std::cout << i << std::endl << inputRegions__[i] << std::endl << std::endl;
					}

					for (u64 i = 0; i < inputs.size(); ++i)
					{
						oc::ostreamLock oo(std::cout);

						if (print) oo << i << ":  ";

						for (u64 t = 0; t < mInputRegions.size(); ++t)
						{
							if (print) oo << inputRegions__[t](i) << ", ";


							if (true_inputRegions(i, t) != inputRegions__[t](i))
							{
								oo << "bad input region " << i << " " << t << std::endl;
							}
						}
						if (print) oo << std::endl;
					}
		UPDATE;
			//	}
			//, "debug-print");
#endif
		UPDATE;
		//	}
		//,"eval-part1").getClosure("eval-part1-closure");


		UPDATE;
		//ret.get();

		UPDATE;
		//ret = ret.then([&inputs, &outputs, D, &evaluator, print, this](CommPkg & comm, Sh3Task self) {

			//rangeTestTask.get();


			functionOutputs.resize(mCoefficients.size());
			UPDATE;
			//auto fxEvalTask = getFunctionValues(inputs, comm, self, D, functionOutputs);
			auto fxEvalTask = getFunctionValues(inputs, dep.getRuntime().mComm, dep, D, functionOutputs);

			//= dep.then(fxEvalRoutine).getClosure();
			//auto combineTask = (rangeTestTask && fxEvalTask);
			auto combineTask = (fxEvalTask);


			//std::vector<CompletionHandle> handles(mCoefficients.size());

			//auto multRoutine = [this, &outputs, &inputs, D](CommPkg & comm, Sh3Task self)
			//{
			outputs.mShares[0].setZero();
			outputs.mShares[1].setZero();
			
			auto multTask = combineTask;

			for (u64 c = 0; c < mCoefficients.size(); ++c)
			{
				if (mCoefficients[c].size())
				{
					if (mCoefficients[c].size() > 1)
					{
						// multiplication by a private value
						//multTask = multTask && 
						evaluator.asyncMul(
							combineTask,
							functionOutputs[c],
							mInputRegions[c],
							functionOutputs[c]).then([&outputs, this, c](Sh3Task self)
								{
									outputs = outputs + functionOutputs[c];
								}
						).get();


						//handles[c].get();
					}
					else
					{
						// multiplication by a public constant
						functionOutputs[c].resize(inputs.rows(), inputs.cols());

						//multTask = multTask && 
						evaluator.asyncMul(
							combineTask,
							mCoefficients[c][0].getFixedPoint(D),
							mInputRegions[c],
							functionOutputs[c]).then([&outputs, this, c](Sh3Task self)
								{
									outputs = outputs + functionOutputs[c];
								}
						).get();
						//handles[c].get();
					} 

					//auto pub = eng.reconstructShare(functionOutputs[c]);
#ifdef Sh3Piecewise_DEBUG
					if (print)
					{
						i64Matrix plain_inputs(inputs.rows(), inputs.cols());
						DebugEnc.revealAll(DebugRt.noDependencies(), inputs, plain_inputs).get();

						std::cout << "coef[" << c << "] = ";
						if (mCoefficients[c].size() > 1)
							std::cout << mCoefficients[c][1].getInteger() << " " << plain_inputs(0) << " + ";

						if (mCoefficients[c].size() > 0)
							std::cout << mCoefficients[c][0].getFixedPoint(D) << std::endl;
					}
#endif
				}
			}



			//auto combineRoutine = [this, &outputs, print](Sh3Task self)
			//{

			//	for (u64 c = 0; c < mCoefficients.size(); ++c)
			//	{
			//		if (handles[c].mGet)
			//		{
			//			handles[c].get();
			//			outputs = outputs + functionOutputs[c];

			//			if (print && c == 1)
			//				oc::ostreamLock(std::cout) << "f" << self.getRuntime().mPartyIdx << " c=" << c << ": "
			//				<< functionOutputs[c].mShares[0](0) << " "
			//				<< functionOutputs[c].mShares[1](0) << std::endl;
			//		}
			//	}
			//};
			//char cccc = 0;

		UPDATE;
		//	}
		//).getClosure();

		//ret.get();

		//return ret;
			return dep;
	}


	Sh3Task Sh3Piecewise::getInputRegions(
		const si64Matrix & inputs, u64 decimal,
		CommPkg & comm, Sh3Task & self,
		bool print)
	{

		// First we want to transform the input into a more efficient prepresentation. 
		// Currently we have x0,x1,x2 which sum to the input. We are going to add
		// x0 and x1 together so that we have a 2-out-of-2 sharing of the input. Party 0 
		// who holds both of these shares will do the addition. After this, we are 
		// going to input these two shares into a circuit to compute the threshold bits

		// This is a bit stange but for each theshold computation, input0 will
		// be different but input1 will be the same. This is a small optimization.
		circuitInput0.resize(mThresholds.size());
		circuitInput1.resize(inputs.size(), 64);
		for (auto& c : circuitInput0) c.resize(inputs.size(), 64);


		// Lets us construct two sharings (x0+x1) and x2. The former will be a normal sharing
		// and input by party 0. This will be stored in the first matrix of circuitInput0.
		// The second shring will be special in that the sharing [x2]=(0,x2,0). In general this
		// can be insecure. However, in this case its ok.

		auto pIdx = self.getRuntime().mPartyIdx;
		auto & c0s0 = circuitInput0[0].mShares[0];
		auto & c0s1 = circuitInput0[0].mShares[1];
		switch (pIdx)
		{
		case 0:
		{

			// have party 0 add the two shares together.
			c0s0.resize(inputs.rows(), inputs.cols());

			for (u64 i = 0; i < c0s0.size(); ++i)
				c0s0(i) = inputs.mShares[0](i)
				+ inputs.mShares[1](i);

			TODO("Radomize the shares....");
			// We need to randomize the share
			//for (u64 i = 0; i < circuitInput0[0].size(); ++i)
			//	c0s0(i) += eng.getBinaryShare();

			circuitInput1.mShares[0].setZero();
			circuitInput1.mShares[1].setZero();
			break;
		}
		case 1:
			circuitInput1.mShares[0].resize(inputs.rows(), inputs.cols());
			circuitInput1.mShares[1].setZero();
			memcpy(circuitInput1.mShares[0].data(), inputs.mShares[0].data(), inputs.size() * sizeof(i64));
			//= std::move(inputs.mShares[0]);

			TODO("Radomize the shares....");
			c0s0.setZero();
			//for (u64 i = 0; i < inputs.size(); ++i)
			//	c0s0(i) += eng.getBinaryShare();
			break;
		default:// case 2:
			circuitInput1.mShares[0].setZero();
			circuitInput1.mShares[1].resize(inputs.rows(), inputs.cols());// = std::move(inputs.mShares[1]);
			memcpy(circuitInput1.mShares[1].data(), inputs.mShares[1].data(), inputs.size() * sizeof(i64));

			TODO("Radomize the shares....");
			c0s0.setZero();
			//for (u64 i = 0; i < inputs.size(); ++i)
			//	c0s0(i) += eng.getBinaryShare();
		}

		comm.mNext.asyncSend(c0s0.data(), c0s0.size());
		auto fu = comm.mPrev.asyncRecv(c0s1.data(), c0s1.size());

		UPDATE;
		//auto ret = self.then([&inputs, pIdx, this, decimal, fu = std::move(fu)](CommPkg & comm, Sh3Task self) mutable {

			fu.get();
			// Now we need to augment the circuitInput0 shares for each of the thresholds.
			// Currently we have the two shares (x0+x1), x2 and we want the pairs
			//   (x0+x1)-t0, x2
			//    ...
			//   (x0+x1)-tn, x2
			// We will then add all of these pairs together to get a binary sharing of
			//   x - t0
			//    ...
			//   x - tn
			// Since we are only interested in the sign of these differences, we only 
			// compute the MSB of the difference. Also, notice that x2 is shared between
			// all of the shares. As such we input have one circuitInput1=x2 while we have 
			// n circuitInputs0.

			for (u64 t = 1; t < mThresholds.size(); ++t)
				circuitInput0[t] = circuitInput0[0];

			for (u64 t = 0; t < mThresholds.size(); ++t)
			{
				if (pIdx < 2)
				{
					auto threshold = mThresholds[t].getFixedPoint(decimal);

					auto& v = circuitInput0[t].mShares[pIdx];
					for (auto& vv : v)
						vv -= threshold;
				}
			}


			auto cir = lib.int_Sh3Piecewise_helper(sizeof(i64) * 8, mThresholds.size());

			binEng.setCir(cir, inputs.size());
			binEng.setInput(mThresholds.size(), circuitInput1);

			// set the inputs for all of the circuits
			for (u64 t = 0; t < mThresholds.size(); ++t)
				binEng.setInput(t, circuitInput0[t]);

			binEng.asyncEvaluate(self).then([&](Sh3Task self) {
				for (u64 t = 0; t < mInputRegions.size(); ++t)
				{
					mInputRegions[t].resize(inputs.rows(), inputs.cols());
					binEng.getOutput(t, mInputRegions[t]);
				}
				}
				, "binEval-continuation")

				.get();
			UPDATE;
				
		//}, "getInputRegions-part2");
		//auto closure = ret.getClosure("getInputRegions-closure");
		//return closure;
		
		return self.getRuntime();

	}

	Sh3Task Sh3Piecewise::getFunctionValues(
		const si64Matrix & inputs,
		CommPkg & comm,
		Sh3Task self,
		u64 decimal,
		span<si64Matrix> outputs)
	{
		// degree:
		//  -1:  the zero function, f(x)=0
		//   0:  the constant function, f(x) = c
		//   1:  the linear function, f(x) = ax+c
		// ...
		i64 maxDegree = 0;
		for (auto& c : mCoefficients)
		{
			if (c.size() > maxDegree)
				maxDegree = c.size();
		}

		--maxDegree;

		if (maxDegree > 1)
			throw std::runtime_error("not implemented" LOCATION);
		auto pIdx = self.getRuntime().mPartyIdx;

		for (u64 c = 0; c < mCoefficients.size(); ++c)
		{
			if (mCoefficients[c].size() > 1)
			{
				outputs[c].mShares[0].resizeLike(inputs.mShares[0]);
				outputs[c].mShares[1].resizeLike(inputs.mShares[1]);
				outputs[c].mShares[0].setZero();
				outputs[c].mShares[1].setZero();


				auto constant = mCoefficients[c][0].getFixedPoint(decimal);
				if (constant && pIdx < 2)
					outputs[c].mShares[pIdx].setConstant(constant);

				if (mCoefficients[c][1].mIsInteger == false)
					throw std::runtime_error("not implemented" LOCATION);

				i64 integerCoeff = mCoefficients[c][1].getInteger();
				outputs[c].mShares[0] += integerCoeff * inputs.mShares[0];
				outputs[c].mShares[1] += integerCoeff * inputs.mShares[1];
			}
		}

		return self;
	}
}
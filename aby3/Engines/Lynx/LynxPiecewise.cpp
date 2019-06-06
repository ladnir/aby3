#include "LynxPiecewise.h"


#include "cryptoTools/Common/Matrix.h"
#include "cryptoTools/Common/Log.h"
#include "aby3/Engines/Lynx/LynxBinaryEngine.h"


namespace Lynx
{

    Piecewise::Piecewise(u64 dec)
        :mDecimal(dec)
    {
    }

    Word mul(Word op1, Word op2, Word dec)
    {

        static_assert(sizeof(Word) == 8, "");
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


    //void LynxPiecewise::eval(LynxEngine::value_type input, LynxEngine::Word & out)
    //{
    //	eval({ &input, 1 }, {&out , 1});
    //}

    void Piecewise::eval(const Engine::value_type_matrix& inputs, Engine::value_type_matrix& outputs,
        bool print)
    {
        word_matrix in, out;
        in.resizeLike(inputs);
        out.resizeLike(inputs);
        for (u64 i = 0; i < in.size(); ++i)
        {
            in(i) = static_cast<Word>(inputs(i) * (1ull << mDecimal));
        }

        eval(in, out, print);

        for (u64 i = 0; i < in.size(); ++i)
        {
            outputs(i) = out(i) / double(1ull << mDecimal);
        }

    }

    oc::Matrix<u8> Piecewise::getInputRegions(const word_matrix& inputs)
    {
        oc::Matrix<u8> inputRegions(inputs.rows(), mThresholds.size() + 1);
        std::vector<u8> thresholds(mThresholds.size());
        for (u64 i = 0; i < inputs.rows(); ++i)
        {
            auto& in = inputs(i);

            for (u64 t = 0; t < mThresholds.size(); ++t)
            {
                thresholds[t] = in < mThresholds[t].getFixedPoint(mDecimal) ? 1 : 0;
                //std::cout << in << " < " << mThresholds[t].getFixedPoint(mDecimal) << " -> " << thresholds[t] << std::endl;
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

    void Piecewise::eval(const word_matrix & inputs, word_matrix & outputs, bool print)
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
                std::cout << mThresholds[i].getFixedPoint(mDecimal) << "  ";
            }
            std::cout << std::endl;
        }


        //auto decShift = std::pow(2.0, mDecimal);
        //Matrix<bool> inputThresholds(inputs.size(), mThresholds.size());
        oc::Matrix<u8> inputRegions = getInputRegions(inputs);

        if (print)
        {
            for (u64 i = 0; i < inputs.rows(); ++i)
            {
                std::cout << i << ":  ";
                for (u64 t = 0; t < mCoefficients.size(); ++t)
                {
                    std::cout << u32(inputRegions(i, t)) << ", ";
                }

                std::cout << "  ~~  " << inputs(i) << " (" << (inputs(i) / double(1ull << mDecimal)) << ")  ~~  ";
                for (u64 t = 0; t < mThresholds.size(); ++t)
                {
                    std::cout << mThresholds[t].getFixedPoint(mDecimal) << ", ";
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
                Engine::Word ft = 0;
				Engine::Word inPower = (1ll << mDecimal);
                if (print)
                    std::cout << "   " << int(inputRegions(i, t)) << " * (";

                for (u64 c = 0; c < mCoefficients[t].size(); ++c)
                {
                    auto coef = mCoefficients[t][c].getFixedPoint(mDecimal);

                    if (print)
                        std::cout << coef << " * " << inPower << " + ";

                    ft += mul(coef, inPower, mDecimal);

                    inPower = mul(in, inPower, mDecimal);
                }


                if (print)
                {
                    std::cout << ")" << std::endl;
                }

                out += inputRegions(i, t) * ft;
            }
        }

    }

    void Piecewise::eval(
        Engine & eng,
        const Engine::Matrix & inputs,
        Engine::Matrix & outputs,
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

        //#define PIECEWISE_DEBUG



        outputs.mShares[0].setZero();
        outputs.mShares[1].setZero();

        std::vector<Engine::Matrix> inputRegions(mCoefficients.size());
        getInputRegions(inputs, eng, inputRegions, print);


        //TODO("!!!!!!!!!!!!!! REMOVE THIS !!!!!!!!!!!!!! ");
#ifdef PIECEWISE_DEBUG

        auto plain_inputs = eng.reveal(inputs);
        Engine::value_type_matrix plain_outputs(outputs.rows(), outputs.cols());
        eval(plain_inputs, plain_outputs);

        auto inputVals = eng.reconstructShare(inputs);
        auto true_inputRegions = getInputRegions(inputVals);

        std::vector<word_matrix> inputRegions__(mCoefficients.size());
        for (u64 i = 0; i < mCoefficients.size(); ++i)
        {
            inputRegions__[i] = eng.reconstructShare(inputRegions[i], ShareType::Binary);
            //std::cout << i << std::endl << inputRegions__[i] << std::endl << std::endl;
        }

        for (u64 i = 0; i < inputs.size(); ++i)
        {
            oc::ostreamLock oo(std::cout);

            if (print) oo << i << ":  ";

            for (u64 t = 0; t < inputRegions.size(); ++t)
            {
                if (print) oo << inputRegions__[t](i) << ", ";


                if (true_inputRegions(i, t) != inputRegions__[t](i))
                {
                    oo << "bad input region " << i << " " << t << std::endl;
                }
            }
            if (print) oo << std::endl;
        }
#endif


        if (print)
        {
            auto c = 1;
            oc::ostreamLock(std::cout) << "f" << eng.mPartyIdx << " c=" << c << ": " << inputRegions[c].mShares[0](0) << " " << inputRegions[c].mShares[1](0) << std::endl;
        }

        //// Phase 3: ...
        std::vector<Engine::Matrix> functionOutputs(mCoefficients.size());
        getFunctionValues(inputs, eng, functionOutputs);
        std::vector<CompletionHandle> handles(mCoefficients.size());

        //char cccc = 0;

        outputs.mShares[0].setZero();
        outputs.mShares[1].setZero();
        for (u64 c = 0; c < mCoefficients.size(); ++c)
        {
            //eng.mNext.asyncSend(cccc);
            //eng.mPrev.asyncSend(cccc);
            //eng.mNext.recv(cccc);
            //eng.mPrev.recv(cccc);

            if (mCoefficients[c].size())
            {

                if (mCoefficients[c].size() > 1)
                {
                    // multiplication by a private value
                    handles[c] =
                        eng.asyncArithBinMul(functionOutputs[c], inputRegions[c], functionOutputs[c]);
                    //handles[c].get();
                }
                else
                {
                    // multiplication by a public constant
                    functionOutputs[c].resize(inputs.rows(), inputs.cols());
                    handles[c] =
                        eng.asyncArithBinMul(mCoefficients[c][0].getFixedPoint(mDecimal), inputRegions[c], functionOutputs[c]);
                    //handles[c].get();
                }


                //auto pub = eng.reconstructShare(functionOutputs[c]);
#ifdef PIECEWISE_DEBUG
                if (print)
                {
                    std::cout << "coef[" << c << "] = ";
                    if (mCoefficients[c].size() > 1)
                        std::cout << mCoefficients[c][1].getInteger() << " " << inputVals(0) << " + ";

                    if (mCoefficients[c].size() > 0)
                        std::cout << mCoefficients[c][0].getFixedPoint(mDecimal) << std::endl;
                }
#endif
            }
        }

        for (u64 c = 0; c < mCoefficients.size(); ++c)
        {
            if (handles[c].mGet)
            {
                handles[c].get();
                outputs = outputs + functionOutputs[c];

                if (print && c == 1)
                    oc::ostreamLock(std::cout) << "f" << eng.mPartyIdx << " c=" << c << ": " << functionOutputs[c].mShares[0](0) << " " << functionOutputs[c].mShares[1](0) << std::endl;
            }
        }
    }


    void Piecewise::getInputRegions(
        const Engine::Matrix & inputs,
        Engine & eng,
        span<Engine::Matrix> inputRegions,
        bool print)
    {

        // First we want to transform the input into a more efficient prepresentation. 
        // Currently we have x0,x1,x2 which sum to the input. We are going to add
        // x0 and x1 together so that we have a 2-out-of-2 sharing of the input. Party 0 
        // who holds both of these shares will do the addition. After this, we are 
        // going to input these two shares into a circuit to compute the threshold bits

        // This is a bit stange but for each theshold computation, input0 will
        // be different but input1 will be the same. This is a small optimization.
        std::vector<Engine::Matrix> circuitInput0(mThresholds.size());
        Engine::Matrix circuitInput1(inputs.size(), 1);
        for (auto&c : circuitInput0) c.resize(inputs.size(), 1);



        // Lets us construct two sharings (x0+x1) and x2. The former will be a normal sharing
        // and input by party 0. This will be stored in the first matrix of circuitInput0.
        // The second shring will be special in that the sharing [x2]=(0,x2,0). In general this
        // is insecure. However, in this case its ok.
        if (eng.mPartyIdx == 0)
        {
            // have party 0 add the two shares together.
            circuitInput0[0].mShares[0]
                = inputs.mShares[0]
                + inputs.mShares[1];


            TODO("Radomize the shares....");
            // We need to randomize the share
            //for (u64 i = 0; i < circuitInput0[0].size(); ++i)
            //	circuitInput0[0].mShares[0](i) += eng.getBinaryShare();

            // send over this share
            eng.mNext.asyncSend(circuitInput0[0].mShares[0].data(), circuitInput0[0].mShares[0].size());


            circuitInput1.mShares[0].setZero();
            circuitInput1.mShares[1].setZero();

            TODO("Optimize with preshared matrix...");
            eng.mPrev.recv(circuitInput0[0].mShares[1].data(), circuitInput0[0].mShares[1].size());
        }
        else if (eng.mPartyIdx == 1)
        {
            circuitInput1.mShares[0] = std::move(inputs.mShares[0]);
            circuitInput1.mShares[1].setZero();

            TODO("Radomize the shares....");
            circuitInput0[0].mShares[0].setZero();
            //for (u64 i = 0; i < inputs.size(); ++i)
            //	circuitInput0[0].mShares[0](i) += eng.getBinaryShare();

            TODO("Optimize with preshared matrix...");
            eng.mNext.asyncSend(circuitInput0[0].mShares[0].data(), circuitInput0[0].mShares[0].size());
            eng.mPrev.recv(circuitInput0[0].mShares[1].data(), circuitInput0[0].mShares[1].size());
        }
        else if (eng.mPartyIdx == 2)
        {
            circuitInput1.mShares[0].setZero();
            circuitInput1.mShares[1] = std::move(inputs.mShares[1]);

            TODO("Radomize the shares....");
            circuitInput0[0].mShares[0].setZero();
            //for (u64 i = 0; i < inputs.size(); ++i)
            //	circuitInput0[0].mShares[0](i) += eng.getBinaryShare();

            TODO("Optimize with preshared matrix...");
            eng.mNext.asyncSend(circuitInput0[0].mShares[0].data(), circuitInput0[0].mShares[0].size());
            eng.mPrev.recv(circuitInput0[0].mShares[1].data(), circuitInput0[0].mShares[1].size());
        }

        //auto test = circuitInput0[0] + circuitInput1;
        //auto tt = eng.reconstructShare(test);
        //auto ii = eng.reconstructShare(inputs);
        //if (tt != ii)
        //{
        //	oc::ostreamLock outt(std::cout);
        //	for (u64 i = 0; i < tt.size(); ++i)
        //	{
        //		outt << tt(i) << "  " << ii(i) << std::endl;
        //	}
        //	throw std::runtime_error(LOCATION);
        //}

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
            if (eng.mPartyIdx < 2)
            {
                auto threshold = mThresholds[t].getFixedPoint(eng.mDecimal);


                circuitInput0[t].mShares[eng.mPartyIdx].array() -= threshold;
            }
        }

        //if (print)
        //{
        //    eng.sync();

        //    auto ii = eng.reconstructShare(inputs);
        //    for (u64 t = 0; t < mThresholds.size(); ++t)
        //    {
        //    	auto test = circuitInput0[t] + circuitInput1;
        //    	auto tt = eng.reconstructShare(test);

        //        //ostreamLock oo(std::cout);
        //        //oo << eng.mPartyIdx <<" " <<t<<" " << test.mShares[0](0) << " " << test.mShares[1](0) << std::endl;

        //    	auto threshold = mThresholds[t].getFixedPoint(eng.mDecimal);

        //    	for (u64 i = 0; i < tt.size(); ++i)
        //    	{
        //    		if (tt(i) != ii(i) - threshold)
        //    		{
        //    			throw std::runtime_error(LOCATION);
        //    		}
        //    	}
        //    }
        //}

        auto cir = eng.mCirLibrary.int_Sh3Piecewise_helper(sizeof(Engine::Word) * 8, mThresholds.size());
        BinaryEngine binEng(eng.mPartyIdx, eng.mPrev, eng.mNext);
        binEng.setCir(cir, inputs.size());
        binEng.setInput(mThresholds.size(), circuitInput1);

        // set the inputs for all of the circuits
        for (u64 t = 0; t < mThresholds.size(); ++t)
            binEng.setInput(t, circuitInput0[t]);

        binEng.evaluate();

        for (u64 t = 0; t < inputRegions.size(); ++t)
        {
            inputRegions[t].resize(inputs.rows(), inputs.cols());
            binEng.getOutput(t, inputRegions[t]);
        }

    }

    void Piecewise::getFunctionValues(
        const Engine::Matrix & inputs,
        Engine & eng,
        span<Engine::Matrix> outputs)
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


        for (u64 c = 0; c < mCoefficients.size(); ++c)
        {
            if (mCoefficients[c].size() > 1)
            {
                outputs[c].mShares[0].resizeLike(inputs.mShares[0]);
                outputs[c].mShares[1].resizeLike(inputs.mShares[1]);
                outputs[c].mShares[0].setZero();
                outputs[c].mShares[1].setZero();


                auto constant = mCoefficients[c][0].getFixedPoint(mDecimal);
                if (constant && eng.mPartyIdx < 2)
                    outputs[c].mShares[eng.mPartyIdx].setConstant(constant);

                if (mCoefficients[c][1].mIsInteger == false)
                    throw std::runtime_error("not implemented" LOCATION);

                i64 integerCoeff = mCoefficients[c][1].getInteger();
                outputs[c].mShares[0] += integerCoeff * inputs.mShares[0];
                outputs[c].mShares[1] += integerCoeff * inputs.mShares[1];
            }
        }
    }
}
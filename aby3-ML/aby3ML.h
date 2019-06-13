#pragma once
#include <cryptoTools/Network/Session.h>
#include <cryptoTools/Network/Channel.h>
#include <aby3/Common/Defines.h>
#include <aby3/Engines/sh3/Sh3FixedPoint.h>
#include <aby3/Engines/sh3/Sh3Encryptor.h>
#include <aby3/Engines/sh3/Sh3Evaluator.h>
#include <aby3/Engines/sh3/Sh3Piecewise.h>

namespace aby3
{
	class aby3ML
	{
	public:
		oc::Channel mPreproNext, mPreproPrev, mNext, mPrev;
		Sh3Encryptor mEnc;
		Sh3Evaluator mEval;
		Sh3Runtime mRt;
		bool mPrint = true;

		u64 partyIdx()
		{
			return mRt.mPartyIdx;
		}

		void init(u64 partyIdx, oc::Session& prev, oc::Session& next, oc::block seed);


		template<Decimal D>
		sf64Matrix<D> localInput(const f64Matrix<D>& val)
		{
			std::array<u64, 2> size{ val.rows(), val.cols() };
			mNext.asyncSendCopy(size);
			mPrev.asyncSendCopy(size);
			sf64Matrix<D> dest(size[0],size[1]);
			mEnc.localFixedMatrix(mRt.noDependencies(), val, dest).get();
			return dest;
		}


		template<Decimal D>
		sf64Matrix<D> localInput(const eMatrix<double>& vals)
		{
			f64Matrix<D> v2(vals.rows(), vals.cols());
			for (u64 i = 0; i < vals.size(); ++i)
				v2(i) = vals(i);

			return localInput(v2);
		}

		template<Decimal D>
		sf64Matrix<D> remoteInput(u64 partyIdx)
		{
			std::array<u64, 2> size;
			if (partyIdx == (mRt.mPartyIdx + 1) % 3)
				mNext.recv(size);
			else if (partyIdx == (mRt.mPartyIdx + 2) % 3)
				mPrev.recv(size);
			else
				throw RTE_LOC;

			sf64Matrix<D> dest(size[0], size[1]);
			mEnc.remoteFixedMatrix(mRt.noDependencies(), dest).get();
			return dest;
		}


		void preprocess(u64 n, Decimal d)
		{
			TODO("implement this");
		}



		template<Decimal D>
		eMatrix<double> reveal(const sf64Matrix<D>& vals)
		{
			f64Matrix<D> temp(vals.rows(), vals.cols());
			mEnc.revealAll(mRt.noDependencies(), vals, temp).get();

			eMatrix<double> ret(vals.rows(), vals.cols());
			for (u64 i = 0; i < ret.size(); ++i)
				ret(i) = static_cast<double>(temp(i));
			return ret;
		}

		template<Decimal D>
		double reveal(const sf64<D>& val)
		{
			f64<D> dest;
			mEnc.revealAll(mRt.noDependencies(), val, dest).get();
			return static_cast<double>(dest);
		}

		template<Decimal D>
		double reveal(const Ref<sf64<D>>& val)
		{
			sf64<D> v2(val);
			return reveal(v2);
		}

		template<Decimal D>
		sf64Matrix<D> mul(const sf64Matrix<D>& left, const sf64Matrix<D>& right)
		{
			sf64Matrix<D> dest;
			mEval.asyncMul(mRt.noDependencies(), left, right, dest).get();
			return dest;
		}

		template<Decimal D>
		sf64Matrix<D> mulTruncate(const sf64Matrix<D>& left, const sf64Matrix<D>& right, u64 shift)
		{
			sf64Matrix<D> dest;
			mEval.asyncMul(mRt.noDependencies(), left, right, dest, shift).get();
			return dest;
		}


		Sh3Piecewise mLogistic;

		template<Decimal D>
		sf64Matrix<D> logisticFunc(const sf64Matrix<D>& Y)
		{
			if (mLogistic.mThresholds.size() == 0){
				mLogistic.mThresholds.resize(2);
				mLogistic.mThresholds[0] = -0.5;
				mLogistic.mThresholds[1] = 0.5;
				mLogistic.mCoefficients.resize(3);
				mLogistic.mCoefficients[1].resize(2);
				mLogistic.mCoefficients[1][0] = 0.5;
				mLogistic.mCoefficients[1][1] = 1;
				mLogistic.mCoefficients[2].resize(1);
				mLogistic.mCoefficients[2][0] = 1;
			}

			sf64Matrix<D> out(Y.rows(), Y.cols());
			mLogistic.eval<D>(mRt.noDependencies(), Y, out, mEval);
			return out;
		}














		template<typename T>
		aby3ML& operator<<(const T& v)
		{
			if(partyIdx()==0 && mPrint) std::cout << v;
			return *this;
		}
		template<typename T>
		aby3ML& operator<<(T& v)
		{
			if (partyIdx() == 0 && mPrint) std::cout << v;
			return *this;
		}
		aby3ML& operator<< (std::ostream& (*v)(std::ostream&))
		{
			if (partyIdx() == 0 && mPrint) std::cout << v;
			return *this;
		}
		aby3ML& operator<< (std::ios& (*v)(std::ios&))
		{
			if (partyIdx() == 0 && mPrint) std::cout << v;
			return *this;
		}
		aby3ML& operator<< (std::ios_base& (*v)(std::ios_base&))
		{
			if (partyIdx() == 0 && mPrint) std::cout << v;
			return *this;
		}

	};

}

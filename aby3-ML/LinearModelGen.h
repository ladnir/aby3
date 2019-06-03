#pragma once
#include <vector>

#include <Eigen/Dense>
#include <aby3/Engines/sh3/Sh3Types.h>

namespace aby3
{
	//template<typename T>
	//using eMatrix = eMatrix<T>;

	class LinearModelGen
	{
	public:

		eMatrix<double> mModel;
		double mNoise, mSd;

		void setModel(eMatrix<double>& model, double noise = 1, double sd = 1);

		void sample(
			eMatrix<double>& X,
			eMatrix<double>& Y);



	};

	class LogisticModelGen
	{
	public:

		eMatrix<double> mModel;
		double mNoise, mSd;

		void setModel(eMatrix<double>& model, double noise = 1, double sd = 1);

		void sample(
			eMatrix<double>& X,
			eMatrix<double>& Y,
			bool print = false);
	};

}

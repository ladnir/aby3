#include "LinearModelGen.h"

#include <cryptoTools/Common/Defines.h>

#include <random>
namespace aby3
{


	void LinearModelGen::setModel(eMatrix<double>& model, double noise, double sd)
	{
		mModel = model;
		mNoise = noise;
		mSd = sd;
	}

	void LinearModelGen::sample(eMatrix<double>& X, eMatrix<double>& Y)
	{
		if (X.rows() != Y.rows()) throw std::runtime_error(LOCATION);
		if (1 != Y.cols()) throw std::runtime_error(LOCATION);
		if (X.cols() != mModel.rows()) throw std::runtime_error(LOCATION);

		std::default_random_engine generator;
		std::normal_distribution<double> distribution(mNoise, mSd);

		eMatrix<double> noise(X.rows(), 1);


		for (int i = 0; i < X.rows(); ++i)
		{
			for (int j = 0; j < X.cols(); ++j)
			{
				X(i, j) = distribution(generator);
			}

			noise(i, 0) = distribution(generator);
		}

		Y = X * mModel + noise;
	}

	void LogisticModelGen::setModel(eMatrix<double>& model, double noise, double sd)
	{
		mModel = model;
		mNoise = noise;
		mSd = sd;
	}

	void LogisticModelGen::sample(
		eMatrix<double>& X,
		eMatrix<double>& Y,
		bool print)
	{
		if (X.rows() != Y.rows()) throw std::runtime_error(LOCATION);
		if (1 != Y.cols()) throw std::runtime_error(LOCATION);
		if (X.cols() != mModel.rows()) throw std::runtime_error(LOCATION);

		std::default_random_engine generator(234345);
		std::normal_distribution<double> distribution(mNoise, mSd);

		eMatrix<double> noise(X.rows(), 1);


		for (int i = 0; i < X.rows(); ++i)
		{
			for (int j = 0; j < X.cols(); ++j)
			{
				X(i, j) = distribution(generator);
			}

			noise(i, 0) = distribution(generator);
			//std::cout << "n" << i << " = " << noise(i, 0) << std::endl;;
		}

		Y = X * mModel + noise;

		for (int i = 0; i < Y.size(); ++i)
		{
			auto y = Y(i);
			//auto yy = 1.0 / (1 + std::exp(-Y(i)));
			Y(i) = y > 0;

			if (print)
			{

				for (int j = 0; j < X.cols(); ++j)
				{
					std::cout << X(i, j) << " ";
				}

				std::cout << "-> " << Y(i) << " (" << y << ")" << std::endl;
			}
		}
	}
}

#pragma once
#include <vector>

#include <Eigen/Dense>
#include <cryptoTools/Crypto/PRNG.h>

class NeuralModelGen
{
public:

	std::vector<Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic>> mModel;
	//double mNoise, mSd;

	void sampleModel(int dimensions, int levels, int outDimension, oc::PRNG& prng);

	void sample(
		Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic>& X,
		Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic>& Y);



};
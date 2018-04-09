#include "PlainEngine.h"



PlainEngine::PlainEngine()
{
}


PlainEngine::~PlainEngine()
{
}

PlainEngine::value_type PlainEngine::reveal(const Share & share)
{
	return share;
}

Eigen::Matrix<PlainEngine::Share, Eigen::Dynamic, Eigen::Dynamic> PlainEngine::mul(
	const Eigen::Matrix<Share, Eigen::Dynamic, Eigen::Dynamic>& A,
	const Eigen::Matrix<Share, Eigen::Dynamic, Eigen::Dynamic>& B)
{
	return A * B;
}

Eigen::Matrix<PlainEngine::Share, Eigen::Dynamic, Eigen::Dynamic> PlainEngine::mulTruncate(
	const Eigen::Matrix<Share, Eigen::Dynamic, Eigen::Dynamic>& A,
	const Eigen::Matrix<Share, Eigen::Dynamic, Eigen::Dynamic>& B,
	u64 truncation)
{
	return A * B / (1ULL << truncation);
}

PlainEngine::Matrix PlainEngine::logisticFunc(const Matrix & Y)
{
	Matrix ret;
	ret.resizeLike(Y);

	for (u64 i = 0; i < ret.size(); ++i)
	{
		ret(i) = 1.0 / (1 + std::exp(-Y(i)));
	}

	return ret;
}


PlainEngine::Matrix PlainEngine::reluFunc(const Matrix & Y)
{
	Matrix ret;
	ret.resizeLike(Y);

	for (u64 i = 0; i < ret.size(); ++i)
	{
		ret(i) = std::max(0.0, Y(i));
	}

	return ret;
}


PlainEngine::Matrix PlainEngine::argMax(const Matrix & Y)
{
	Matrix ret;
	ret.resize(Y.rows(), 1);
	ret.setZero();

	for (u64 i = 0; i < ret.size(); ++i)
	{
		auto max = Y(i, 0);
		for (u64 j = 1; j < Y.cols(); ++j)
		{
			if (Y(i, j) > max)
			{
				max = Y(i, j);
				ret(i) = j;
			}
		}
	}

	return ret;
}

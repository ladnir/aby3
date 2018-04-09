#pragma once

#include <Eigen/Dense>
#include <cryptoTools/Common/Defines.h>
using namespace oc;


class PlainEngine
{
public:
	PlainEngine();
	~PlainEngine();


	typedef double Share;
	typedef double value_type;
	typedef Eigen::Matrix<Share, Eigen::Dynamic, Eigen::Dynamic> Matrix;

	value_type reveal(const Share& share);

	Matrix mul(
		const Matrix& A,
		const Matrix& B);

	Matrix mulTruncate(
		const Matrix& A,
		const Matrix& B,
		u64 truncation);


	Matrix logisticFunc(const Matrix& Y);
	Matrix reluFunc(const Matrix& Y);
	Matrix argMax(const Matrix& Y);


	void sync() {};
};


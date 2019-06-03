#pragma once

#include <aby3/Engines/sh3/Sh3Types.h>

namespace aby3
{

	class PlainRuntime
	{
	public:

		eMatrix<double> mul(const eMatrix<double>& left, const eMatrix<double>& right)
		{
			return left * right;
		}

		eMatrix<double> mulTruncate(const eMatrix<double>& left, const eMatrix<double>& right, u64 shift)
		{
			auto div = 1ull << shift;
			eMatrix<double> ret = left * right;
			for (u64 i = 0; i < ret.size(); ++i)
			{
				ret(i) /= div;
			}

			return ret;
		}
		double reveal(const double& v)
		{
			return v;
		}
		eMatrix<double> reveal(const eMatrix<double>& v)
		{
			return v;
		}
	};

}

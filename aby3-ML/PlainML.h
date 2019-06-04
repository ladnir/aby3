#pragma once

#include <aby3/Engines/sh3/Sh3Types.h>

namespace aby3
{

	class PlainML
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

		eMatrix<double> logisticFunc(eMatrix<double> x)
		{
			for (u64 i = 0; i < x.size(); ++i)
				x(i) = 1.0 / (1 + std::exp(-x(i)));
			return x;
		}

		double reveal(const double& v)
		{
			return v;
		}
		eMatrix<double> reveal(const eMatrix<double>& v)
		{
			return v;
		}

		u64 partyIdx()
		{
			return 0;
		}




















		template<typename T>
		std::ostream& operator<<(const T& v)
		{
			std::cout << v;
			return std::cout;
		}
		template<typename T>
		std::ostream& operator<<(T& v)
		{
			std::cout << v;
			return std::cout;
		}
		std::ostream& operator<< (std::ostream& (*v)(std::ostream&))
		{
			std::cout << v;
			return std::cout;
		}
		std::ostream& operator<< (std::ios& (*v)(std::ios&))
		{
			std::cout << v;
			return std::cout;
		}
		std::ostream& operator<< (std::ios_base& (*v)(std::ios_base&))
		{
			std::cout << v;
			return std::cout;
		}

	};

}

#include "Sh3FixedPoint.h"


#include <boost/multiprecision/cpp_int.hpp>
#include <cryptoTools/Common/BitVector.h>
namespace aby3
{
	template<typename T, Decimal D>
	fp<T, D> fp<T, D>::operator*(const fp<T, D>& rhs) const
	{
		boost::multiprecision::int128_t v0 = mValue, v1 = rhs.mValue;

		v0 = v0 * v1;
		v0 = v0 / (1ull << mDecimal);

		return {
			static_cast<i64>(v0),
			monostate{}
		};
	}


	//template<typename T, Decimal D>
	//std::ostream& operator<<(std::ostream& o, const fp<T, D>& f)
	//{
	//	auto mask = ((1ull << f.mDecimal) - 1);
	//	auto print = [mask](T v) {
	//		std::stringstream s;
	//		oc::BitVector bv;
	//		bv.append((u8*)& v, D);
	//		s << bv;

	//		s << ".";
	//		bv = {};
	//		bv.append((u8*)& v, 64 - D, D);
	//		s << bv;
	//		return s.str();
	//	};

	//	std::stringstream ss;
	//	//auto& ss = o;
	//	auto vv = (f.mValue / double(1 << f.mDecimal));
	//	o << std::setprecision(std::numeric_limits<long double>::digits10 + 1)
	//		<< vv << std::endl;
	//	o << print(f.mValue) << std::endl;

	//	u64 v = 0;
	//	if (f.mValue >= 0)
	//		v = f.mValue;
	//	else
	//	{
	//		ss << '-';
	//		v =-f.mValue;
	//	}

	//	ss << (v >> f.mDecimal) << ".";


	//	v &= mask;

	//	int i = 0;
	//	if (v)
	//	{
	//		o << "["<<i<<"]  " << print(v) << std::endl;
	//		while (v & mask)
	//		{
	//			++i;
	//			v *= 10;
	//			ss << (v >> f.mDecimal);
	//			o << "[" << i << "]  " << print(v) << std::endl;

	//			v &= mask;
	//			o << "[" << i << "]* " << print(v) << std::endl;
	//		}
	//	}
	//	else
	//	{
	//		ss << '0';
	//	}
	//	o << ss.str();

	//	return o;
	//}

	//template std::ostream& operator<<<i64, D8>(std::ostream& o, const fp<i64, D8>& f);
	//template std::ostream& operator<<<i64, D16>(std::ostream& o, const fp<i64, D16>& f);
	//template std::ostream& operator<<<i64, D32>(std::ostream& o, const fp<i64, D32>& f);

	template struct fp<i64, D0>;
	template struct fp<i64, D8>;
	template struct fp<i64, D16>;
	template struct fp<i64, D32>;

	template struct sf64<D0>;
	template struct sf64<D8>;
	template struct sf64<D16>;
	template struct sf64<D32>;

	template struct sf64Matrix<D0>;
	template struct sf64Matrix<D8>;
	template struct sf64Matrix<D16>;
	template struct sf64Matrix<D32>;

}

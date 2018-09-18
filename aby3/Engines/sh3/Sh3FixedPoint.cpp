#include "Sh3FixedPoint.h"


#include <boost/multiprecision/cpp_int.hpp>

namespace aby3
{
    namespace Sh3
    {

        template<typename T,Decimal D>
        fp<T,D> fp<T,D>::operator*(const fp<T,D>& rhs) const
        {
            boost::multiprecision::int128_t v0 = mValue, v1 = rhs.mValue;

            v0 = v0 * v1;
            v0 = v0 / (1ull << mDecimal);

            return {
                static_cast<i64>(v0),
                monostate{}
            };
        }



        template struct fp<i64, D0>;
        template struct fp<i64, D8>;
        template struct fp<i64, D16>;
        template struct fp<i64, D32>;



        template struct sf64<D0>;
        template struct sf64<D8>;
        template struct sf64<D16>;
        template struct sf64<D32>;
    }
}

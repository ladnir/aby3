#pragma once
#include <aby3/Common/Defines.h>
#include <aby3/Engines/sh3/Sh3Types.h>
namespace aby3
{
    namespace Sh3
    {
        enum Decimal
        {
            D0 = 0,
            D8 = 8,
            D16 = 16,
            D32 = 32
        };

        //template<Decimal D>
        //using f64 = fpml::fixed_point<i64, 63 - D, D>;
        struct monostate {};



        template<typename T, Decimal D>
        struct fp
        {
            static const Decimal mDecimal = D;

            T mValue = 0;

            fp() = default;
            fp(const fp<T,D>&) = default;
            fp(fp<T,D>&&) = default;
            fp(const double v) {
                *this = v;
            }

            fp<T,D> operator+(const fp<T, D>& rhs) const {
                return { mValue + rhs.mValue, monostate{} };
            }

            fp<T, D> operator-(const fp<T, D>& rhs) const {
                return { mValue - rhs.mValue, monostate{} };
            }
            fp<T, D> operator*(const fp<T,D>& rhs) const;
            //fp<T,D> operator*(double val) const;
            fp<T,D> operator>>(i64 shift) const { return { mValue >> shift, monostate{} }; }
            fp<T,D> operator<<(i64 shift) const { return { mValue << shift, monostate{} }; }


            template<typename T2>
            fp<T,D>& operator=(const fp<T2,D>& v)
            {
                mValue = v.mValue;

                return *this;
            }

            template<typename T2>
            fp<T, D>& operator=(fp<T2, D>&& v)
            {
                mValue = v.mValue;

                return *this;
            }

            void operator=(const double& v)
            {
                mValue = T(v * (1 << D));
            }

            bool operator==(const fp<T,D>& v) const
            {
                return mValue == v.mValue;
            }
            bool operator!=(const fp<T,D>& v) const
            {
                return !(*this == v);
            }

        private:
            fp(T v, monostate)
                : mValue(v)
            {}
        };

        template<Decimal D>
        using f64 = fp<i64, D>;

        template<typename T, Decimal D>
        std::ostream& operator<<(std::ostream& o, const fp<T,D>& f)
        {
            o << (f.mValue >> f.mDecimal) << "."
                << (u64(f.mValue) & ((1ull << f.mDecimal) - 1));

            return o;
        }


        template<Decimal D>
        struct sf64
        {
            static const Decimal mDecimal = D;

            using value_type = si64::value_type;
            si64 mShare;

            sf64() = default;
            sf64(const sf64<D>&) = default;
            sf64(sf64<D>&&) = default;
            sf64(const std::array<value_type, 2>& d) :mShare(d) {}
            sf64(const Ref<sf64<D>>& s) {
                mShare.mData[0] = *s.mData[0];
                mShare.mData[1] = *s.mData[1];
            }

            sf64<D>& operator=(const sf64<D>& copy)
            {
                mShare = copy.mShare;
                return *this;
            }

            sf64<D> operator+(const sf64<D>& rhs) const
            {
                sf64<D> r;
                r.mShare = mShare + rhs.mShare;
                return r;
            }
            sf64<D> operator-(const sf64<D>& rhs) const
            {
                sf64<D> r;
                r.mShare = mShare - rhs.mShare;
                return r;
            }

            value_type& operator[](u64 i) { return mShare[i]; }
            const value_type& operator[](u64 i) const { return mShare[i]; }
        };

    }
}
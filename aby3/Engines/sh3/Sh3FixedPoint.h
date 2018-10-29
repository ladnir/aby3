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
		struct fp;

		template<typename T, Decimal D>
		struct fpRef
		{
			static const Decimal mDecimal = D;
			T* mData;

			const fp<T, D>& operator=(const fp<T, D>& v);

			operator fp<T, D>() const
			{
				fp<T, D> v;
				v = *this;
				return v;
			}


			operator double() const
			{
				fp<T, D> v;
				v = *this;
				return static_cast<double>(v);
			}
		};



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

			const fp<T, D>& operator=(const fpRef<T, D>& v)
			{
				mValue = *v.mData;
				return *this;
			}


			operator double() const
			{
				return mValue / double(T(1) << mDecimal);
			}

            void operator=(const double& v)
            {
                mValue = T(v * (T(1) << D));
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


		template<typename T, Decimal D>
		const fp<T, D>& fpRef<T,D>::operator=(const fp<T, D>& v)
		{
			*mData = v.mValue;
			return v;
		}


        template<Decimal D>
        using f64 = fp<i64, D>;

        template<typename T, Decimal D>
        std::ostream& operator<<(std::ostream& o, const fp<T,D>& f)
        {
            o << (f.mValue >> f.mDecimal) << "."
                << (u64(f.mValue) & ((1ull << f.mDecimal) - 1));

            return o;
		}


		template<typename T, Decimal D>
		//using fpMatrix = eMatrix<fp<T, D>>;
		struct fpMatrix
		{
			static const Decimal mDecimal = D;
			eMatrix<T> mData;

			fpMatrix() = default;
			fpMatrix(const fpMatrix<T, D>&) = default;
			fpMatrix(fpMatrix<T, D>&&) = default;

			fpMatrix(u64 xSize, u64 ySize)
				: mData(xSize, ySize)
			{}

			void resize(u64 xSize, u64 ySize)
			{
				mData.resize(xSize, ySize);
			}


			u64 rows() const { return mData.rows(); }
			u64 cols() const { return mData.cols(); }
			u64 size() const { return mData.size(); }


			const fpMatrix<T, D>& operator=(const fpMatrix<T, D>& rhs) 
			{
				mData = rhs.mData;
				return rhs;
			}


			fpMatrix<T, D> operator+(const fpMatrix<T, D>& rhs) const
			{
				return { mData + rhs.mData };
			}
			fpMatrix<T, D> operator-(const fpMatrix<T, D>& rhs) const
			{
				return { mData - rhs.mData };
			}
			fpMatrix<T, D> operator*(const fpMatrix<T, D>& rhs) const
			{
				return { mData * rhs.mData };
			}

			fpMatrix<T, D>& operator+=(const fpMatrix<T, D>& rhs) 
			{
				 mData += rhs.mData ;
				 return *this;
			}
			fpMatrix<T, D>& operator-=(const fpMatrix<T, D>& rhs)
			{
				mData -= rhs.mData;
				return *this;
			}

			fpMatrix<T, D>& operator*=(const fpMatrix<T, D>& rhs)
			{
				mData *= rhs.mData;
				return *this;
			}


			fpRef<T, D> operator()(u64 x, u64 y)
			{ 
				return { &mData(x, y) };
			}

			fpRef<T, D> operator()(u64 xy)
			{
				return { &mData(xy) };
			}





		private:
			fpMatrix(const eMatrix<T>&v)
				:mData(v) {}
			fpMatrix(eMatrix<T>&&v)
				:mData(v) {}
		};

		template<Decimal D>
		using f64Matrix = fpMatrix<i64, D>;


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


		template<Decimal D>
		struct sf64Matrix
		{
		    static const Decimal mDecimal = D;
		    std::array<eMatrix<i64>, 2> mShares;

		    struct ConstRow { const sf64Matrix<D>& mMtx; const u64 mIdx; };
		    struct Row { sf64Matrix<D>& mMtx; const u64 mIdx; const Row& operator=(const Row& row); const ConstRow& operator=(const ConstRow& row); };

		    struct ConstCol { const sf64Matrix<D>& mMtx; const u64 mIdx; };
		    struct Col { sf64Matrix<D>& mMtx; const u64 mIdx; const Col& operator=(const Col& col); const ConstCol& operator=(const ConstCol& row); };

		    sf64Matrix() = default;
		    sf64Matrix(u64 xSize, u64 ySize)
		    {
		        resize(xSize, ySize);
		    }

		    void resize(u64 xSize, u64 ySize)
		    {
		        mShares[0].resize(xSize, ySize);
		        mShares[1].resize(xSize, ySize);
		    }


		    u64 rows() const { return mShares[0].rows(); }
		    u64 cols() const { return mShares[0].cols(); }
		    u64 size() const { return mShares[0].size(); }

			Ref<sf64<D>> operator()(u64 x, u64 y) 
			{
				typename sf64<D>::value_type& s0 = mShares[0](x, y);
				typename sf64<D>::value_type& s1 = mShares[1](x, y);

				return Ref<sf64<D>>(s0, s1);
			}

		    Ref<sf64<D>> operator()(u64 xy) 
			{
				auto& s0 = static_cast<typename sf64<D>::value_type&>(mShares[0](xy));
				auto& s1 = static_cast<typename sf64<D>::value_type&>(mShares[1](xy));

				return Ref<sf64<D>>(s0, s1);
			}

			const sf64Matrix<D>& operator=(const sf64Matrix<D>& B) 
			{
				mShares = B.mShares;
				return B;
			}


			sf64Matrix<D>& operator+=(const sf64Matrix<D>& B)
			{
				mShares[0] += B.mShares[0];
				mShares[1] += B.mShares[1];
				return *this;
			}


			sf64Matrix<D>& operator-=(const sf64Matrix<D>& B)
			{
				mShares[0] += B.mShares[0];
				mShares[1] += B.mShares[1];
				return *this;
			}

			sf64Matrix<D> operator+(const sf64Matrix<D>& B) const
			{
				sf64Matrix<D> r = *this;
				r += B;
				return r;
			}
		    sf64Matrix<D> operator-(const sf64Matrix<D>& B) const
			{
				sf64Matrix<D> r = *this;
				r -= B;
				return r;
			}

			sf64Matrix<D> transpose() const
			{
				sf64Matrix<D> r = *this;
				r.transposeInPlace();
				return r;
			}
			void transposeInPlace()
			{
				mShares[0].transposeInPlace();
				mShares[1].transposeInPlace();
			}


			Row row(u64 i) { return Row{ *this, i }; }
		    Col col(u64 i) { return Col{ *this, i }; }
		    ConstRow row(u64 i) const { return ConstRow{ *this, i }; }
			ConstCol col(u64 i) const { return ConstCol{ *this, i }; }

		    bool operator !=(const sf64Matrix<D>& b) const
		    {
		        return !(*this == b);
		    }

		    bool operator ==(const sf64Matrix<D>& b) const
		    {
				return (rows() == b.rows() &&
					cols() == b.cols() &&
					mShares == b.mShares);
		    }

		};

    }
}
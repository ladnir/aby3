#pragma once
#include <aby3/Common/Defines.h>
//#include <cryptoTools/Common/Matrix.h>
#include <Eigen/Dense>
#include <cryptoTools/Network/Channel.h>
#include <cryptoTools/Common/Matrix.h>

namespace aby3
{
    namespace Sh3
    {
        struct CommPkg {
            oc::Channel mPrev, mNext;
        };

        template<typename T>
        using eMatrix = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
        using i64Matrix = eMatrix<i64>;

        namespace details
        {

            bool areEqualImpl(
                const std::array<oc::MatrixView<u8>, 2>& a,
                const std::array<oc::MatrixView<u8>, 2>& b,
                u64 bitCount);

            template<typename T>
            bool areEqual(
                const std::array<oc::Matrix<T>, 2>& a,
                const std::array<oc::Matrix<T>, 2>& b,
                u64 bitCount)
            {
                std::array<oc::MatrixView<u8>, 2> aa;
                std::array<oc::MatrixView<u8>, 2> bb;

                static_assert(std::is_pod<T>::value, "");
                aa[0] = oc::MatrixView<u8>((u8*)a[0].data(), a[0].rows(), a[0].cols() * sizeof(T));
                aa[1] = oc::MatrixView<u8>((u8*)a[1].data(), a[1].rows(), a[1].cols() * sizeof(T));
                bb[0] = oc::MatrixView<u8>((u8*)b[0].data(), b[0].rows(), b[0].cols() * sizeof(T));
                bb[1] = oc::MatrixView<u8>((u8*)b[1].data(), b[1].rows(), b[1].cols() * sizeof(T));

                return areEqualImpl(aa, bb, bitCount);
            }

            void trimImpl(oc::MatrixView<u8> a, u64 bits);
            template<typename T>
            void trim(oc::MatrixView<T> a, i64 bits)
            {
                static_assert(std::is_pod<T>::value, "");
                oc::MatrixView<u8> aa((u8*)a.data(), a.rows(), a.cols() * sizeof(T));
                trimImpl(aa, bits);
            }
        }
        struct si64;

        // a reference to a si64 64 bit integer (stored in a matrix).
        template<typename ShareType>
        struct Ref
        {
            friend ShareType;
            using ref_value_type = typename ShareType::value_type;

            std::array<ref_value_type*, 2> mData;


            Ref(ref_value_type& a0, ref_value_type& a1)
            {
                mData[0] = &a0;
                mData[1] = &a1;
            }

            const ShareType& operator=(const ShareType& copy);
            ref_value_type& operator[](u64 i) { return *mData[i]; }
            const ref_value_type& operator[](u64 i) const { return *mData[i]; }

        };


        // a shared 64 bit integer
        struct si64
        {
            using value_type = i64;
            std::array<value_type, 2> mData;

            si64() = default;
            si64(const si64&) = default;
            si64(si64&&) = default;
            si64(const std::array<value_type, 2>& d) :mData(d) {}
            si64(const Ref<si64>& s) {
                mData[0] = *s.mData[0];
                mData[1] = *s.mData[1];
            }

            si64& operator=(const si64& copy);
            si64 operator+(const si64& rhs) const;
            si64 operator-(const si64& rhs) const;

            value_type& operator[](u64 i) { return mData[i]; }
            const value_type& operator[](u64 i) const { return mData[i]; }
        };

        // a shared 64 binary value
        struct sb64
        {
            using value_type = i64;
            std::array<i64, 2> mData;

            sb64() = default;
            sb64(const sb64&) = default;
            sb64(sb64&&) = default;
            sb64(const std::array<value_type, 2>& d) :mData(d) {}

            i64& operator[](u64 i) { return mData[i]; }
            const i64& operator[](u64 i) const { return mData[i]; }


            sb64& operator=(const sb64& copy) = default;
            sb64 operator^(const sb64& x) { return { { mData[0] ^ x.mData[0], mData[1] ^ x.mData[1] } }; }

        };

        struct si64Matrix
        {
            std::array<eMatrix<i64>, 2> mShares;

            struct ConstRow { const si64Matrix& mMtx; const u64 mIdx; };
            struct Row { si64Matrix& mMtx; const u64 mIdx; const Row& operator=(const Row& row); const ConstRow& operator=(const ConstRow& row); };

            struct ConstCol { const si64Matrix& mMtx; const u64 mIdx; };
            struct Col { si64Matrix& mMtx; const u64 mIdx; const Col& operator=(const Col& col); const ConstCol& operator=(const ConstCol& row); };

            si64Matrix() = default;
            si64Matrix(u64 xSize, u64 ySize)
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

            Ref<si64> operator()(u64 x, u64 y) const;
            Ref<si64> operator()(u64 xy) const;
            si64Matrix operator+(const si64Matrix& B) const;
            si64Matrix operator-(const si64Matrix& B) const;

            si64Matrix transpose() const;
            void transposeInPlace();


            Row row(u64 i);
            Col col(u64 i);
            ConstRow row(u64 i) const;
            ConstCol col(u64 i) const;

            bool operator !=(const si64Matrix& b) const
            {
                return !(*this == b);
            }

            bool operator ==(const si64Matrix& b) const
            {
                return (rows() == b.rows() &&
                    cols() == b.cols() &&
                    mShares == b.mShares);
            }

        };


        struct sbMatrix
        {
            std::array<oc::Matrix<i64>, 2> mShares;
            u64 mBitCount = 0;
            //struct ConstRow { const si64Matrix& mMtx; const u64 mIdx; };
            //struct Row { si64Matrix& mMtx; const u64 mIdx; const Row& operator=(const Row& row); const ConstRow& operator=(const ConstRow& row); };

            //struct ConstCol { const si64Matrix& mMtx; const u64 mIdx; };
            //struct Col { si64Matrix& mMtx; const u64 mIdx; const Col& operator=(const Col& col); const ConstCol& operator=(const ConstCol& row); };

            sbMatrix() = default;
            sbMatrix(u64 xSize, u64 ySize)
            {
                resize2(xSize, ySize);
            }

            void resize2(u64 xSize, u64 bitCount)
            {
                mBitCount = bitCount;
                auto ySize = (bitCount + 63) / 64;
                mShares[0].resize(xSize, ySize);
                mShares[1].resize(xSize, ySize);
            }


            u64 rows() const { return mShares[0].rows(); }
            u64 i64Size() const { return mShares[0].size(); }
            u64 i64Cols() const { return mShares[0].cols(); }
            u64 bitCount() const { return mBitCount; }

            bool operator !=(const sbMatrix& b) const
            {
                return !(*this == b);
            }

            bool operator ==(const sbMatrix& b) const
            {
                return (rows() == b.rows() &&
                    bitCount() == b.bitCount() &&
                    details::areEqual(mShares, b.mShares, bitCount()));
            }

            void trim()
            {
                for (auto i = 0; i < mShares.size(); ++i)
                {
                    details::trim(mShares[i], bitCount());
                }
            }
        };


        // Represents a packed set of binary secrets. Data is stored in a tranposed format. 
        // The 'ith bit of all the shares are packed together into the i'th row. This allows 
        // efficient SIMD operations. E.g. applying bit[0] = bit[1] ^ bit[2] to all the shares
        // can be performed to 64 shares using one instruction.
        template<typename T = i64>
        struct sPackedBinBase
        {
            static_assert(std::is_pod<T>::value, "must be pod");
            u64 mShareCount;

            std::array<oc::Matrix<T>, 2> mShares;

            sPackedBinBase() = default;
            sPackedBinBase(u64 shareCount, u64 bitCount)
            {
                resize(shareCount, bitCount);
            }

            void resize(u64 shareCount, u64 bitCount)
            {
                mShareCount = shareCount;
                auto bitsPerWord = 8 * sizeof(T);
                auto wordCount = (shareCount + bitsPerWord - 1) / bitsPerWord;
                mShares[0].resize(bitCount, wordCount);
                mShares[1].resize(bitCount, wordCount);
            }

            u64 size() const { return mShares[0].size(); }

            // the number of shares that are stored in this packed (shared) binary matrix.
            u64 shareCount() const { return mShareCount; }

            // the number of bits that each share has.
            u64 bitCount() const { return mShares[0].rows(); }

            // the number of i64s in each row = divCiel(mShareCount, 8 * sizeof(i64))
            u64 simdWidth() const { return mShares[0].cols(); }

            sPackedBinBase<T> operator^(const sPackedBinBase<T>& rhs)
            {
                if (shareCount() != rhs.shareCount() || bitCount() != rhs.bitCount())
                    throw std::runtime_error(LOCATION);

                sPackedBinBase<T> r(shareCount(), bitCount());
                for (u64 i = 0; i < 2; ++i)
                {
                    for (u64 j = 0; j < mShares[0].size(); ++j)
                    {
                        r.mShares[i](j) = mShares[i](j) ^ rhs.mShares[i](j);
                    }
                }
                return r;
            }

            bool operator!=(const sPackedBinBase<T>& b) const
            {
                return !(*this == b);
            }

            bool operator==(const sPackedBinBase<T>& b) const
            {
                return (shareCount() == b.shareCount() &&
                    bitCount() == b.bitCount() &&
                    details::areEqual(mShares, b.mShares, bitCount()));
            }

            void trim()
            {
                for (auto i = 0; i < mShares.size(); ++i)
                {
                    oc::MatrixView<T>s(mShares[i].data(), mShares[i].rows(), mShares[i].cols());
                    details::trim(s, shareCount());
                }
            }

        };

        using sPackedBin = sPackedBinBase<i64>;
        using sPackedBin128 = sPackedBinBase<block>;

        template<typename T>
        inline const T& Ref<T>::operator=(const T & copy)
        {
            mData[0] = (i64*)&copy.mData[0];
            mData[1] = (i64*)&copy.mData[1];
            return copy;
        }

        inline si64& si64::operator=(const si64& copy)
        {
            mData[0] = copy.mData[0];
            mData[1] = copy.mData[1];
            return *this;
        }

        inline si64 si64::operator+(const si64& rhs) const
        {
            si64 ret;
            ret.mData[0] = mData[0] + rhs.mData[0];
            ret.mData[1] = mData[1] + rhs.mData[1];
            return ret;
        }

        inline si64 si64::operator-(const si64& rhs) const
        {
            si64 ret;
            ret.mData[0] = mData[0] - rhs.mData[0];
            ret.mData[1] = mData[1] - rhs.mData[1];
            return ret;
        }

        inline Ref<si64> si64Matrix::operator()(u64 x, u64 y) const
        {
            return Ref<si64>(
                (i64&)mShares[0](x, y),
                (i64&)mShares[1](x, y));
        }

        inline Ref<si64> si64Matrix::operator()(u64 xy) const
        {
            return Ref<si64>(
                (i64&)mShares[0](xy),
                (i64&)mShares[1](xy));
        }

        inline si64Matrix si64Matrix::operator+(const si64Matrix& B) const
        {
            si64Matrix ret;
            ret.mShares[0] = mShares[0] + B.mShares[0];
            ret.mShares[1] = mShares[1] + B.mShares[1];
            return ret;
        }

        inline si64Matrix si64Matrix::operator-(const si64Matrix& B) const
        {
            si64Matrix ret;
            ret.mShares[0] = mShares[0] - B.mShares[0];
            ret.mShares[1] = mShares[1] - B.mShares[1];
            return ret;
        }

        inline si64Matrix si64Matrix::transpose() const
        {
            si64Matrix ret;
            ret.mShares[0] = mShares[0].transpose();
            ret.mShares[1] = mShares[1].transpose();
            return ret;
        }


        inline void si64Matrix::transposeInPlace()
        {
            mShares[0].transposeInPlace();
            mShares[1].transposeInPlace();
        }

        inline si64Matrix::Row si64Matrix::row(u64 i)
        {
            return Row{ *this, i };
        }
        inline si64Matrix::ConstRow si64Matrix::row(u64 i) const
        {
            return ConstRow{ *this, i };
        }
        inline si64Matrix::Col si64Matrix::col(u64 i)
        {
            return Col{ *this, i };
        }
        inline si64Matrix::ConstCol si64Matrix::col(u64 i) const
        {
            return ConstCol{ *this, i };
        }

        inline const si64Matrix::Row & si64Matrix::Row::operator=(const Row & row)
        {
            mMtx.mShares[0].row(mIdx) = row.mMtx.mShares[0].row(row.mIdx);
            mMtx.mShares[1].row(mIdx) = row.mMtx.mShares[1].row(row.mIdx);

            return row;
        }

        inline const si64Matrix::ConstRow & si64Matrix::Row::operator=(const ConstRow & row)
        {
            mMtx.mShares[0].row(mIdx) = row.mMtx.mShares[0].row(row.mIdx);
            mMtx.mShares[1].row(mIdx) = row.mMtx.mShares[1].row(row.mIdx);
            return row;
        }

        inline const si64Matrix::Col & si64Matrix::Col::operator=(const Col & col)
        {
            mMtx.mShares[0].col(mIdx) = col.mMtx.mShares[0].col(col.mIdx);
            mMtx.mShares[1].col(mIdx) = col.mMtx.mShares[1].col(col.mIdx);
            return col;
        }

        inline const si64Matrix::ConstCol & si64Matrix::Col::operator=(const ConstCol & col)
        {
            mMtx.mShares[0].col(mIdx) = col.mMtx.mShares[0].col(col.mIdx);
            mMtx.mShares[1].col(mIdx) = col.mMtx.mShares[1].col(col.mIdx);
            return col;
        }

    }

}
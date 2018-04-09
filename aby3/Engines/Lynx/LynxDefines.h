#pragma once
#include <Eigen/Dense>
#include <cryptoTools/Common/Defines.h>
#include <cryptoTools/Crypto/PRNG.h>
#include <cryptoTools/Network/Session.h>
#include <cryptoTools/Network/Channel.h>
#include <utility>
#include "aby3/Circuit/BetaLibrary.h"
#include <functional>

using namespace oc;

namespace Lynx
{

	typedef i64 Word;
	typedef double value_type;
	typedef Eigen::Matrix<value_type, Eigen::Dynamic, Eigen::Dynamic> value_type_matrix;
	typedef Eigen::Matrix<Word, Eigen::Dynamic, Eigen::Dynamic> word_matrix;

    struct CommPackage
    {
        Channel mNext, mPrev;
    };

	class CompletionHandle
	{
	public:
		std::function<void()> mGet;

		void get() { mGet(); };
	};

	struct Share;

	struct ShareRef
	{
		friend Share;
	private:
		std::array<Word*, 2> mData;
	public:

		ShareRef(Word& a0, Word& a1)
		{
			mData[0] = &a0;
			mData[1] = &a1;
		}

		const Share& operator=(const Share& copy);
		Word& operator[](u64 i) { return *mData[i]; }
		const Word& operator[](u64 i) const { return *mData[i]; }

	};


	struct Share
	{
		friend ShareRef;
	private:
		std::array<Word, 2> mData;
	public:

		Share() = default;
		Share(const Share& share) = default;
		Share(Share&& share) = default;

		Share(const ShareRef& share) {
			mData[0] = *share.mData[0];
			mData[1] = *share.mData[1];
		}

		Share& operator=(const Share& copy);
		Share operator+(const Share& rhs) const;
		Share operator-(const Share& rhs) const;

		Word& operator[](u64 i) { return mData[i]; }
		const Word& operator[](u64 i) const { return mData[i]; }
	};



	struct Matrix
	{
		std::array<word_matrix, 2> mShares;

		struct ConstRow { const Matrix& mMtx; const u64 mIdx; };
		struct Row { Matrix& mMtx; const u64 mIdx; const Row& operator=(const Row& row); const ConstRow& operator=(const ConstRow& row); };

		struct ConstCol { const Matrix& mMtx; const u64 mIdx; };
		struct Col { Matrix& mMtx; const u64 mIdx; const Col& operator=(const Col& col); const ConstCol& operator=(const ConstCol& row); };

		Matrix() = default;
		Matrix(u64 xSize, u64 ySize)
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

		ShareRef operator()(u64 x, u64 y) const;
		ShareRef operator()(u64 xy) const;
		Matrix operator+(const Matrix& B) const;
		Matrix operator-(const Matrix& B) const;

		Matrix transpose() const;
		void transposeInPlace();


		Row row(u64 i);
		Col col(u64 i);
		ConstRow row(u64 i) const;
		ConstCol col(u64 i) const;
	};

    // Represents a packed set of binary secret shares. Data is stored in a tranposed format. 
    // The 'ith bit of all the shares are packed together into the i'th row. This allows 
    // efficient SIMD operations. E.g. applying bit[0] = bit[1] ^ bit[2] to all the shares
    // can be performed to 64 shares using one instruction.
    struct PackedBinMatrix
    {
        u64 mShareCount;

        std::array<word_matrix, 2> mShares;

        PackedBinMatrix() = default;
        PackedBinMatrix(u64 shareCount, u64 bitCount)
        {
            resize(shareCount, bitCount);
        }

        void resize(u64 shareCount, u64 bitCount)
        {
            mShareCount = shareCount;
            auto bitsPerWord = 8 * sizeof(Word);
            auto wordCount = (shareCount + bitsPerWord - 1) / bitsPerWord;
            mShares[0].resize(bitCount, wordCount);
            mShares[1].resize(bitCount, wordCount);
        }

        // the number of shares that are stored in this packed (shared) binary matrix.
        u64 shareCount() const { return mShareCount; }

        // the number of bits that each share has.
        u64 bitCount() const { return mShares[0].rows(); }
        
        // the number of words in each row = divCiel(mShareCount, 8 * sizeof(Word))
        u64 simdWidth() const { return mShares[0].cols(); }

    };

	enum class ShareType
	{
		Arithmetic,
		Binary
	};


    inline const Lynx::Share& Lynx::ShareRef::operator=(const Share & copy)
    {
        mData[0] = (Word*)&copy.mData[0];
        mData[1] = (Word*)&copy.mData[1];
        return copy;
    }

    inline Lynx::Share& Lynx::Share::operator=(const Lynx::Share& copy)
    {
        mData[0] = copy.mData[0];
        mData[1] = copy.mData[1];
        return *this;
    }

    inline Lynx::Share Lynx::Share::operator+(const Lynx::Share& rhs) const
    {
        Share ret;
        ret.mData[0] = mData[0] + rhs.mData[0];
        ret.mData[1] = mData[1] + rhs.mData[1];
        return ret;
    }

    inline Lynx::Share Lynx::Share::operator-(const Lynx::Share& rhs) const
    {
        Share ret;
        ret.mData[0] = mData[0] - rhs.mData[0];
        ret.mData[1] = mData[1] - rhs.mData[1];
        return ret;
    }

    inline Lynx::ShareRef Lynx::Matrix::operator()(u64 x, u64 y) const
    {
        return ShareRef(
            (Word&)mShares[0](x, y),
            (Word&)mShares[1](x, y));
    }

    inline Lynx::ShareRef Lynx::Matrix::operator()(u64 xy) const
    {
        return ShareRef(
            (Word&)mShares[0](xy),
            (Word&)mShares[1](xy));
    }

    inline Lynx::Matrix Lynx::Matrix::operator+(const Matrix& B) const
    {
        Matrix ret;
        ret.mShares[0] = mShares[0] + B.mShares[0];
        ret.mShares[1] = mShares[1] + B.mShares[1];
        return ret;
    }

    inline Lynx::Matrix Lynx::Matrix::operator-(const Matrix& B) const
    {
        Matrix ret;
        ret.mShares[0] = mShares[0] - B.mShares[0];
        ret.mShares[1] = mShares[1] - B.mShares[1];
        return ret;
    }

    inline Lynx::Matrix Lynx::Matrix::transpose() const
    {
        Matrix ret;
        ret.mShares[0] = mShares[0].transpose();
        ret.mShares[1] = mShares[1].transpose();
        return ret;
    }


    inline void Lynx::Matrix::transposeInPlace()
    {
        mShares[0].transposeInPlace();
        mShares[1].transposeInPlace();
    }

    inline Lynx::Matrix::Row Lynx::Matrix::row(u64 i)
    {
        return Row{ *this, i };
    }
    inline Lynx::Matrix::ConstRow Lynx::Matrix::row(u64 i) const
    {
        return ConstRow{ *this, i };
    }
    inline Lynx::Matrix::Col Lynx::Matrix::col(u64 i)
    {
        return Col{ *this, i };
    }
    inline Lynx::Matrix::ConstCol Lynx::Matrix::col(u64 i) const
    {
        return ConstCol{ *this, i };
    }

    inline const Lynx::Matrix::Row & Lynx::Matrix::Row::operator=(const Row & row)
    {
        mMtx.mShares[0].row(mIdx) = row.mMtx.mShares[0].row(row.mIdx);
        mMtx.mShares[1].row(mIdx) = row.mMtx.mShares[1].row(row.mIdx);

        return row;
    }

    inline const Lynx::Matrix::ConstRow & Lynx::Matrix::Row::operator=(const ConstRow & row)
    {
        mMtx.mShares[0].row(mIdx) = row.mMtx.mShares[0].row(row.mIdx);
        mMtx.mShares[1].row(mIdx) = row.mMtx.mShares[1].row(row.mIdx);
        return row;
    }

    inline const Lynx::Matrix::Col & Lynx::Matrix::Col::operator=(const Col & col)
    {
        mMtx.mShares[0].col(mIdx) = col.mMtx.mShares[0].col(col.mIdx);
        mMtx.mShares[1].col(mIdx) = col.mMtx.mShares[1].col(col.mIdx);
        return col;
    }

    inline const Lynx::Matrix::ConstCol & Lynx::Matrix::Col::operator=(const ConstCol & col)
    {
        mMtx.mShares[0].col(mIdx) = col.mMtx.mShares[0].col(col.mIdx);
        mMtx.mShares[1].col(mIdx) = col.mMtx.mShares[1].col(col.mIdx);
        return col;
    }
}
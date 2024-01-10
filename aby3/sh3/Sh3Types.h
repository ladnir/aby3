#pragma once
#include <aby3/Common/Defines.h>
//#include <cryptoTools/Common/Matrix.h>
#include <Eigen/Dense>
#include <cryptoTools/Network/Channel.h>
#include <cryptoTools/Common/Matrix.h>
#include <cryptoTools/Common/BitVector.h>
#include <cryptoTools/Crypto/PRNG.h>
#include <cstddef>
#ifndef NDEBUG
//#define ABY3_RANDOMIZE_MEM
#endif

namespace aby3
{
	template<typename T, typename U>
	span<T> asSpan(U& u)
	{
		auto tt = (T*)u.data();
		auto ss = u.size() * sizeof(*u.data()) / sizeof(T);
		return span<T>(tt, ss);
	}

	std::string hexImpl(span<u8> d);

	template<typename T>
	std::string hex(T& d)
	{
		return hexImpl(asSpan<u8>(d));
	}

	struct CommPkg {
		oc::Channel mPrev, mNext;
	};



	template<typename T>
	using eMatrix = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;


	template<typename T>
	oc::MatrixView<T> view(eMatrix<T>& m)
	{
		return oc::MatrixView<T>(m.data(), m.rows(), m.cols());
	}
	template<typename T>
	oc::MatrixView<const T> view(const eMatrix<T>& m)
	{
		return oc::MatrixView<const T>(m.data(), m.rows(), m.cols());
	}

	using i64Matrix = eMatrix<i64>;

	namespace details
	{

		bool areEqualImpl(
			span<u8> a,
			span<u8> b,
			u64 bitCount);

		template<typename T>
		bool areEqual(
			const span<T>& a,
			const span<T>& b,
			u64 bitCount)
		{
			return areEqualImpl(asSpan<u8>(a), asSpan<u8>(b), bitCount);
		}

		bool areEqualImpl(
			oc::MatrixView<u8> a,
			oc::MatrixView<u8> b,
			u64 bitCount);

		template<typename T>
		bool areEqual(
			const oc::MatrixView<T>& a,
			const oc::MatrixView<T>& b,
			u64 bitCount)
		{
			oc::MatrixView<u8> aa;
			oc::MatrixView<u8> bb;

			static_assert(std::is_pod<T>::value, "");
			aa = oc::MatrixView<u8>((u8*)a.data(), a.rows(), a.cols() * sizeof(T));
			bb = oc::MatrixView<u8>((u8*)b.data(), b.rows(), b.cols() * sizeof(T));

			return areEqualImpl(aa, bb, bitCount);
		}

		template<typename T>
		bool areEqual(
			const std::array<oc::MatrixView<T>, 2>& aa,
			const std::array<oc::MatrixView<T>, 2>& bb,
			u64 bitCount)
		{
			return 
				areEqual(aa[0], bb[0], bitCount) &&
				areEqual(aa[1], bb[1], bitCount);
		}

		template<typename T>
		bool areEqual(
			const std::array<oc::Matrix<T>, 2>& a,
			const std::array<oc::Matrix<T>, 2>& b,
			u64 bitCount)
		{

			std::array<oc::MatrixView<T>, 2> aa{ a[0], a[1] };
			std::array<oc::MatrixView<T>, 2> bb{ b[0], b[1] };

			return areEqual(aa, bb, bitCount);
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

	// a reference to a Share integer (stored in a matrix).
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

	template<typename T>
	struct Share
	{
		using value_type = i64;
		std::array<value_type, 2> mData;

		Share() = default;
		Share(const Share&) = default;
		Share(Share&&) = default;
		Share(const std::array<value_type, 2>&d) :mData(d) {}
		Share(const Ref<Share>&s) {
			mData[0] = *s.mData[0];
			mData[1] = *s.mData[1];
		}

		Share& operator=(const Share & copy);
		Share operator+(const Share & rhs) const;
		Share operator-(const Share & rhs) const;

		value_type& operator[](u64 i) { return mData[i]; }
		const value_type& operator[](u64 i) const { return mData[i]; }
	};

	// a shared 64 bit integer
	using si64 = Share<i64>;

	// an additively shared 64 bit integer
	using sia64 = i64;

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

	template<typename T>
	struct sMatrix
	{
		std::array<eMatrix<T>, 2> mShares;

		struct ConstRow { const sMatrix& mMtx; const u64 mIdx; };
		struct Row { sMatrix& mMtx; const u64 mIdx; const Row& operator=(const Row& row); const ConstRow& operator=(const ConstRow& row); };

		struct ConstCol { const sMatrix& mMtx; const u64 mIdx; };
		struct Col { sMatrix& mMtx; const u64 mIdx; const Col& operator=(const Col& col); const ConstCol& operator=(const ConstCol& row); };

		sMatrix() = default;
		sMatrix(u64 xSize, u64 ySize)
		{
			resize(xSize, ySize);
		}

		void resize(u64 xSize, u64 ySize)
		{
#ifdef ABY3_RANDOMIZE_MEM
			auto oldSize = size();
#endif

			mShares[0].resize(xSize, ySize);
			mShares[1].resize(xSize, ySize);
#ifdef ABY3_RANDOMIZE_MEM

			if (oldSize < size())
			{
				// if we are in debug mode, randomize the memory 
				// to make sure the caller isn't relaying on 
				// and uninitialized memory.
				oc::PRNG dd(block((u64)this, 0));
				dd.get(mShares[0].data() + oldSize, size() - oldSize);
				dd.get(mShares[1].data() + oldSize, size() - oldSize);
			}
#endif // !NDEBUG
		}


		u64 rows() const { return mShares[0].rows(); }
		u64 cols() const { return mShares[0].cols(); }
		u64 size() const { return mShares[0].size(); }

		Ref<Share<T>> operator()(u64 x, u64 y) const;
		Ref<Share<T>> operator()(u64 xy) const;
		sMatrix operator+(const sMatrix& B) const;
		sMatrix operator-(const sMatrix& B) const;

		sMatrix transpose() const;
		void transposeInPlace();


		Row row(u64 i);
		Col col(u64 i);
		ConstRow row(u64 i) const;
		ConstCol col(u64 i) const;

		bool operator !=(const sMatrix& b) const
		{
			return !(*this == b);
		}

		bool operator ==(const sMatrix& b) const
		{
			return (rows() == b.rows() &&
				cols() == b.cols() &&
				mShares == b.mShares);
		}

		eMatrix<T>& operator[](u64 i) { return mShares[i]; }
		const eMatrix<T>& operator[](u64 i) const { return mShares[i]; }
	};

	
	using si64Matrix = sMatrix<i64>;

	//template<Decimal D>
	//struct sf64Matrix
	//{
	//    static const Decimal mDecimal = D;
	//    std::array<eMatrix<i64>, 2> mShares;

	//    struct ConstRow { const sf64Matrix& mMtx; const u64 mIdx; };
	//    struct Row { sf64Matrix& mMtx; const u64 mIdx; const Row& operator=(const Row& row); const ConstRow& operator=(const ConstRow& row); };

	//    struct ConstCol { const sf64Matrix& mMtx; const u64 mIdx; };
	//    struct Col { sf64Matrix& mMtx; const u64 mIdx; const Col& operator=(const Col& col); const ConstCol& operator=(const ConstCol& row); };

	//    sf64Matrix() = default;
	//    sf64Matrix(u64 xSize, u64 ySize)
	//    {
	//        resize(xSize, ySize);
	//    }

	//    void resize(u64 xSize, u64 ySize)
	//    {
	//        mShares[0].resize(xSize, ySize);
	//        mShares[1].resize(xSize, ySize);
	//    }


	//    u64 rows() const { return mShares[0].rows(); }
	//    u64 cols() const { return mShares[0].cols(); }
	//    u64 size() const { return mShares[0].size(); }

	//    Ref<sf64<D>> operator()(u64 x, u64 y) const;
	//    Ref<sf64<D>> operator()(u64 xy) const;
	//    sf64Matrix operator+(const sf64Matrix& B) const;
	//    sf64Matrix operator-(const sf64Matrix& B) const;

	//    sf64Matrix transpose() const;
	//    void transposeInPlace();


	//    Row row(u64 i);
	//    Col col(u64 i);
	//    ConstRow row(u64 i) const;
	//    ConstCol col(u64 i) const;

	//    bool operator !=(const sf64Matrix& b) const
	//    {
	//        return !(*this == b);
	//    }

	//    bool operator ==(const sf64Matrix& b) const
	//    {
	//        return (rows() == b.rows() &&
	//            cols() == b.cols() &&
	//            mShares == b.mShares);
	//    }

	//};



	struct sbMatrix
	{
		std::array<oc::Matrix<i64>, 2> mShares;
		u64 mBitCount = 0;
		//struct ConstRow { const sMatrix<T>& mMtx; const u64 mIdx; };
		//struct Row { sMatrix<T>& mMtx; const u64 mIdx; const Row& operator=(const Row& row); const ConstRow& operator=(const ConstRow& row); };

		//struct ConstCol { const sMatrix<T>& mMtx; const u64 mIdx; };
		//struct Col { sMatrix<T>& mMtx; const u64 mIdx; const Col& operator=(const Col& col); const ConstCol& operator=(const ConstCol& row); };

		sbMatrix() = default;
		sbMatrix(u64 xSize, u64 ySize)
		{
			resize(xSize, ySize);
		}

		void resize(u64 xSize, u64 bitCount)
		{
			mBitCount = bitCount;
			auto ySize = (bitCount + 63) / 64;
			mShares[0].resize(xSize, ySize, oc::AllocType::Uninitialized);
			mShares[1].resize(xSize, ySize, oc::AllocType::Uninitialized);
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

		auto& operator[](u64 i) { return mShares[i]; }
		auto& operator[](u64 i) const { return mShares[i]; }

		void trim()
		{
			for (auto i = 0ull; i < mShares.size(); ++i)
			{
				details::trim(mShares[i], bitCount());
			}
		}
	};

	//using value_type = T;

	//// the number of independent shares being held.
	//u64 mShareCount = 0;

	//std::array<oc::MatrixView<T>, 2> mShares;

	//std::unique_ptr<u8[]> mBacking;
	//u64 mAllocSize = 0;
	//u64 mAlignment = 1;

	//sPackedBinBase() = default;
	//sPackedBinBase(const sPackedBinBase & o) { *this = o; }
	//sPackedBinBase(sPackedBinBase && o)
	//	: mShareCount(std::exchange(o.mShareCount, 0))
	//	, mShares(std::exchange(o.mShares, std::array<oc::MatrixView<T>, 2>{}))
	//	, mBacking(std::exchange(o.mBacking, nullptr))
	//	, mAllocSize(std::exchange(o.mAllocSize, 0))
	//	, mAlignment(std::exchange(o.mAlignment, 1))
	//{};

	//sPackedBinBase(u64 shareCount, u64 bitCount, u64 rowAligment = 1)
	//{
	//	reset(shareCount, bitCount, rowAligment);
	//}

	//sPackedBinBase& operator=(const sPackedBinBase & o)
	//{
	//	reset(o.shareCount(), o.bitCount(), o.mAlignment);

	//	assert(o.mShares[0].size() == mShares[0].size());
	//	memcpy(mShares[0].data(), o.mShares[0].data(), mShares[0].size() * sizeof(T));
	//	memcpy(mShares[1].data(), o.mShares[1].data(), mShares[1].size() * sizeof(T));
	//	return *this;
	//}

	//// resize without copy. 
	////   * shareCount is the number of independent shares
	////     that can be stored. 
	////   * Each share will have bitCount number of bits.
	////   * rowAlignment is the desired alignment of the 
	////     start of each row with respect to T. Each row will 
	////     start on a multiple of `sizeof(T) * rowAlignment`.
	//// In memory the i'th bit of each share will be packed together on a single 'row'.
	//void reset(u64 shareCount, u64 bitCount, u64 rowAligment = 1)
	//{
	//	// the number of bits a single T can hold.
	//	auto bitsPerT = 8 * sizeof(T);

	//	// the number of T's we need to store one row of bits.
	//	auto rowSizeT = oc::roundUpTo(oc::divCeil(shareCount, bitsPerT), rowAligment);

	//	// each bit gets its own row.
	//	auto rowCount = bitCount;

	//	mAlignment = rowAligment;

	// Represents a packed set of binary secrets. Data is stored in a transposed format. 
	// The 'ith bit of all the shares are packed together into the i'th row. This allows 
	// efficient SIMD operations. E.g. applying bit[0] = bit[1] ^ bit[2] to all the shares
	// can be performed to 64 shares using one instruction.
	template<typename T = i64>
	struct sPackedBinBase
	{
		static_assert(std::is_pod<T>::value, "must be pod");
		
		// the number of independent shares being held.
		u64 mShareCount = 0;

		std::array<oc::MatrixView<T>, 2> mShares;

		std::unique_ptr<u8[]> mBacking;
		u64 mAllocSize = 0;
		u64 mAlignment = 1;

		sPackedBinBase() = default;
		sPackedBinBase(const sPackedBinBase& o) { *this = o; }
		sPackedBinBase(sPackedBinBase && o) noexcept
			: mShareCount(std::exchange(o.mShareCount, 0))
			, mShares(std::exchange(o.mShares, std::array<oc::MatrixView<T>, 2>{}))
			, mBacking(std::exchange(o.mBacking, nullptr))
			, mAllocSize(std::exchange(o.mAllocSize, 0))
			, mAlignment(std::exchange(o.mAlignment, 1))
		{};

		sPackedBinBase(u64 shareCount, u64 bitCount, u64 rowAligment = 1)
		{
			reset(shareCount, bitCount, rowAligment);
		}

		sPackedBinBase& operator=(const sPackedBinBase & o)
		{
			assert(this != &o);
			reset(o.shareCount(), o.bitCount(), o.mAlignment);

			if (o.mShares[0].size() == mShares[0].size() &&
				o.mShares[0].cols() == mShares[0].cols())
			{
				memcpy(mShares[0].data(), o.mShares[0].data(), mShares[0].size() * sizeof(T));
				memcpy(mShares[1].data(), o.mShares[1].data(), mShares[1].size() * sizeof(T));
			}
			else
			{
				auto r = bitCount();
				auto y = oc::divCeil(shareCount(),8);
				auto& s0 = o.mShares[0];
				auto& s1 = o.mShares[1];
				auto& d0 = mShares[0];
				auto& d1 = mShares[1];

				for (u64 i = 0; i < r; ++i)
				{
					memcpy(&d0(i, 0), &s0(i,0), y);
					memcpy(&d1(i, 0), &s1(i,0), y);

					if (details::areEqual(s0[i], d0[i], shareCount()) == false)
					{
						std::cout << "s " << hex(s0[i]) << std::endl;
						std::cout << "d " << hex(d0[i]) << std::endl;
						assert(details::areEqual(s0[i], d0[i], shareCount()));
					}
					assert(details::areEqual(s1[i], d1[i], shareCount()));
				}
			}

			assert(shareCount() == o.shareCount());
			assert(bitCount() == o.bitCount());
			assert(details::areEqual(mShares, o.mShares, shareCount()));

			return *this;
		}

		void reshape(u64 shareCount)
		{
			if (shareCount > mShares[0].cols() * sizeof(*mShares[0].data()) * 8)
				throw RTE_LOC;

			mShareCount = shareCount;
		}

		// resize without copy. 
		//   * shareCount is the number of independent shares
		//     that can be stored. 
		//   * Each share will have bitCount number of bits.
		//   * rowAlignment is the desired alignment of the 
		//     start of each row with respect to T. Each row will 
		//     start on a multiple of `sizeof(T) * rowAlignment`.
		// In memory the i'th bit of each share will be packed together on a single 'row'.
		void reset(u64 shareCount, u64 bitCount, u64 rowAligment = 1)
		{
			// the number of bits a single T can hold.
			auto bitsPerT = 8 * sizeof(T);

			// the number of T's we need to store one row of bits.
			auto rowSizeT = oc::divCeil(shareCount, rowAligment * bitsPerT) * rowAligment;

			// each bit gets its own row.
			auto rowCount = bitCount;

			mAlignment = rowAligment;

			// check if we can just no-op.
			if (shareCount != mShareCount || 
				rowSizeT != mShares[0].stride() ||
				rowCount != mShares[0].rows())
			{

				mShareCount = shareCount;

				// The number of T's we will use. We need, each of size rowSizeT
				// and we have two shares we times that by 2.
				auto sizeT = rowCount * rowSizeT * 2;

				// the alignment each row should have in term of bytes.
				auto byteAlignment = sizeof(T) * rowAligment;

				// we will alloc:
				// * sizeT T's, plus
				// * (2 * byteAlignment) to make sure we start on the right alignment, plus
				//   byteAlignment to make sure we can always cast the last element to something
				//   of size byteAlignment.
				auto newSize = sizeof(T) * sizeT + 2 * byteAlignment;
				if (newSize > mAllocSize)
				{
					mAllocSize = newSize;
					mBacking = {};
					mBacking.reset(new u8[mAllocSize]);
				}

				void* ptr = reinterpret_cast<void*>(mBacking.get());

				// align the start of our data. We have sizeBytes worth of
				// data that we can work with since the last byteAlignment
				// is reserved to casting the last element to something of
				// size byteAlignment.
				auto sizeBytes = sizeof(T) * sizeT + byteAlignment;

				// perform the alignment, ptr and sizeBytes are updated. 
				if (!std::align(byteAlignment, sizeT * sizeof(T), ptr, sizeBytes))
					throw RTE_LOC;

				T* aligned0 = static_cast<T*>(ptr);
				T* aligned1 = aligned0 + rowCount * rowSizeT;

				mShares[0] = oc::MatrixView<T>(aligned0, rowCount, rowSizeT);
				mShares[1] = oc::MatrixView<T>(aligned1, rowCount, rowSizeT);

				if ((u8*)(mShares[1].data() + mShares[1].size()) > mBacking.get() + mAllocSize)
					throw RTE_LOC;
			}
		}

		u64 size() const { return mShares[0].size(); }

		// the number of shares that are stored in this packed (shared) binary matrix.
		u64 shareCount() const { return mShareCount; }

		// the number of bits that each share has.
		u64 bitCount() const { return mShares[0].rows(); }

		// the number of T's in each row
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
				details::areEqual(mShares, b.mShares, shareCount()));
		}

		void trim()
		{
			for (auto i = 0ull; i < mShares.size(); ++i)
			{
				oc::MatrixView<T>s(mShares[i].data(), mShares[i].rows(), mShares[i].cols());
				details::trim(s, shareCount());
			}
		}

	};

	using sPackedBin = sPackedBinBase<i64>;
	using sPackedBin128 = sPackedBinBase<block>;

	template<typename T = i64>
	struct PackedBinBase
	{
		static_assert(std::is_pod<T>::value, "must be pod");
		u64 mShareCount;

		oc::Matrix<T> mData;

		PackedBinBase() = default;
		PackedBinBase(u64 shareCount, u64 bitCount, u64 wordMultiple = 1)
		{
			resize(shareCount, bitCount, wordMultiple);
		}

		void resize(u64 shareCount, u64 bitCount, u64 wordMultiple = 1)
		{
			mShareCount = shareCount;
			auto bitsPerWord = 8 * sizeof(T);
			auto wordCount = (shareCount + bitsPerWord - 1) / bitsPerWord;
			wordCount = oc::roundUpTo(wordCount, wordMultiple);
			mData.resize(bitCount, wordCount, oc::AllocType::Uninitialized);


//#ifdef ABY3_RANDOMIZE_MEM
//			// if we are in debug mode, randomize the memory 
//			// to make sure the caller isn't relaying on 
//			// and uninitialized memory.
//			oc::PRNG dd(block((u64)this, 0));
//			dd.get(mData.data(), mData.size());
//#endif // !NDEBUG
		}

		u64 size() const { return mData.size(); }

		// the number of shares that are stored in this packed (shared) binary matrix.
		u64 shareCount() const { return mShareCount; }

		// the number of bits that each share has.
		u64 bitCount() const { return mData.rows(); }

		// the number of i64s in each row = divCiel(mShareCount, 8 * sizeof(i64))
		u64 simdWidth() const { return mData.cols(); }

		PackedBinBase<T> operator^(const PackedBinBase<T>& rhs)
		{
			if (shareCount() != rhs.shareCount() || bitCount() != rhs.bitCount())
				throw std::runtime_error(LOCATION);

			PackedBinBase<T> r(shareCount(), bitCount());
			for (u64 j = 0; j < mData.size(); ++j)
			{
				r.mData(j) = mData(j) ^ rhs.mData(j);
			}
			return r;
		}

		void trim()
		{
			details::trim(mData, shareCount());
		}
	};

	using PackedBin = PackedBinBase<i64>;
	using PackedBin128 = PackedBinBase<block>;


	template<typename T>
	inline const T& Ref<T>::operator=(const T& copy)
	{
		mData[0] = (i64*)&copy.mData[0];
		mData[1] = (i64*)&copy.mData[1];
		return copy;
	}

	// template<typename T>
	// inline const T& Refa<T>::operator=(const T &copy)
	// {
	//     mData = (i64*)copy.mData;
	//     return copy;
	// }

	// inline sia64& sia64::operator=(const sia64& copy)
	// {
	//     mData = copy.mData;
	//     return *this;
	// }

	// inline sia64 sia64::operator+(const sia64& rhs) const
	// {
	//     sia64 ret;
	//     ret.mData = mData + rhs.mData;
	//     return ret;
	// }

	// inline sia64 sia64::operator-(const sia64& rhs) const 
	// {
	//     sia64 ret; 
	//     ret.mData = mData - rhs.mData;
	//     return ret;
	// }

	template<typename T>
	inline Share<T>& Share<T>::operator=(const Share<T>& copy)
	{
		mData[0] = copy.mData[0];
		mData[1] = copy.mData[1];
		return *this;
	}

	template<typename T>
	inline Share<T> Share<T>::operator+(const Share<T>& rhs) const
	{
		Share<T> ret;
		ret.mData[0] = mData[0] + rhs.mData[0];
		ret.mData[1] = mData[1] + rhs.mData[1];
		return ret;
	}

	template<typename T>
	inline Share<T> Share<T>::operator-(const Share<T>& rhs) const
	{
		Share<T> ret;
		ret.mData[0] = mData[0] - rhs.mData[0];
		ret.mData[1] = mData[1] - rhs.mData[1];
		return ret;
	}

	template<typename T>
	inline Ref<Share<T>> sMatrix<T>::operator()(u64 x, u64 y) const
	{
		return Ref<Share<T>>(
			(T&)mShares[0](x, y),
			(T&)mShares[1](x, y));
	}

	template<typename T>
	inline Ref<Share<T>> sMatrix<T>::operator()(u64 xy) const
	{
		return {
			(T&)mShares[0](xy),
			(T&)mShares[1](xy) };
	}

	template<typename T>
	inline sMatrix<T> sMatrix<T>::operator+(const sMatrix<T>& B) const
	{
		sMatrix<T> ret;
		ret.mShares[0] = mShares[0] + B.mShares[0];
		ret.mShares[1] = mShares[1] + B.mShares[1];
		return ret;
	}

	template<typename T>
	inline sMatrix<T> sMatrix<T>::operator-(const sMatrix<T>& B) const
	{
		sMatrix<T> ret;
		ret.mShares[0] = mShares[0] - B.mShares[0];
		ret.mShares[1] = mShares[1] - B.mShares[1];
		return ret;
	}

	template<typename T>
	inline sMatrix<T> sMatrix<T>::transpose() const
	{
		sMatrix<T> ret;
		ret.mShares[0] = mShares[0].transpose();
		ret.mShares[1] = mShares[1].transpose();
		return ret;
	}


	template<typename T>
	inline void sMatrix<T>::transposeInPlace()
	{
		mShares[0].transposeInPlace();
		mShares[1].transposeInPlace();
	}

	template<typename T>
	inline typename sMatrix<T>::Row sMatrix<T>::row(u64 i)
	{
		return Row{ *this, i };
	}
	template<typename T>
	inline typename sMatrix<T>::ConstRow sMatrix<T>::row(u64 i) const
	{
		return ConstRow{ *this, i };
	}
	template<typename T>
	inline typename sMatrix<T>::Col sMatrix<T>::col(u64 i)
	{
		return Col{ *this, i };
	}
	template<typename T>
	inline typename sMatrix<T>::ConstCol sMatrix<T>::col(u64 i) const
	{
		return ConstCol{ *this, i };
	}

	template<typename T>
	inline const typename  sMatrix<T>::Row& sMatrix<T>::Row::operator=(const Row& row)
	{
		mMtx.mShares[0].row(mIdx) = row.mMtx.mShares[0].row(row.mIdx);
		mMtx.mShares[1].row(mIdx) = row.mMtx.mShares[1].row(row.mIdx);

		return row;
	}

	template<typename T>
	inline const typename sMatrix<T>::ConstRow& sMatrix<T>::Row::operator=(const ConstRow& row)
	{
		mMtx.mShares[0].row(mIdx) = row.mMtx.mShares[0].row(row.mIdx);
		mMtx.mShares[1].row(mIdx) = row.mMtx.mShares[1].row(row.mIdx);
		return row;
	}

	template<typename T>
	inline const typename sMatrix<T>::Col& sMatrix<T>::Col::operator=(const Col& col)
	{
		mMtx.mShares[0].col(mIdx) = col.mMtx.mShares[0].col(col.mIdx);
		mMtx.mShares[1].col(mIdx) = col.mMtx.mShares[1].col(col.mIdx);
		return col;
	}

	template<typename T>
	inline const typename  sMatrix<T>::ConstCol& sMatrix<T>::Col::operator=(const ConstCol& col)
	{
		mMtx.mShares[0].col(mIdx) = col.mMtx.mShares[0].col(col.mIdx);
		mMtx.mShares[1].col(mIdx) = col.mMtx.mShares[1].col(col.mIdx);
		return col;
	}

	template<typename T>
	struct sMatrixView
	{
		sMatrixView() = default;
		sMatrixView(const sMatrixView&) = default;
		sMatrixView& operator=(const sMatrixView&) = default;

		// construct a const view from non-const 
		template<typename U>
		sMatrixView(sMatrix<U>& dst)
		{
			mShares[0] = ::aby3::view<U>(dst.mShares[0]);
			mShares[1] = ::aby3::view<U>(dst.mShares[1]);
		}
		// construct a const view from non-const 
		template<typename U>
		sMatrixView(const sMatrix<U>& dst)
		{
			mShares[0] = ::aby3::view<U>(dst.mShares[0]);
			mShares[1] = ::aby3::view<U>(dst.mShares[1]);
		}

		// 
		//	typename std::enable_if<
		//	std::is_same<
		//	U,
		//	typename std::remove_const<T>::type>::value&&
		//	std::is_const<U>::value == false>::type* = 0
		//sMatrixView(sMatrix<T>& dst)
		//{
		//	mShares[0] = view<T>(dst.mShares[0]);
		//	mShares[1] = view<T>(dst.mShares[1]);
		//}

		sMatrixView(sbMatrix& dst)
		{
			static_assert(std::is_same<T, i64>::value, "assumed");
			mShares[0] = oc::MatrixView<i64>(dst.mShares[0]);
			mShares[1] = oc::MatrixView<i64>(dst.mShares[1]);
		}

		sMatrixView(const sbMatrix& dst)
		{
			static_assert(std::is_same<T, const i64>::value, "assumed");
			mShares[0] = oc::MatrixView<const i64>(dst.mShares[0].data(), dst.mShares[0].rows(), dst.mShares[0].cols());
			mShares[1] = oc::MatrixView<const i64>(dst.mShares[1].data(), dst.mShares[1].rows(), dst.mShares[1].cols());
		}

		//template<typename T>
		//inline sMatrixView<const T> view(const sMatrix<T>& dst)
		//{
		//	return {
		//		view(dst.mShares[0]),
		//		view(dst.mShares[1])
		//	};
		//}

		std::array<oc::MatrixView<T>, 2> mShares;

		u64 rows() const { return mShares[0].rows(); }
		u64 cols() const { return mShares[0].cols(); }
		u64 size() const { return mShares[0].size(); }

		oc::MatrixView<T>& operator[](u64 i) { return mShares[i]; }
		const oc::MatrixView<T>& operator[](u64 i) const { return mShares[i]; }
	};



	namespace details
	{

		template<typename T>
		bool areEqual(oc::MatrixView<T>& a, oc::MatrixView<T>& b, oc::BitVector& mask)
		{
			if (a.rows() != b.rows())
				return false;

			if (a.cols() != b.cols())
				return false;

			auto u8Size = std::min<u64>(mask.sizeBytes(), sizeof(T) * a.cols());

			auto r = mask.size() % 8;
			if (r)
			{
				u8 m = ~(~1 << r);
				mask.data()[mask.size() / 8] &= m;
			}

			for (u64 i = 0; i < a.rows(); ++i)
			{
				auto aPtr = (u8*)&a(i, 0);
				auto bPtr = (u8*)&b(i, 0);
				for (u64 j = 0; j < u8Size; ++j)
				{
					auto v = (aPtr[j] ^ bPtr[j]) & mask.data()[j];
					if (v)
						return false;
				}
			}

			return true;
		}

		template<typename T>
		bool areEqual(sMatrixView<T> a, sMatrixView<T> b, oc::BitVector& mask)
		{
			return
				areEqual(a[0], b[0], mask) &&
				areEqual(a[1], b[1], mask);
		}


		template<typename T>
		bool trim(oc::MatrixView<T>& a, oc::BitVector& mask)
		{
			auto u8Size = std::min<u64>(mask.sizeBytes(), sizeof(T) * a.cols());
			auto u8Rem = sizeof(T) * a.cols() - u8Size;

			auto r = mask.size() % 8;
			if (r)
			{
				u8 m = ~(~1 << r);
				mask.data()[mask.size() / 8] &= m;
			}

			for (u64 i = 0; i < a.rows(); ++i)
			{
				auto aPtr = (u8*)&a(i, 0);
				for (u64 j = 0; j < u8Size; ++j)
				{
					aPtr[j] &= mask.data()[j];
				}
			}

			return true;
		}

		template<typename T>
		void trim(sMatrixView<T> a, oc::BitVector& mask)
		{
			trim(a[0], mask);
			trim(a[1], mask);
		}


		inline void trim(sPackedBin& a)
		{
			trim(a.mShares[0], a.shareCount());
			trim(a.mShares[1], a.shareCount());
		}
	}


}
#pragma once
#include "cryptoTools/Common/Defines.h"
#ifdef ENABLE_CIRCUITS

#include "cryptoTools/Common/BitVector.h"
#include <functional>
#include <sstream>
#include <unordered_set>

namespace osuCrypto
{
	namespace Mx
	{

		class Circuit;

		enum class OpType {

			Zero = 0,   //0000,
			Nor = 1,    //0001
			nb_And = 2, //0010
			nb = 3,     //0011
			na_And = 4, //0100
			na = 5,     //0101
			Xor = 6,    //0110
			Nand = 7,   //0111
			And = 8,    //1000
			Nxor = 9,   //1001
			a = 10,     //1010
			nb_Or = 11, //1011
			b = 12,     //1100
			na_Or = 13, //1101
			Or = 14,    //1110
			One = 15,   //1111
			Input,
			Output,
			Print,
			Other
		};

		inline std::ostream& operator<<(std::ostream& o, const OpType&ot)
		{
			o << [&]() {
				switch (ot)
				{
				case OpType::Zero: return "Zero";
				case OpType::Nor: return "Nor";
				case OpType::nb_And: return "nb_And";
				case OpType::nb: return "nb";
				case OpType::na_And: return "na_And";
				case OpType::na: return "na";
				case OpType::Xor: return "Xor";
				case OpType::Nand: return "Nand";
				case OpType::And: return "And";
				case OpType::Nxor: return "Nxor";
				case OpType::a: return "a";
				case OpType::nb_Or: return "nb_Or";
				case OpType::b: return "b";
				case OpType::na_Or: return "na_Or";
				case OpType::Or: return "Or";
				case OpType::One: return "One";
				case OpType::Input: return "Input";
				case OpType::Output: return "Output";
				case OpType::Print: return "Print";
				case OpType::Other: return "Other";
				default:
					throw RTE_LOC;
					break;
				}
			}();

			return o;
		}

		inline bool isLinear(OpType type)
		{
			return
				type == OpType::Xor ||
				type == OpType::Nxor ||
				type == OpType::a ||
				type == OpType::Zero ||
				type == OpType::nb ||
				type == OpType::na ||
				type == OpType::b ||
				type == OpType::One;
		}



		template<typename T, int S>
		struct SmallVector
		{
			//static_assert(std::is_trivial_v<T>, "impl assumes this");


			std::array<T, S> mBuff;
			std::unique_ptr<T[]> mPtr;
			u64 mSize = 0;
			static_assert(sizeof(mBuff) >= sizeof(u64), "buffer must be at least this big");

			SmallVector()
				: mBuff{}
				, mPtr{}
				, mSize{ 0 }
			{}

			SmallVector(SmallVector&& m) noexcept
			{
				mBuff = m.mBuff;
				mSize = m.mSize;
				mPtr = std::move(m.mPtr);
			}

			T& operator[](u64 i)
			{
				if (i >= mSize)
					throw RTE_LOC;
				if (isSmall())
					return mBuff[i];
				else
					return mPtr.get()[i];
			}

			const T& operator[](u64 i)const
			{
				if (i >= mSize)
					throw RTE_LOC;
				if (isSmall())
					return mBuff[i];
				else
					return mPtr.get()[i];
			}

			void setCap(u64 c)
			{
				memcpy(&mBuff, &c, sizeof(u64));
			}

			u64 capacity() const
			{
				if (isSmall())
					return S;
				u64 c;
				memcpy(&c, &mBuff, sizeof(u64));
				return c;
			}

			void reserve(u64 n)
			{
				if (n > capacity())
				{
					auto b = data();
					auto e = b + mSize;
					std::unique_ptr<T[]> vv(new T[n]);
					std::copy(b, e, vv.get());
					setCap(n);
					mPtr = std::move(vv);
				}
			}

			T& front() { return (*this)[0]; }
			T& back() { return (*this)[mSize - 1]; }
			const T& front() const { return (*this)[0]; }
			const T& back() const { return (*this)[mSize - 1]; }

			template<typename T2>
			void push_back(T2&& v)
			{
				if (isSmall() && mSize < S)
				{
					mBuff[mSize] = v;
					++mSize;
				}
				else
				{
					if (mSize == capacity())
						reserve(std::max<u64>(mSize * 2, 32));

					mPtr.get()[mSize] = v;
					++mSize;
				}
			}

			void pop_back()
			{
				--mSize;
			}

			bool isSmall() const {
				return mPtr.get() == nullptr;
			}

			T* data()
			{
				if (isSmall())
					return mBuff.data();
				else
					return mPtr.get();
			}

			u64 size() const {
				return mSize;
			}

			T* begin() { return data(); }
			T* end() { return data() + size(); }
			const T* begin() const { return data(); }
			const T* end() const { return data() + size(); }
		};


		template<typename T, int S>
		struct SmallSet
		{
			//static const u64 MidCap = 32;
			SmallVector<T, S> mBuff;
			//std::unordered_set<T> mSet;
			//u64 mSize = 0;

			//bool isLarge() const { return mBuff.size() == MidCap + 1; }

			void insert(T& v)
			{

				if (std::find(mBuff.begin(), mBuff.end(), v) == mBuff.end())
					mBuff.push_back(v);
				//if (isLarge())
				//	mSet.insert(v);
				//else
				//{
				//	if (mBuff.size() == MidCap)
				//	{
				//		for (auto& x : mBuff)
				//			mSet.insert(x);

				//		mSet.insert(v);
				//	}
				//	else
				//	{
				//		if(std::find(mBuff.begin(), mBuff.end(), v) == mBuff.end())
				//			mBuff.push_back(v);
				//	}
				//}
			}

			void erase(T& v)
			{
				auto iter = std::find(mBuff.begin(), mBuff.end(), v);
				if (iter == mBuff.end())
					throw RTE_LOC;
				std::swap(mBuff.back(), *iter);
				mBuff.pop_back();

				//if (isLarge())
				//{
				//	auto i = mSet.find(v);
				//	if (i == mSet.end())
				//		throw RTE_LOC;
				//	mSet.erase(i);
				//}
				//else
				//{
				//	auto iter = std::find(mBuff.begin(), mBuff.end(), v);
				//	if (iter == mBuff.end())
				//		throw RTE_LOC;
				//	std::swap(mBuff.back(), *iter);
				//	mBuff.pop_back();
				//}
			}

			template<typename F>
			void forEach(F&& f)
			{
				for (u64 i = 0; i < mBuff.size(); ++i)
				{
					f(mBuff[i]);
				}
				//if (isLarge())
				//{
				//	for (auto b = mSet.begin(); b != mSet.end(); ++b)
				//	{
				//		f(*b);
				//	}
				//}
				//else
				//{
				//	for (u64 i = 0; i < mBuff.size(); ++i)
				//	{
				//		f(mBuff[i]);
				//	}
				//}
			}

			u64 size() const
			{
				//if (isLarge())
				//	return mSet.size();
				//else
				return mBuff.size();
			}
		};


		struct Address
		{
			Address() = default;
			Address(const Address&) = default;
			Address&operator=(const Address&) = default;
			Address(u64 g, u64 o)
				:mVal(g | (o << 40))
			{}

			// the gate index the produced this output.
			u64 gate() const
			{
				assert(hasValue());
				return (u64(-1) >> 24) & mVal;
			}

			// the offset of this output in that gate, typically 0.
			u64 offset() const
			{
				assert(hasValue());
				return i64(mVal) >> 40;
			}

			bool hasValue() const  { return mVal != u64(-1); }
		private:
			u64 mVal = u64(-1);
		};

		inline std::ostream& operator<<(std::ostream& o, const Address& a)
		{
			o << a.gate() << "." << a.offset();
			return o;
		}

		struct Bit
		{
			using representation_type = Bit;

			Circuit* mCir = nullptr;
			Address mAddress;

			Bit() = default;
			Bit(const Bit& o)
			{
				*this = o;
			}
			Bit(bool v)
			{
				*this = v;
			}

			Circuit* circuit() const
			{
				assert(!isConst());
				return mCir;
			}
			bool isConst() const
			{
				return ((u64)mCir <= 1);
			}
			bool constValue() const
			{
				if (isConst() == false)
					throw RTE_LOC;
				else
					return ((u64)mCir) & 1;
			}
			Bit& operator=(const Bit& o) = default;
			Bit& operator=(bool b);

			Bit operator^(const Bit& b)const;
			Bit operator&(const Bit& b)const;
			Bit operator|(const Bit& b)const;
			Bit operator!() const;
			Bit operator~() const;

			template<typename T> 
			T select(const T& IfTrue, const T& IfFalse) const
			{
				T ret = IfTrue;
				auto d = ret.deserialize();
				auto bOne = IfTrue.serialize();
				auto bZero = IfFalse.serialize();

				if (bZero.size() != d.size() || bOne.size() != d.size())
					throw RTE_LOC;

				for (u64 i = 0; i < d.size(); ++i)
					*d[i] = *bZero[i] ^ ((*bZero[i] ^ *bOne[i]) & *this);

				return ret;
			}

			Bit addGate(OpType t, const Bit& b) const;

			static std::function<std::string(const BitVector& b)> toString()
			{
				return [](const BitVector& b) {
					std::stringstream ss;
					ss << b;
					return ss.str();
					};
			}

			std::array<const Bit*, 1> serialize() const
			{
				return std::array<const Bit*, 1>{ { this } };
			}
			std::array<Bit*, 1> deserialize()
			{
				return std::array<Bit*, 1>{ { this } };
			}
		};



		template<u64 n>
		struct AInt
		{
			AInt() = default;
			AInt(const AInt&) = default;
			AInt(AInt&&) = default;
			AInt& operator=(const AInt&) = default;
			AInt& operator=(AInt&&) = default;

		};
	}


}
#endif
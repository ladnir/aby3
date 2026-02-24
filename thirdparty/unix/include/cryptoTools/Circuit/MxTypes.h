#pragma once
#include "cryptoTools/Common/Defines.h"
#ifdef ENABLE_CIRCUITS

#include "MxCircuit.h"
#include <vector>
#include <array>
#include "MxCircuitLibrary.h"
#include "cryptoTools/Common/BitVector.h"

namespace osuCrypto
{
	namespace Mx
	{
		// a crtp that implement the bitwise operations for type C.
		template<typename C>
		struct VectorTrait
		{
		private:
			C& self()
			{
				return *(C*)this;
			}

			const C& self() const
			{
				return *(const C*)this;
			}

		public:
			C operator&(const C& b)const
			{
				if (self().size() != b.size())
					throw RTE_LOC;
				auto r = C::makeFromSize(self().size());
				for (u64 i = 0; i < self().size(); ++i)
					r[i] = self()[i] & b[i];
				return r;
			}

			C& operator&=(const C&b) {
				*this = *this & b;
				return *this;
			}


			C operator|(const C& b)const
			{
				if (self().size() != b.size())
					throw RTE_LOC;
				auto r = C::makeFromSize(self().size());
				for (u64 i = 0; i < self().size(); ++i)
					r[i] = self()[i] | b[i];
				return r;
			}

			C& operator|=(const C& b) {
				*this = *this | b;
				return *this;
			}

			C operator^(const C& b)const
			{
				if (self().size() != b.size())
					throw RTE_LOC;
				auto r = C::makeFromSize(self().size());
				for (u64 i = 0; i < self().size(); ++i)
					r[i] = self()[i] ^ b[i];
				return r;
			}

			C& operator^=(const C& b) {
				*this = *this ^ b;
				return *this;
			}

			C operator~()const
			{
				auto r = C::makeFromSize(self().size());
				for (u64 i = 0; i < self().size(); ++i)
					r[i] = ~self()[i];
				return r;
			}

			Bit operator!=(const C& b)const
			{
				return !(*this == b);
			}

			Bit operator==(const C& b)const
			{
				return parallelEquality(self().asBits(), b.asBits());
			}
			Bit operator!()const
			{
				auto z = C::makeFromSize(self().size());
				z = 0;
				return *this == z;
			}

			auto begin() { return self().asBits().begin(); }
			auto end() { return self().asBits().end(); }
			auto begin() const { return self().asBits().begin(); }
			auto end() const { return self().asBits().end(); }
			auto size() const { return self().asBits().size(); }

			Bit& operator[](u64 i) { return self().asBits()[i]; }
			const Bit& operator[](u64 i) const { return self().asBits()[i]; }

			Bit& front() { return self().asBits().front(); }
			Bit& back() { return self().asBits().back(); }
			const Bit& front() const { return self().asBits().front(); }
			const Bit& back() const { return self().asBits().back(); }


			std::vector<const Bit*> serialize() const
			{
				std::vector<const Bit*> r(size());
				for (u64 i = 0; i < size(); ++i)
					r[i] = &(*this)[i];
				return r;
			}

			std::vector<Bit*> deserialize()
			{
				std::vector<Bit*> r(size());
				for (u64 i = 0; i < size(); ++i)
					r[i] = &(*this)[i];
				return r;
			}

			static std::function<std::string(const BitVector& b)> toString()
			{
				return [](const BitVector& b) {
					std::stringstream ss;
					ss << b;
					return ss.str();
					};
			}
		};


		template<u64 n>
		struct BitArray : public std::array<Bit, n>, VectorTrait<BitArray<n>>
		{
			using representation_type = Bit;
			static const u64 mSize = n;

			BitArray() = default;
			BitArray(const BitArray&) = default;
			BitArray(BitArray&&) = default;
			BitArray& operator=(const BitArray&) = default;
			BitArray& operator=(BitArray&&) = default;


			using std::array<Bit, n>::size;
			using std::array<Bit, n>::back;
			using std::array<Bit, n>::front;
			using std::array<Bit, n>::begin;
			using std::array<Bit, n>::end;
			using std::array<Bit, n>::operator[];
			std::array<Bit, n>& asBits() { return *this; }
			const std::array<Bit, n>& asBits() const { return *this; }
		};


		struct BVector : public std::vector<Bit>, VectorTrait<BVector>
		{
			using representation_type = Bit;

			BVector() = default;
			BVector(const BVector&) = default;
			BVector(BVector&&) = default;
			BVector& operator=(const BVector&) = default;
			BVector& operator=(BVector&&) = default;

			template<typename Iter>
			BVector(Iter&& b, Iter&& e)
			{
				resize(std::distance(b, e));
				for (u64 i = 0; i < size(); ++i)
					(*this)[i] = *b++;
			}

			BVector(u64 n)
			{
				resize(n);
			}

			using std::vector<Bit>::size;
			using std::vector<Bit>::back;
			using std::vector<Bit>::front;
			using std::vector<Bit>::begin;
			using std::vector<Bit>::end;
			using std::vector<Bit>::operator[];

			std::vector<Bit>& asBits() { return *this; }
			const std::vector<Bit>& asBits() const { return *this; }

		};

		// a crtp that implement the integer operations for type C.
		template<typename C, IntType Signed>
		struct IntegerTraits
		{
		private:
			C& self()
			{
				return *(C*)this;
			}

			const C& self() const
			{
				return *(const C*)this;
			}

		public:

			using value_type = std::conditional_t<Signed == IntType::TwosComplement, i64, u64>;

			C& operator=(value_type v)
			{
				for (u64 i = 0; i < self().size(); ++i)
				{
					self().asBits()[i] = v & 1;
					v >>= 1;
				}
				return self();
			}

			C operator+(const C& b)const
			{
				auto r = C::makeFromSize(self().size());
				add(self().asBits(), b.asBits(), r.asBits(), Signed, AdderType::Addition, Optimized::Depth);
				return r;
			}

			C& operator+=(const C&b) {
				*this = *this + b; 
				return *this;
			}

			C operator-(const C& b)const
			{
				auto r = C::makeFromSize(self().size());
				add(self().asBits(), b.asBits(), r.asBits(), Signed, AdderType::Subtraction, Optimized::Depth);
				return r;
			}

			C& operator-=(const C&b) {
				*this = *this - b;
				return *this;
			}

			C operator-() const
			{
				auto r = C::makeFromSize(self().size());
				negate(self().asBits(), r.asBits(), Optimized::Depth);
				return r;
			}

			C operator*(const C& b)const
			{
				auto r = C::makeFromSize(self().size());
				multiply(self().asBits(), b.asBits(), r.asBits(), Optimized::Depth, Signed);
				return r;
			}

			C& operator*=(const C&b) {
				*this = *this * b;
				return *this;
			}


			C operator/(const C& b)const
			{
				auto r = C::makeFromSize(self().size());
				divideRemainder(self().asBits(), b.asBits(), r.asBits(), {}, Optimized::Depth, Signed);
				return r;
			}

			C operator%(const C& b) const
			{
				auto d = C::makeFromSize(self().size());
				auto r = C::makeFromSize(self().size());
				divideRemainder(self().asBits(), b.asBits(), d.asBits(), r.asBits(), Optimized::Depth, Signed);
				return r;
			}

			Bit operator<(const C& b)const
			{
				Bit r;
				lessThan(self().asBits(), b.asBits(), r, Signed, Optimized::Depth);
				return r;
			}

			Bit operator>(const C& b)const
			{
				return b < self();
			}

			Bit operator<=(const C& b)const
			{
				return !(self() > b);
			}

			Bit operator>=(const C& b)const
			{
				return b <= self();
			}

			C operator>>(const u64 v) const
			{
				if (v > self().size())
					throw RTE_LOC;

				auto r = C::makeFromSize(self().size());
				for (u64 i = 0, j = v; j < self().size(); ++i, ++j)
					r[i] = self()[j];

				for (u64 j = self().size() - v; j < self().size(); ++j)
					r[j] = (Signed == IntType::TwosComplement) ? self().back() : 0;

				return r;
			}

			C operator<<(const u64 v) const
			{
				if (v > self().size())
					throw RTE_LOC;

				auto r = C::makeFromSize(self().size());
				for (u64 i = 0, j = v; j < self().size(); ++i, ++j)
					r[j] = self()[i];

				return r;
			}

			static std::function<std::string(const BitVector& b)> toString()
			{
				return [](const BitVector& b) {

					if (b.size() > 64)
						throw std::runtime_error("not implemented. " LOCATION);
					value_type v = 0;
					for (u64 i = 0; i < b.size(); ++i)
					{
						v |= u64(b[i]) << i;
					}
					if (Signed == IntType::TwosComplement)
					{
						for (u64 i = b.size(); i < 64; ++i)
						{
							v |= u64(b[b.size() - 1]) << i;
						}
					}
					return std::to_string(v);
					};
			}
		};

		// an integer with n bits using signed (twos complement) or unsigned representation.
		template<u64 n, IntType Signed = IntType::TwosComplement>
		struct BInt :
			// pulls in the basic bit operations
			public VectorTrait<BInt<n, Signed>>,
			// pulls in the addition and comparisons operations.
			IntegerTraits<BInt<n, Signed>, Signed>
		{
			BitArray<n> mBits;

			using representation_type = Bit;
			using typename IntegerTraits<BInt, Signed>::value_type;
			using IntegerTraits<BInt<n, Signed>, Signed>::toString;

			BInt() = default;
			BInt(const BInt&) = default;
			BInt(BInt&&) = default;
			BInt& operator=(const BInt&) = default;
			BInt& operator=(BInt&&) = default;

			BInt(value_type v)
			{
				*(IntegerTraits<BInt, Signed>*)this = v;
			}

			auto& asBits() { return mBits; }
			auto& asBits() const { return mBits; }
			static BInt makeFromSize(u64 s)
			{
				if (s != n)
					throw RTE_LOC;
				return BInt{};
			}

			template<u64 n2, IntType S>
			operator BInt<n2, S>() const&
			{
				BInt<n2, S> r;
				for (u64 i = 0; i < std::min<u64>(n2, this->size()); ++i)
				{
					r[i] = (*this)[i];
				}
				if (Signed == IntType::TwosComplement && S == IntType::TwosComplement)
				{
					for (u64 i = this->size(); i < n2; ++i)
						r[i] = r[i - 1];
				}
				return r;
			}

			template<u64 n2, IntType S>
			operator BInt<n2, S>()&&
			{
				BInt<n2, S> r;
				for (u64 i = 0; i < std::min<u64>(n2, this->size()); ++i)
				{
					r[i] = std::move((*this)[i]);
				}
				if (Signed == IntType::TwosComplement && S == IntType::TwosComplement)
				{
					for (u64 i = this->size(); i < n2; ++i)
						r[i] = r[i - 1];
				}
				return r;
			}


		};

		template<u64 n>
		using BUInt = BInt<n, IntType::Unsigned>;


		// a dynamically sized int.
		template<IntType Signed = IntType::TwosComplement>
		struct BDynXInt : public VectorTrait<BDynXInt<Signed>>, IntegerTraits<BDynXInt<Signed>, Signed>
		{
			BVector mBits;
			using representation_type = Bit;
			using typename IntegerTraits<BDynXInt, Signed>::value_type;
			using IntegerTraits<BDynXInt, Signed>::toString;
			using VectorTrait<BDynXInt<Signed>>::size;

			BDynXInt() = default;
			BDynXInt(const BDynXInt&) = default;
			BDynXInt(BDynXInt&&) = default;
			BDynXInt& operator=(const BDynXInt&) = default;
			BDynXInt& operator=(BDynXInt&&) = default;
			using IntegerTraits<BDynXInt, Signed>::operator=;

			BDynXInt(u64 size, value_type value = 0)
			{
				resize(size);
				*(IntegerTraits<BDynXInt, Signed>*)this = value;
			}

			template<u64 n, IntType S>
			BDynXInt(const BInt<n, S>& v)
			{
				resize(n);
				for (u64 i = 0; i < n; ++i)
					mBits[i] = v[i];
			}
			template<u64 n, IntType S>
			BDynXInt(BInt<n, S>&& v)
			{
				resize(n);
				for (u64 i = 0; i < n; ++i)
					mBits[i] = std::move(v[i]);
			}

			void resize(u64 size)
			{
				mBits.resize(size);
			}
			auto& asBits() { return mBits; }
			auto& asBits() const { return mBits; }
			static BDynXInt makeFromSize(u64 s)
			{
				return BDynXInt(s);
			}
			template<u64 n, IntType S>
			operator BInt<n, S>() const&
			{
				BInt<n, S> r;
				for (u64 i = 0; i < std::min<u64>(n, size()); ++i)
				{
					r[i] = (*this)[i];
				}
				if (Signed == IntType::TwosComplement && S == IntType::TwosComplement)
				{
					for (u64 i = size(); i < n; ++i)
						r[i] = r[i - 1];
				}
				return r;
			}

			template<u64 n, IntType S>
			operator BInt<n, S>()&&
			{
				BInt<n, S> r;
				for (u64 i = 0; i < std::min<u64>(n, size()); ++i)
				{
					r[i] = std::move((*this)[i]);
				}
				if (Signed == IntType::TwosComplement && S == IntType::TwosComplement)
				{
					for (u64 i = size(); i < n; ++i)
						r[i] = r[i - 1];
				}
				return r;
			}



		};

		using BDynInt = BDynXInt<IntType::TwosComplement>;
		using BDynUInt = BDynXInt<IntType::Unsigned>;


	}
}
#endif

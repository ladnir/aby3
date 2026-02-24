#pragma once
#include <iterator>
#include "Defines.h"
namespace osuCrypto
{
	template<typename T, T step = 1>
	class Increment
	{
	public:
		inline void operator()(T& v) const
		{
			v += step;
		}
	};
	template<typename T, T step = 1>
	class Deccrement
	{
	public:
		inline void operator()(T& v) const
		{
			v -= step;
		}
	};

	template<typename T, typename Inc = Increment<T>>
	class Range
	{
	public:

		struct Iterator
		{
			T mVal;
			Inc mInc;

			template<typename V, typename I>
			Iterator(V&& v,I&&i)
				: mVal(std::forward<V>(v))
				, mInc(std::forward<I>(i))
			{}

			T operator*() const { return mVal; }

			Iterator& operator++()
			{
				mInc(mVal);
				return *this;
			}
			Iterator operator++(int) const
			{
				auto v = *this;
				mInc(v.mVal);
				return v;
			}

			bool operator==(const Iterator& v) const
			{
				return v.mVal == mVal;
			}

			bool operator!=(const Iterator& v) const
			{
				return v.mVal != mVal;
			}
		};

		Iterator mBegin, mEnd;

		auto begin() const { return mBegin; }
		auto end() const { return mEnd; }

		template<typename B, typename E>
		Range(B&& begin, E&& end, Inc&& step)
			: mBegin(std::forward<B>(begin), step)
			, mEnd(std::forward<E>(end), std::move(step))
		{}
	};



	template<typename T, typename B,typename E, typename Inc>
	Range<T, Inc> rng(B&& begin, E&& end, Inc&& inc)
	{
		return Range<T, Inc>(std::forward<B>(begin), std::forward<E>(end), std::forward<Inc>(inc));
	}

	template<typename T = u64, typename B, typename E>
	Range<T> rng(B&& begin, E&& end)
	{
		using Inc = Increment<T, 1>;
		return rng<T,B,E, Inc>(std::forward<B>(begin), std::forward<E>(end), Inc{});
	}

	template<typename T = u64, typename V>
	Range<T> rng(V&& end)
	{
		return rng<T>(0, std::forward<V>(end));
	}



}
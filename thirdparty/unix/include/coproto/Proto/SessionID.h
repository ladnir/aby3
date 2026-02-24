#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.



#include "coproto/Common/Defines.h"
#include <functional>
#include <random>
#include <iostream>
#include <iomanip>
#include <ios>

namespace coproto
{
	struct SessionID;
	std::ostream& operator<<(std::ostream& out, const SessionID& s);

	struct SessionID
	{
	private:
		SessionID(u64 v0, u64 v1)
		{
			mVal[0] = v0;
			mVal[1] = v1;
		}
	public:
		u64 mVal[2] = { 0,0 };
		u64 mChildIdx = 0;


		SessionID() = default;
		SessionID(const SessionID&) = default;
		SessionID& operator=(const SessionID&) = default;

		SessionID derive()
		{

			auto bHash = [](u64 v)
			{
				return v * (v - 1) * (v - 2) - 23 * v;
			};

			SessionID ret;
			ret.mVal[0] = mVal[0];
			ret.mVal[1] = mVal[1];

			// feistel like "hash function". TODO, use real hash function
			for (u64 i = 0; i < 5; ++i)
			{
				u64& l = ret.mVal[(i & 1) ^ 0];
				u64& r = ret.mVal[(i & 1) ^ 1];
				l = bHash(r + mChildIdx + 32243534) ^ l ^ 9478532833;
			}
			ret.mVal[0] ^= mVal[0];
			ret.mVal[1] ^= mVal[1];

			++mChildIdx;
			return ret;
		}

		bool operator==(const SessionID& o) const
		{
			return
				mVal[0] == o.mVal[0] &&
				mVal[1] == o.mVal[1];
		}
		bool operator!=(const SessionID& o) const { return !(*this == o); }


		static SessionID random()
		{
			SessionID ret;
			std::random_device rd;
			ret.mVal[0] = (u64(rd()) << 32) | rd();
			ret.mVal[1] = (u64(rd()) << 32) | rd();
			return ret;
		}

		static SessionID root()
		{

			return { 45234ull, 325434 };
		}
	};


	inline std::ostream& operator<<(std::ostream& out, const SessionID& s)
	{
		//using namespace coproto; 
		std::ios_base::fmtflags f(out.flags());
		out << std::hex;
		out << std::setw(16) << std::setfill('0') << s.mVal[1]
			<< std::setw(16) << std::setfill('0') << s.mVal[0];
		out.flags(f);
		return out;
	}
}


// custom specialization of std::hash can be injected in namespace std
template<>
struct std::hash<coproto::SessionID>
{
	std::size_t operator()(coproto::SessionID const& s) const noexcept
	{
		return s.mVal[0];
	}
};

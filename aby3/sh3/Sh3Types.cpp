#include "Sh3Types.h"
#include <iomanip>

namespace aby3
{

	std::string hexImpl(span<u8> d)
	{
		std::stringstream ss;
		for (u64 i = d.size() -1; i < d.size(); --i)
			ss << std::hex << std::setw(2) << std::setfill('0') << int(d[i]);
		return ss.str();
	}
}

bool aby3::details::areEqualImpl(
	oc::MatrixView<u8> a,
	oc::MatrixView<u8> b,
	u64 bitCount)
{
	auto mod8 = bitCount & 7;
	auto div8 = bitCount >> 3;
	auto acols = a.cols();
	auto bcols = b.cols();
	if (a.rows() != b.rows())
		throw RTE_LOC;
	if (a.cols() * 8 < bitCount)
		throw RTE_LOC;
	if (b.cols() * 8 < bitCount)
		throw RTE_LOC;

	if (mod8 || acols != div8 || bcols != div8)
	{
		if (div8 > acols)
			throw RTE_LOC;
		if (div8 > bcols)
			throw RTE_LOC;

		u8 mask = mod8 ? ((1 << mod8) - 1) : ~0;

		auto stride = (bitCount + 7) / 8;
		if (stride > a.stride())
			throw RTE_LOC;

		for (u64 i = 0; i < a.rows(); ++i)
		{

			if (div8)
			{
				auto c1 = memcmp(a[i].data(), b[i].data(), div8);
				if (c1)
					return false;
			}

			if (mod8)
			{
				auto cc = a(i, div8) ^ b(i, div8);
				if (mask & cc)
					return false;
			}
		}
		return true;
	}
	else
	{
		if (div8 > acols)
			throw RTE_LOC;

		return memcmp(a.data(), b.data(), a.size()) == 0;
	}
}


bool aby3::details::areEqualImpl(
	span<u8> a,
	span<u8> b,
	u64 bitCount)
{
	auto mod8 = bitCount & 7;
	auto div8 = bitCount >> 3;
	if (a.size() * 8 < bitCount)
		throw RTE_LOC;
	if (b.size() * 8 < bitCount)
		throw RTE_LOC;

	if (mod8)
	{
		u8 mask = mod8 ? ((1 << mod8) - 1) : ~0;

		if (div8)
		{
			auto c1 = memcmp(a.data(), b.data(), div8);
			if (c1)
				return false;
		}

		if (mod8)
		{
			auto cc = a[div8] ^ b[div8];
			if (mask & cc)
				return false;
		}
		return true;
	}
	else
	{
		return memcmp(a.data(), b.data(), div8) == 0;
	}
}


void aby3::details::trimImpl(oc::MatrixView<u8> a, u64 bits)
{
	if (bits > a.stride() * 8)
		throw std::runtime_error(LOCATION);

	if (bits == a.stride() * 8)
		return;

	auto mod8 = bits & 7;
	auto div8 = bits >> 3;
	u8 mask = mod8 ? ((1 << mod8) - 1) : 0;

	for (u64 j = 0; j < a.rows(); ++j)
	{
		auto i = div8;
		a(j, i) &= mask;
		++i;
		for (; i < a.stride(); ++i)
			a(j, i) = 0;
	}
}

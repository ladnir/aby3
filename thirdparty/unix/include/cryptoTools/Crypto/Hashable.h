#pragma once
// This file and the associated implementation has been placed in the public domain, waiving all copyright. No restrictions are placed on its use.
#include <cryptoTools/Common/Defines.h>
#include <type_traits>
#include <cstring>

namespace osuCrypto {
	// Specializations of Hashable should inherit from std::true_type and contain:
	//
	// template<typename Hasher>
	// static void hash(const T& t, Hasher& mHasher);
	//
	// Hasher will contain an Update method that can be applied to byte arrays and to Hashable
	// types.
	template<typename T, typename Enable = void>
	struct Hashable : std::false_type {};

	template<typename T>
	struct Hashable<T, typename std::enable_if<std::is_pod<T>::value>::type> : std::true_type
	{
		template<typename Hasher>
		static void hash(const T& t, Hasher& hasher)
		{
			hasher.Update((u8*) &t, sizeof(T));
		}
	};
}

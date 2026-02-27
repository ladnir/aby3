#pragma once

#include "macoro/config.h"
#ifdef MACORO_VARIANT_LITE_V
#ifndef variant_CONFIG_SELECT_VARIANT
	#define variant_CONFIG_SELECT_VARIANT 1
#endif
#include "nonstd/variant.hpp"
#else
#include <variant>
#endif

namespace macoro
{
#ifdef MACORO_VARIANT_LITE_V
	using nonstd::variant;
	using nonstd::in_place_index;
	using nonstd::get;

#define MACORO_VARIANT_NAMESPACE nonstd

#else
	using std::variant;
	using std::in_place_index;
	using std::get;
#define MACORO_VARIANT_NAMESPACE std

#endif
}
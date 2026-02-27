#pragma once

#include "macoro/config.h"
#ifdef MACORO_OPTIONAL_LITE_V
#ifndef optional_CONFIG_SELECT_OPTIONAL
	#define optional_CONFIG_SELECT_OPTIONAL 1
#endif
#include "nonstd/optional.hpp"
#else
#include <optional>
#endif

namespace macoro
{
#ifdef MACORO_OPTIONAL_LITE_V
	using nonstd::optional;
	using nonstd::nullopt;
#else
	using std::optional;
	using std::nullopt;
#endif
}
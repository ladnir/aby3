///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See github.com/lewissbaker/cppcoro LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <exception>

namespace macoro
{
	class operation_cancelled : public std::exception
	{
	public:

		operation_cancelled() noexcept
			: std::exception()
		{}

		const char* what() const noexcept override { return "operation cancelled"; }
	};
}


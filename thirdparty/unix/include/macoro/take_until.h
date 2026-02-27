#pragma once

#include "stop.h"
#include "task.h"
#include <chrono>
#include "type_traits.h"
#include "result.h"
#include "macoro/macros.h"
#include "macoro/wrap.h"

namespace macoro
{

	template<typename T>
	void request_stop(T&& t)
	{
		t.request_stop();
	}

	template<typename AWAITABLE, typename UNTIL>
	task<remove_rvalue_reference_t<awaitable_result_t<AWAITABLE>>> take_until(AWAITABLE awaitable, UNTIL until)
	{
		using return_type = awaitable_result_t<AWAITABLE>;
		constexpr bool is_void = std::is_void<awaitable_result_t<AWAITABLE>>::value;
		using storage = std::conditional_t< is_void, int, std::optional<return_type>>;
		storage v;
		std::exception_ptr ex;
		try {
			if constexpr (is_void)
				co_await std::move(awaitable);
			else
				v.emplace(co_await(std::move(awaitable)));
		}
		catch (...) {
			ex = std::current_exception();
		}


		request_stop(until);

		try {
			co_await std::move(until);
		}
		catch(...)
		{ }

		if (ex)
			std::rethrow_exception(ex);

		if constexpr (is_void == false)
			co_return static_cast<awaitable_result_t<AWAITABLE>>(v.value());
	}
}

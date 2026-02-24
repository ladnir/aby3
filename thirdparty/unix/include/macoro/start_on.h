#pragma once
#include "macoro/task.h"
#include "macoro/type_traits.h"
#include "macoro/transfer_to.h"

namespace macoro
{
	template<typename scheduler, typename awaitable>
		auto start_on(scheduler& s, awaitable& a)
		-> eager_task<remove_rvalue_reference_t<awaitable_result_t<awaitable>>>
	{
		co_await transfer_to(s);
		co_return co_await a;
	}
	template<typename scheduler, typename awaitable>
		auto start_on(scheduler& s, awaitable a)
		-> eager_task<remove_rvalue_reference_t<awaitable_result_t<awaitable>>>
	{
		co_await transfer_to(s);
		co_return co_await std::move(a);
	}

	template<typename Scheduler>
	struct start_on_t
	{
		template<typename S>
		start_on_t(S&& s)
			: scheduler(std::forward<S>(s))
		{}

		Scheduler scheduler;
	};

	template<typename scheduler>
	start_on_t<scheduler&> start_on(scheduler& s)
	{
		return start_on_t<scheduler&>{ s };
	}

	template<typename scheduler>
	start_on_t<scheduler> start_on(scheduler&& s)
	{
		return start_on_t<scheduler>{ s };
	}

	template<typename awaitable, typename scheduler>
	decltype(auto) operator|(awaitable&& a, start_on_t<scheduler> s)
	{
		return start_on(s.scheduler, std::forward<awaitable>(a));
	}

}



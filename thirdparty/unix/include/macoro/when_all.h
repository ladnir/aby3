#pragma once
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See github.com/lewissbaker/cppcoro LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include "macoro/type_traits.h"
#include <tuple>
#include <atomic>
#include "macoro/coroutine_handle.h"
#include "macoro/detail/when_all_awaitable.h"
#include "macoro/detail/when_all_task.h"
#include <utility>

namespace macoro
{

	template<typename... Awaitables,
		enable_if_t<
			conjunction<
				is_awaitable<
					remove_reference_and_wrapper_t<Awaitables>
				>...
			>::value, int> = 0
		>
	MACORO_NODISCARD
	auto when_all_ready(Awaitables&&... awaitables)
	{
		return detail::when_all_ready_awaitable<
				std::tuple<
					detail::when_all_task<
						awaitable_result_t<
							remove_reference_and_wrapper_t<Awaitables>
						>
					>...
				>
			>
			(std::make_tuple(
				detail::make_when_all_task<remove_rvalue_reference_t<Awaitables>>(
					std::forward<Awaitables>(awaitables)
				)...
			)
		);
	}



	// TODO: Generalise this from vector<AWAITABLE> to arbitrary sequence of awaitable.

	template<
		typename AWAITABLE,
		typename RESULT = awaitable_result_t<remove_reference_wrapper_t<AWAITABLE>>>
	MACORO_NODISCARD
	auto when_all_ready(std::vector<AWAITABLE> awaitables)
	{
		std::vector<detail::when_all_task<RESULT>> tasks;

		tasks.reserve(awaitables.size());

		for (auto& awaitable : awaitables)
		{
			tasks.emplace_back(detail::make_when_all_task(std::move(awaitable)));
		}

		return detail::when_all_ready_awaitable<std::vector<detail::when_all_task<RESULT>>>(
			std::move(tasks));
	}
}
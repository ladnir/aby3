
#pragma once

#include <functional>
#include <optional>
#include "macoro/coroutine_handle.h"
#ifdef MACORO_HAS_STD_STOP_TOKEN
#include <stop_token>
#else
#include "macoro/detail/stop_callback.h"
#include "macoro/detail/stop_source.h"
#include "macoro/detail/stop_state.h"
#include "macoro/detail/stop_token.h"
#endif

namespace macoro
{
#ifdef MACORO_HAS_STD_STOP_TOKEN
	using stop_token = std::stop_token;
	using stop_source = std::stop_source;
	using stop_callback = std::stop_callback<std::function<void()>>;
#endif

	using optional_stop_callback = std::optional<stop_callback>;

	class stop_awaiter
	{
	public:
		stop_awaiter(stop_token&& t)
			:mtoken(std::move(t))
		{}
		stop_awaiter() = default;
		stop_awaiter(const stop_awaiter&) = delete;
		stop_awaiter(stop_awaiter&&) = delete;

		stop_token mtoken;
		optional_stop_callback mReg;
		bool await_ready() { return false; }

		std::coroutine_handle<> await_suspend(std::coroutine_handle<> h)
		{
			return await_suspend(coroutine_handle<>(h)).std_cast();
		}
		coroutine_handle<> await_suspend(coroutine_handle<> h)
		{
			if (mtoken.stop_possible() && !mtoken.stop_requested())
			{
				mReg.emplace(mtoken, [h] {
					h.resume();
					});

				return noop_coroutine();
			}
			return h;
		}
		void await_resume() {}
	};


	inline stop_awaiter MACORO_OPERATOR_COAWAIT(stop_token t)
	{
		return std::move(t);
	}
}

#pragma once
#include "macoro/coroutine_handle.h"
#include "macoro/type_traits.h"
#include <stdexcept>
#include <string>

namespace macoro
{


	namespace detail
	{

		template<typename continuation, typename T>
		enable_if_t<has_set_continuation_member<continuation, T>::value>
			set_continuation(continuation&& c, T&& mTask)
		{
			c.promise().set_continuation(mTask);
		}

		template<typename continuation, typename T>
		enable_if_t<!has_set_continuation_member<continuation, T>::value>
			set_continuation(continuation&& c, T&& mTask)
		{}

		template<typename T, typename continuation>
		auto convert_handle(const continuation& c)
		{
#ifdef MACORO_CPP_20
			using traits_T = coroutine_handle_traits<typename std::remove_reference<T>::type>;
			using traits_C = coroutine_handle_traits<continuation>;
			using promise_type = typename traits_C::promise_type;
			using handle = typename std::conditional<
				// if we have a macoro handle
				std::is_same<typename traits_C::template coroutine_handle<void>, coroutine_handle<void>>::value&&
				// and we are requesting a std handle
				std::is_same<typename traits_T::template coroutine_handle<void>, std::coroutine_handle<void>>::value,
				// then we give a void std handle
				std::coroutine_handle<void>,
				// otherwise we give back the requesting handle type
				typename traits_T::template coroutine_handle<promise_type>
			>::type;

			return static_cast<handle>(c);
#else
			return c;
#endif
		}
		

		// a basic awaiter that will get the caller to 
		// run the continuation that it contains. continuation
		// should be std::coroutine_handle<T> or macoro::coroutine_handle<T>
		// depending on the use.
		template<typename continuation = coroutine_handle<>, bool check_ready = false>
		struct continuation_awaiter
		{
			continuation cont;
			bool await_ready() const noexcept {
				if (check_ready)
					return !cont || cont.done();
				else
					return false;
			}

			template<typename T>
			auto await_suspend(T&&mTask) noexcept
			{
				set_continuation(cont, mTask);
				return convert_handle<T>(cont);
			}

			void await_resume() const noexcept {}
		};
	}

	struct broken_promise : public std::logic_error
	{

		broken_promise()
			: std::logic_error("broken promise")
		{}

		broken_promise(const char* str)
			: std::logic_error(std::string("broken promise: ") + str)
		{}
	};

}


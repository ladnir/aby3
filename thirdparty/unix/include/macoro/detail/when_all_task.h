///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See github.com/lewissbaker/cppcoro LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#pragma once


#include "macoro/coroutine_handle.h"
#include "when_all_awaitable.h"

#include "macoro/type_traits.h"
#include "macoro/macros.h"
#include "macoro/trace.h"

#include <cassert>
#include <utility>

#ifdef MACORO_CPP_20
#define MACORO_CPP_20_WHEN_ALL 1
#endif

namespace macoro
{
	namespace detail
	{
		template<typename TASK_CONTAINER>
		class when_all_ready_awaitable;

		template<typename RESULT>
		class when_all_task;

		template<typename RESULT>
		class when_all_task_promise final
		{
		public:
#ifdef MACORO_CPP_20
			using std_coroutine_handle_t = std::coroutine_handle<when_all_task_promise<RESULT>>;
#endif
			using coroutine_handle_t = coroutine_handle<when_all_task_promise<RESULT>>;

			when_all_task_promise() noexcept
			{}

			auto get_return_object() noexcept
			{
				m_type = coroutine_handle_type::std;
				return coroutine_handle_t::from_promise(*this, m_type);
			}
			auto macoro_get_return_object() noexcept
			{
				m_type = coroutine_handle_type::macoro;
				return coroutine_handle_t::from_promise(*this, m_type);
			}

			suspend_always initial_suspend() noexcept
			{
				return{};
			}

			auto final_suspend() noexcept
			{
				class completion_notifier
				{
				public:

					bool await_ready() const noexcept { return false; }
#ifdef MACORO_CPP_20
					void await_suspend(std_coroutine_handle_t coro) const noexcept
					{
						await_suspend(coroutine_handle_t(coro));
					}
#endif

					void await_suspend(coroutine_handle_t coro) const noexcept
					{
						coro.promise().m_awaitable->notify_awaitable_completed();
					}

					void await_resume() const noexcept {}

				};

				return completion_notifier{};
			}

			void unhandled_exception() noexcept
			{
				m_exception = std::current_exception();
			}

			void return_void() noexcept
			{
				// We should have either suspended at co_yield point or
				// an exception was thrown before running off the end of
				// the coroutine.
				assert(false);
			}

			auto yield_value(RESULT&& result) noexcept
			{
				m_result = std::addressof(result);
				return final_suspend();
			}

			void start(when_all_ready_awaitable_base& awaitable) noexcept
			{
				m_awaitable = &awaitable;
				coroutine_handle_t::from_promise(*this, m_type).resume();
			}

			RESULT& result()&
			{
				rethrow_if_exception();
				return *m_result;
			}

			RESULT&& result()&&
			{
				rethrow_if_exception();
				return std::forward<RESULT>(*m_result);
			}


			//private:

			void rethrow_if_exception()
			{
				if (m_exception)
				{
					std::rethrow_exception(m_exception);
				}
			}

			traceable* get_traceable()
			{
				return m_awaitable;
			}

			coroutine_handle_type m_type;
			when_all_ready_awaitable_base* m_awaitable = nullptr;
			std::exception_ptr m_exception;
			std::add_pointer_t<RESULT> m_result;

		};

		template<>
		class when_all_task_promise<void> final
		{
		public:
#ifdef MACORO_CPP_20
			using std_coroutine_handle_t = std::coroutine_handle<when_all_task_promise<void>>;
#endif
			using coroutine_handle_t = coroutine_handle<when_all_task_promise<void>>;

			when_all_task_promise() noexcept
			{}

			auto get_return_object() noexcept
			{
				m_type = coroutine_handle_type::std;
				return coroutine_handle_t::from_promise(*this, m_type);
			}
			auto macoro_get_return_object() noexcept
			{
				m_type = coroutine_handle_type::macoro;
				return coroutine_handle_t::from_promise(*this, m_type);
			}

			suspend_always initial_suspend() noexcept
			{
				return{};
			}

			auto final_suspend() noexcept
			{
				class completion_notifier
				{
				public:

					bool await_ready() const noexcept { return false; }
#ifdef MACORO_CPP_20
					void await_suspend(std_coroutine_handle_t coro) const noexcept
					{
						coro.promise().m_awaitable->notify_awaitable_completed();
					}
#endif

					void await_suspend(coroutine_handle_t coro) const noexcept
					{
						coro.promise().m_awaitable->notify_awaitable_completed();
					}

					void await_resume() const noexcept {}

				};

				return completion_notifier{};
			}

			void unhandled_exception() noexcept
			{
				m_exception = std::current_exception();
			}

			void return_void() noexcept
			{
			}

			void start(when_all_ready_awaitable_base& awaitable) noexcept
			{
				m_awaitable = &awaitable;
				coroutine_handle_t::from_promise(*this, m_type).resume();
			}

			void result()
			{
				if (m_exception)
				{
					std::rethrow_exception(m_exception);
				}
			}

			//private:

			when_all_ready_awaitable_base* m_awaitable;
			coroutine_handle_type m_type;
			//when_all_counter* m_counter;
			std::exception_ptr m_exception;

		};

		template<typename RESULT>
		class when_all_task final
		{
		public:

			using promise_type = when_all_task_promise<RESULT>;
#ifdef MACORO_CPP_20
			using std_coroutine_handle_t = typename promise_type::std_coroutine_handle_t;
#endif
			using coroutine_handle_t = typename promise_type::coroutine_handle_t;

			when_all_task(coroutine_handle_t coroutine) noexcept
				: m_coroutine(coroutine)
			{}
#ifdef MACORO_CPP_20
			when_all_task(std_coroutine_handle_t coroutine) noexcept
				: m_coroutine(coroutine_handle_t(coroutine))
			{}
#endif

			when_all_task(when_all_task&& other) noexcept
				: m_coroutine(std::exchange(other.m_coroutine, coroutine_handle_t{}))
			{}

			~when_all_task()
			{
				if (m_coroutine) m_coroutine.destroy();
			}

			when_all_task(const when_all_task&) = delete;
			when_all_task& operator=(const when_all_task&) = delete;

			decltype(auto) result()&
			{
				return m_coroutine.promise().result();
			}

			decltype(auto) result()&&
			{
				return std::move(m_coroutine.promise()).result();
			}

			//private:

			template<typename TASK_CONTAINER>
			friend class when_all_ready_awaitable;

			void start(when_all_ready_awaitable_base& awaiter) noexcept
			{
				m_coroutine.promise().start(awaiter);
			}

			coroutine_handle_t m_coroutine;

		};

#ifdef MACORO_CPP_20_WHEN_ALL
		template<
			typename AWAITABLE>
		when_all_task<typename awaitable_traits<AWAITABLE>::await_result_t> make_when_all_task(AWAITABLE awaitable)
		{
			static_assert(std::is_rvalue_reference_v<AWAITABLE> == false, "awaitable must be an value or lvalue reference");
			using RESULT = typename awaitable_traits<AWAITABLE>::await_result_t;
			if constexpr(std::is_lvalue_reference_v<AWAITABLE>)
			{
				if constexpr (!std::is_void<RESULT>::value)
				{
					co_yield co_await awaitable;
				}
				else
				{
					co_await awaitable;
				}
			}
			else
			{
				if constexpr (!std::is_void<RESULT>::value)
				{
					co_yield co_await static_cast<AWAITABLE&&>(awaitable);
				}
				else
				{
					co_await static_cast<AWAITABLE&&>(awaitable);
				}
			}
		}

#else
		template<
			typename AWAITABLE,
			typename RESULT = typename awaitable_traits<AWAITABLE&&>::await_result_t,
			enable_if_t<!std::is_void<RESULT>::value, int> = 0>
		when_all_task<RESULT> make_when_all_task(AWAITABLE a)
		{
			MC_BEGIN(when_all_task<RESULT>, awaitable = std::move(a));
			MC_YIELD_AWAIT(static_cast<AWAITABLE&&>(awaitable));
			MC_END();
		}

		template<
			typename AWAITABLE,
			typename RESULT = typename awaitable_traits<AWAITABLE&&>::await_result_t,
			enable_if_t<std::is_void<RESULT>::value, int> = 0>
		when_all_task<void> make_when_all_task(AWAITABLE a)
		{
			MC_BEGIN(when_all_task<void>, awaitable = std::move(a));
			MC_AWAIT(static_cast<AWAITABLE&&>(awaitable));
			MC_END();
		}

		template<
			typename AWAITABLE,
			typename RESULT = typename awaitable_traits<AWAITABLE&>::await_result_t,
			enable_if_t<!std::is_void<RESULT>::value, int> = 0>
		when_all_task<RESULT> make_when_all_task(std::reference_wrapper<AWAITABLE> awaitable)
		{

			MC_BEGIN(when_all_task<RESULT>, awaitable);
			MC_YIELD_AWAIT(awaitable.get());
			MC_END();
			//co_yield co_await awaitable.get();
		}

		template<
			typename AWAITABLE,
			typename RESULT = typename awaitable_traits<AWAITABLE&>::await_result_t,
			enable_if_t<std::is_void<RESULT>::value, int> = 0>
		when_all_task<void> make_when_all_task(std::reference_wrapper<AWAITABLE> awaitable)
		{
			MC_BEGIN(when_all_task<void>, awaitable);
			MC_AWAIT(awaitable.get());
			MC_END();
			//co_await awaitable.get();
		}
#endif

	}
}

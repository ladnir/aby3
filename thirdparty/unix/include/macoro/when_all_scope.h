#pragma once
#include "macoro/async_scope.h"
#include "macoro/manual_reset_event.h"
#include "macoro/coroutine_handle.h"

namespace macoro
{
	template<typename awaitable>
	struct scoped_t;

	template<>
	struct scoped_t<void>
	{};

	template<typename awaitable>
	struct scoped_t
	{
		using awaitable_type = awaitable;
		using storage_type = remove_rvalue_reference_t<awaitable>;

		storage_type m_awaitable;

		template<typename awaitable2>
		scoped_t(awaitable2&& a)
			: m_awaitable(std::forward<awaitable2>(a))
		{}
	};

	inline scoped_t<void> scoped() { return {}; }

	// wraps an awaitable so that it is started as an eager
	// scoped_task.
	template<typename awaitable>
	auto scoped(awaitable&& a)
	{
		return scoped_t<awaitable>(std::forward<awaitable>(a));
	}

	template<typename awaitable>
	decltype(auto) operator|(awaitable&& a, scoped_t<void>)
	{
		return scoped(std::forward<awaitable>(a));
	}

	// when_all_scope allows the caller to await a scoped(awaitable).
	// when awaited, a scoped_task is eagerly started but has its lifetime 
	// tied to some scoped object when_all_scope. scoped_task can be 
	// fire and forget but will always be joined then the scope 
	// object is awaited. For example,
	// 
	// co_await [&]()->when_all_scope{
	//		scoped_task<int> t0 = co_await scoped(foo());
	//		scoped_task<int> t1 = co_await scoped(foo());
	//		...
	//		int i = co_await std::move(t0);
	//	
	//		// t1 has not been joined.
	// }();
	// 
	// // t1 has been joined
	// 
	//
	// If an unjoined scoped_task throws, or an exception is thrown
	// in the body, then they are caught, collected, and rethrow in
	// an async_scope_exception.
	struct when_all_scope 
	{
		struct promise_type
		{
			promise_type()
			{
				m_scope.m_barrier.increment();
			}

			struct final_awaitable
			{
				bool await_ready() noexcept { return false; }
				void await_suspend(std::coroutine_handle<promise_type> h) noexcept
				{
					h.promise().m_scope.m_barrier.decrement();
				}
				void await_resume() noexcept {}
			};

			suspend_always initial_suspend() noexcept { return {}; }
			final_awaitable final_suspend() noexcept { return {}; }

			when_all_scope get_return_object() noexcept { 
				return { coroutine_handle<promise_type>::from_promise(*this, macoro::coroutine_handle_type::std) };
			}

			void unhandled_exception() noexcept {

				std::lock_guard<std::mutex> lock(m_scope.m_exception_mutex);
				m_scope.m_exceptions.push_back(std::current_exception());
			}

			void return_void() noexcept {};

			template<typename A>
			decltype(auto) await_transform(A&& a) noexcept
			{
				return std::forward<A>(a);
			}

			template<typename A>
			decltype(auto) await_transform(scoped_t<A>&& a, std::source_location loc = std::source_location::current()) noexcept
			{
				using scoped_task_of_a = decltype(m_scope.add(std::forward<typename scoped_t<A>::awaitable_type>(a.m_awaitable), loc));
				struct awaiter
				{
					scoped_task_of_a m_scoped_task;
					bool await_ready() noexcept { return true; }
					void await_suspend(std::coroutine_handle<promise_type>) noexcept {}
					scoped_task_of_a await_resume()noexcept
					{
						return std::move(m_scoped_task);
					}
				};
				return awaiter{ m_scope.add(std::forward<typename scoped_t<A>::awaitable_type>(a.m_awaitable), loc) };
			}

			traceable* get_traceable()
			{
				return &m_scope;
			}

			async_scope m_scope;
		};

		when_all_scope(coroutine_handle<promise_type> h)
			: m_handle(h)
		{}

		struct awaiter
		{
			coroutine_handle<promise_type> m_handle;
			async_scope::join_awaiter m_join;

			bool await_ready() { return m_join.await_ready(); }

			template<typename promise>
			std::coroutine_handle<> await_suspend(
				std::coroutine_handle<promise> p, 
				std::source_location loc = std::source_location::current())
			{
				m_handle.promise().m_scope.set_parent(get_traceable(p.promise()), loc);
				m_handle.resume();
				return m_join.await_suspend(p);
			}

			void await_resume()
			{
				m_join.await_resume();
			}
		};

		auto MACORO_OPERATOR_COAWAIT() const& noexcept
		{
			return awaiter{ m_handle, m_handle.promise().m_scope.MACORO_OPERATOR_COAWAIT() };
		}


		coroutine_handle<promise_type> m_handle;
	};
}
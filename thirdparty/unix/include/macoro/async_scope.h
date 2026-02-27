#pragma once

#include "macoro/trace.h"
#include "macoro/coroutine_handle.h"
#include "macoro/barrier.h"
#include "macoro/awaiter.h"
#include <iostream>
#include "macoro/detail/scoped_task_promise.h"

namespace macoro
{
	// a scoped_task is eagerly started but has its lifetime 
	// tied to some scoped object, e.g. async_scope or 
	// when_all_scope. scoped_task can be fire and forget but
	// will always be joined then the scope object is awaited.
	template<typename T = void>
	struct scoped_task
	{
		friend struct when_all_scope;

		using promise_type = detail::scoped_task_promise<T>;

		scoped_task() = default;
		scoped_task(scoped_task&& other)
		{
			*this = std::move(other);
		}
		scoped_task& operator=(scoped_task&& other) noexcept
		{
			if (std::addressof(other) != this)
			{
				if (m_coroutine)
					this->~scoped_task();
				m_coroutine = std::exchange(other.m_coroutine, nullptr);
			}
			return *this;
		}

		scoped_task(coroutine_handle<promise_type>&& c)
			: m_coroutine(c)
		{}

		~scoped_task();

		auto MACORO_OPERATOR_COAWAIT() && noexcept
		{
			return detail::scoped_task_awaiter<T>{ std::exchange(m_coroutine, nullptr) };
		}

		bool is_ready() const
		{
			if (m_coroutine)
			{
				auto v = m_coroutine.promise().m_completion_state.load(std::memory_order_relaxed);
				return v & (unsigned char)promise_type::completion_state::has_completed;
			}
			return false;

		}
	private:

		coroutine_handle<promise_type> m_coroutine;
	};

	// an exception representing a one or more sub exceptions
	// throw from within async_scope::add(...) but not manually joined.
	// when async_scope is awaited, the unhandled exceptions from add(...)
	// will be  collected and throw using async_scope_exception.
	struct async_scope_exception : std::exception
	{
		friend struct async_scope;

		std::vector<std::exception_ptr> m_exceptions;

		mutable std::string m_what;
	private:
		async_scope_exception(std::vector<std::exception_ptr> ex)
			: m_exceptions(std::move(ex))
		{}

		char const* what() const noexcept override
		{
			try {
				if (m_exceptions.size())
				{
					if (m_what.size() == 0)
					{
						std::stringstream ss;
						ss << "async_scope_exception: ";
						for (auto i = 0ull; i < m_exceptions.size(); ++i)
						{
							ss << i << ": ";
							try {
								std::rethrow_exception(m_exceptions[i]);
							}
							catch (std::exception& e)
							{
								ss << e.what() << "\n";
							}
							catch (...)
							{
								ss << "unknown exception\n";
							}
						}

						m_what = ss.str();
					}
					return m_what.c_str();
				}
			}
			catch (...) {}

			return "";
		}

		// get the list of exceptions.
		auto& exceptions() { return m_exceptions; }

	};

	// a scope object that the caller can add awaitables to 
	// via add(...). When the scope object is awaited, e.g.
	// 
	//	co_await scope;
	//
	// then all tasks will be joined. individual tasks can be
	// joined by awaiting the scoped_task that is returned by 
	// add(...). e.g.
	//
	//	scoped_task<int> task = scope.add(...);
	//  ...
	//	int i = co_await std::move(task);
	//
	// exceptions will be propegated correct.
	// If some tasks are not explicitly awaited and they throw
	// an exception, then awaiting the scope will throw 
	// async_scope_exception that will contain a vector with 
	// all exceptions that were thrown.
	struct async_scope : public traceable
	{
		void add_child(traceable* child) final override
		{
			assert(0);
		}

		void remove_child(traceable* child) final override
		{
			assert(0);
		}


		template<typename T>
		friend struct scoped_task;

		template<typename T>
		friend class detail::scoped_task_promise;

		friend struct when_all_scope;

		// add an awaitable to the scope. The awaitable
		// will be executed immediately. A scoped_task
		// is returned. The caller may await the scoped_task
		// or may let it get destroyed. The underlying 
		// awaitable will continued to be executed. All 
		// awaitables can be joined by calling join().
		// exceptions 
		template<typename Awaitable>
		scoped_task<macoro::remove_rvalue_reference_t<macoro::awaitable_result_t<Awaitable>>>
			add(Awaitable a, std::source_location loc = std::source_location::current()) noexcept
		{
			co_return co_await std::move(a);
		}


		// add an awaitable to the scope. The awaitable
		// will be executed immediately. A scoped_task
		// is returned. The caller may await the scoped_task
		// or may let it get destroyed. The underlying 
		// awaitable will continued to be executed. All 
		// awaitables can be joined by calling join().
		// exceptions 
		template<typename Awaitable>
		scoped_task<macoro::remove_rvalue_reference_t<macoro::awaitable_result_t<Awaitable>>>
			add(Awaitable& a, std::source_location loc = std::source_location::current()) noexcept
		{
			co_return co_await a;
		}

		struct join_awaiter
		{
			barrier_awaiter m_barrier;
			std::vector<std::exception_ptr> m_exceptions;

			bool await_ready() const noexcept { return m_barrier.await_ready(); }

			template<typename Promise>
			coroutine_handle<> await_suspend(
				coroutine_handle<Promise> awaiting_coroutine,
				std::source_location loc = std::source_location::current()) noexcept
			{
				return m_barrier.await_suspend(awaiting_coroutine, loc);
			}

			template<typename Promise>
			std::coroutine_handle<> await_suspend(
				std::coroutine_handle<Promise> awaiting_coroutine,
				std::source_location loc = std::source_location::current()) noexcept
			{
				return m_barrier.await_suspend(awaiting_coroutine, loc);
			}

			void await_resume()
			{
				m_barrier.await_resume();
				if (m_exceptions.size())
					throw async_scope_exception(std::move(m_exceptions));
			}
		};

		// returns an awaiter that can be co_awaited to suspend 
		// until all added awaitables have been completed. Only
		// one 
		join_awaiter join() noexcept
		{
			return MACORO_OPERATOR_COAWAIT();
		}

		// returns an awaiter that can be co_awaited to suspend 
		// until all added awaitables have been completed.
		join_awaiter MACORO_OPERATOR_COAWAIT() & noexcept
		{
			return { m_barrier.MACORO_OPERATOR_COAWAIT(), std::move(m_exceptions) };
		}

	private:
		// used to count how many active tasks exists. barrier
		// then then be awaited to
		traceable* m_parent = nullptr;
		barrier m_barrier;
		std::mutex m_exception_mutex;
		std::vector<std::exception_ptr> m_exceptions;
	};


	namespace detail
	{


		template<typename T>
		template<typename Awaiter>
		scoped_task_promise<T>::scoped_task_promise(async_scope& scope, Awaiter&&, std::source_location& loc) noexcept
			: m_completion_state((unsigned char)completion_state::inprogress)
			, m_scope(&scope)
		{
			set_parent(&scope, loc);
			scope.m_barrier.increment();
		}



		template<typename T>
		bool scoped_task_promise<T>::final_awaitable::await_ready() noexcept
		{
			scoped_task_promise& promise = m_promise;
			async_scope* scope = promise.m_scope;
			bool done = false;

			// race to see if we need to call the completion handle
			// it might not have been set yet (or ever).
			// release our result. acquire their continuation (if they beat us).
			completion_state b = promise.completion_state_fetch_or(
				completion_state::has_completed,
				std::memory_order_acq_rel);

			// if b, then we have finished after the continuation was set. 
			if (b == completion_state::has_continuation_or_dropped)
			{
				// if we dont have a continuation, that means the 
				// scoped_task has been dropped. Therefore, no one
				// wants the result and we can destroy the coro.
				//
				// if we do have a continuation, that means we have 
				// a scoped_task::awaiter somewhere and they will
				// call destroy for us, once the continuation consumes
				// the return value. 
				if (promise.m_continuation)
				{
					m_run_continutation = true;
					done = false;
				}
				else
				{
					if (promise.m_result_type == result_type::exception)
					{
						// todo, replace vector by a lock free linked list, either to the full
						// coro frame (no allocations) or to the execptions (node allocations).
						// for now, locking on expcetions should be fine.
						std::lock_guard<std::mutex> lock(promise.m_scope->m_exception_mutex);
						promise.m_scope->m_exceptions.push_back(std::move(promise.m_exception));
					}

					// destroy the coro.
					// this is the same as coro.destroy()
					// but there is a msvc bug if you do that.
					done = true;
				}
			}
			else
			{
				// the handle is still alive and the user might set 
				// a continuation. If they set a continuation, the 
				// awaiter will destroy the coro once the continuation
				// constumes the result.
				// 
				// If a continuation is never set, then the scoped_task
				// will destroy the coro in its distructor. 
				assert(b == completion_state::inprogress);
				done = false;
			}

			// notify the scope that we are done.
			scope->m_barrier.decrement();
			return done;
		}
	}


	template<typename T>
	scoped_task<T>::~scoped_task()
	{
		if (m_coroutine)
		{
			auto& promise = m_coroutine.promise();
			// if we are here, then the coro has been started but 
			// we have not set a continuation. As such, we need to
			// query the promise to see if the coro has completed.
			// If so, we need to destroy it. Otherwise, we let it 
			// know that there wont be a continuation and the coro
			// will destroy itself onces it completes.
			auto b =
				promise.try_set_continuation(coroutine_handle<>{});

			if (b == promise_type::completion_state::has_completed)
			{
				if (promise.m_result_type == promise_type::result_type::exception)
				{
					// todo, replace vector by a lock free linked list, either to the full
					// coro frame (no allocations) or to the execptions (node allocations).
					// for now, locking on expcetions should be fine.
					std::lock_guard<std::mutex> lock(promise.m_scope->m_exception_mutex);
					promise.m_scope->m_exceptions.push_back(std::move(promise.m_exception));
				}

				m_coroutine.destroy();
			}
		}
	}

}
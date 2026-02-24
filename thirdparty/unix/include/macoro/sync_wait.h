#pragma once

#include "macoro/type_traits.h"
#include "macoro/coro_frame.h"
#include "macoro/optional.h"
#include "macoro/coroutine_handle.h"
#include <future>
#include "macros.h"
#include <mutex>

namespace macoro
{

#ifdef MACORO_CPP_20
#define MACORO_MAKE_BLOCKING_20 1
#endif

	template<typename T>
	struct blocking_task;

	namespace detail
	{


		template<typename Awaitable>
		struct blocking_promise : basic_traceable
		{

			// can be used to check if it allocates
			//void* operator new(std::size_t n) noexcept
			//{
			//	return std::malloc(n);
			//}
			//void operator delete(void* ptr, std::size_t sz)
			//{
			//	std::free(ptr);
			//}

			// construct the awaiter in the promise
			template<typename A>
			blocking_promise(A&& a, std::source_location& loc)
				: m_awaiter(get_awaiter(std::forward<Awaitable>(a)))
			{
				//std::cout << "blocking_promise" << std::endl;
				set_parent(nullptr, loc);
			}

			//~blocking_promise()
			//{
			//	std::cout << "~blocking_promise " << (size_t)this << std::endl;
			//}

			using inner_awaiter = decltype(get_awaiter(std::declval<Awaitable&&>()));

			// the promise will hold the awaiter. this way when
			// we return the value, you can directly get it
			// by calling m_awaiter.await_resume()
			inner_awaiter m_awaiter;

			// any active exceptions that is thrown.
			std::exception_ptr m_exception;

			// mutex and cv to block until the caller until the result is ready.
			std::mutex m_mutex;
			std::condition_variable m_cv;
			bool m_is_set = false;

			// block the caller.
			void wait()
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				m_cv.wait(lock, [this] { return m_is_set; });
			}

			// notify the caller.
			void set()
			{
				assert(m_is_set == false);
				std::lock_guard<std::mutex> lock(this->m_mutex);
				this->m_is_set = true;
				this->m_cv.notify_all();
			}

			void return_void() {}

			struct final_awaiter
			{
				bool await_ready() noexcept { return false; };

				template<typename C>
				void await_suspend(C c) noexcept {
					c.promise().set(); }

				void await_resume() noexcept {}
			};

			suspend_never initial_suspend() noexcept { return{}; }
			final_awaiter final_suspend() noexcept {
				return {};
			}

			void unhandled_exception() noexcept
			{
				m_exception = std::current_exception();
			}

			blocking_task<Awaitable> get_return_object() noexcept;
			blocking_task<Awaitable> macoro_get_return_object() noexcept;

			// intercept the dummy co_await and manually co_await m_awaiter.
			auto& await_transform(Awaitable&& a)
			{
				return *this;
			}

			// forward to m_awaiter
			bool await_ready() //noexcept(std::declval<inner_awaiter>().await_ready())
			{
				return m_awaiter.await_ready();
			}

			// forward to m_awaiter
			auto await_suspend(std::coroutine_handle<blocking_promise> c) //noexcept(macoro::await_suspend(m_awaiter, c))
			{
				return m_awaiter.await_suspend(c);
			}

			// onces m_awaiter completes, notify the caller that they 
			// can get the result. this will happen by calling m_awaiter.await_resume();
			void await_resume() { }

			// get the value out of m_awaiter.
			decltype(auto) value()
			{
				if (this->m_exception)
					std::rethrow_exception(this->m_exception);
				assert(m_is_set);

				return m_awaiter.await_resume();
			}
		};
	}

	template<typename awaitable>
	struct blocking_task
	{
		using promise_type = detail::blocking_promise<awaitable>;
		coroutine_handle<promise_type> m_handle;
		blocking_task(coroutine_handle<promise_type> h)
			: m_handle(h)
		{}

		blocking_task() = default;
		blocking_task(blocking_task&& other)
			: m_handle(std::exchange(other.m_handle, nullptr))
		{}
		blocking_task& operator=(blocking_task&& h) {
			this->~blocking_task();
			m_handle = std::exchange(h.m_handle, std::nullptr_t{});
			return *this;
		}

		~blocking_task()
		{
			if (m_handle)
				m_handle.destroy();
		}

		decltype(auto) get()
		{
			assert(m_handle);
			m_handle.promise().wait();
			return m_handle.promise().value();
		}
	};

	template<typename Awaitable>
	blocking_task<Awaitable> detail::blocking_promise<Awaitable>::get_return_object() noexcept { return { coroutine_handle<blocking_promise>::from_promise(*this, coroutine_handle_type::std) }; }
	template<typename Awaitable>
	blocking_task<Awaitable> detail::blocking_promise<Awaitable>::macoro_get_return_object() noexcept { return { coroutine_handle<blocking_promise>::from_promise(*this, coroutine_handle_type::macoro) }; }

	template<typename Awaitable>
	auto make_blocking(Awaitable awaitable, std::source_location loc = std::source_location::current())
		-> blocking_task<Awaitable>
	{
#if MACORO_MAKE_BLOCKING_20
		co_await static_cast<Awaitable&&>(awaitable);
#else
		MC_BEGIN(blocking_task<ResultType>, &awaitable);
		MC_AWAIT(static_cast<Awaitable&&>(awaitable));
		MC_END();
#endif
	}


	struct make_blocking_t { };
	inline make_blocking_t make_blocking() { return {}; }

	template<typename awaitable>
	decltype(auto) operator|(awaitable&& a, make_blocking_t)
	{
		return make_blocking(std::forward<awaitable>(a));
	}

	template<typename Awaitable>
	decltype(auto) sync_wait(Awaitable&& awaitable, std::source_location loc = std::source_location::current())
	{
		if constexpr (std::is_reference_v<Awaitable>)
		{
			return make_blocking<std::remove_reference_t<Awaitable>&>(
				std::forward<Awaitable>(awaitable), loc).get();
		}
		else
		{
			return make_blocking<std::remove_reference_t<Awaitable>&&>(
				std::forward<Awaitable>(awaitable), loc).get();
		}
	}

	struct sync_wait_t { };
	inline sync_wait_t sync_wait() { return {}; }


	template<typename awaitable>
	decltype(auto) operator|(awaitable&& a, sync_wait_t)
	{
		return sync_wait(std::forward<awaitable>(a));
	}
}
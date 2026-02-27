#pragma once

#include "macoro/trace.h"
#include "macoro/coroutine_handle.h"
#include "macoro/barrier.h"
#include "macoro/awaiter.h"
#include <iostream>

namespace macoro
{
	struct async_scope;
	struct when_all_scope;

	template<typename T>
	struct scoped_task;
	namespace detail
	{
		// A CRTP class that customizes the value type
		// based on if its a ref, value or void.
		template<typename Self, typename T>
		class scoped_task_promise_storage;


		// A CRTP class that customizes the value type
		// based on if its a ref, value or void. 
		template<typename Self, typename T>
		class scoped_task_promise_storage<Self, T&>
		{
		public:
			Self& self() { return *(Self*)this; }

			using value_type = T&;

			template<
				typename VALUE,
				typename = enable_if_t<std::is_convertible<VALUE&&, value_type>::value>
			>
			void return_value(VALUE&& value)
				noexcept(std::is_nothrow_constructible<value_type, VALUE&&>::value)
			{
				m_value = &value;
				self().m_result_type = Self::result_type::value;
			}

			// the actual storage, only one is ever active.
			union
			{
				T* m_value;
				std::exception_ptr m_exception;
			};

			scoped_task_promise_storage() {}

			~scoped_task_promise_storage()
			{
				if (self().m_result_type == Self::result_type::exception)
					m_exception.~exception_ptr();
			}

			T& result()
			{
				if (self().m_result_type == Self::result_type::exception)
					std::rethrow_exception(self().m_exception);
				assert(self().m_result_type == Self::result_type::value);
				return *self().m_value;
			}
		};


		// A CRTP class that customizes the value type
		// based on if its a ref, value or void.
		template<typename Self, typename T>
		class scoped_task_promise_storage<Self, T&&>
		{
		public:
			Self& self() { return *(Self*)this; }

			using value_type = T&&;

			template<
				typename VALUE,
				typename = enable_if_t<std::is_convertible<VALUE&&, value_type>::value>
			>
			void return_value(VALUE&& value)
				noexcept(std::is_nothrow_constructible<value_type, VALUE&&>::value)
			{
				m_value = &value;
				self().m_result_type = Self::result_type::value;
			}

			// the actual storage, only one is ever active.
			union
			{
				T* m_value;
				std::exception_ptr m_exception;
			};

			scoped_task_promise_storage() {}

			~scoped_task_promise_storage()
			{
				if (self().m_result_type == Self::result_type::exception)
					m_exception.~exception_ptr();
			}

			T&& result()&
			{
				if (self().m_result_type == Self::result_type::exception)
					std::rethrow_exception(self().m_exception);
				assert(self().m_result_type == Self::result_type::value);
				return std::move(*self().m_value);
			}
		};


		// A CRTP class that customizes the value type
		// based on if its a ref, value or void.
		template<typename Self>
		class scoped_task_promise_storage<Self, void>
		{
		public:
			Self& self() { return *(Self*)this; }

			void return_void() noexcept
			{
				self().m_result_type = Self::result_type::value;
			}

			scoped_task_promise_storage() {}
			~scoped_task_promise_storage()
			{
				if (self().m_result_type == Self::result_type::exception)
					m_exception.~exception_ptr();
			}

			// the actual storage, only one is ever active.
			union {
				std::exception_ptr m_exception;
			};

			void result()
			{
				if (self().m_result_type == Self::result_type::exception)
					std::rethrow_exception(self().m_exception);
				assert(self().m_result_type == Self::result_type::value);
			}
		};


		// A CRTP class that customizes the value type
		// based on if its a ref, value or void.
		template<typename Self, typename T>
		class scoped_task_promise_storage
		{
		public:
			Self& self() { return *(Self*)this; }

			using value_type = T;

			template<
				typename VALUE,
				typename = enable_if_t<std::is_convertible<VALUE&&, value_type>::value>
			>
			void return_value(VALUE&& value)
				noexcept(std::is_nothrow_constructible<value_type, VALUE&&>::value)
			{
				::new (static_cast<void*>(std::addressof(m_value))) value_type(std::forward<VALUE>(value));
				self().m_result_type = Self::result_type::value;
			}

			// the actual storage, only one is ever active.
			union
			{
				value_type m_value;
				std::exception_ptr m_exception;
			};

			scoped_task_promise_storage() {}

			~scoped_task_promise_storage()
			{
				switch (self().m_result_type)
				{
				case Self::result_type::value:
					if constexpr (std::is_destructible_v<T>)
						m_value.~T();
					break;
				case Self::result_type::exception:
					m_exception.~exception_ptr();
					break;
				default:
					break;
				}
			}

			T& result()&
			{
				if (self().m_result_type == Self::result_type::exception)
					std::rethrow_exception(self().m_exception);
				assert(self().m_result_type == Self::result_type::value);
				return self().m_value;
			}

			T result()&&
			{
				if (self().m_result_type == Self::result_type::exception)
					std::rethrow_exception(self().m_exception);
				assert(self().m_result_type == Self::result_type::value);
				return std::move(self().m_value);
			}
		};

		template<typename T>
		class scoped_task_promise final :
			public scoped_task_promise_storage<scoped_task_promise<T>, T>,
			public basic_traceable
		{
		public:
			//// can be used to check if it allocates
			//void* operator new(std::size_t n) noexcept
			//{
			//	std::cout << "scoped_task_promise alloc" << std::endl;
			//	return std::malloc(n);
			//}
			//void operator delete(void* ptr, std::size_t sz)
			//{
			//	std::free(ptr);
			//}


			using value_type = T;

			friend struct final_awaitable;

			enum class completion_state : unsigned char {
				// has not completed and does not have a continuation
				inprogress = 0,

				// has not completed and does     have a continuation
				has_continuation_or_dropped = 1,

				// has     completed and does not have a continuation
				has_completed = 2
			};

			// has the task completed or the continueation been set.
			std::atomic<unsigned char> m_completion_state;

			completion_state completion_state_fetch_or(completion_state state, std::memory_order order)
			{
				return (completion_state)m_completion_state.fetch_or((unsigned char)state, order);
			}

			// the continuation if present.
			coroutine_handle<> m_continuation;

			// the containing scope. 
			async_scope* m_scope = nullptr;

			enum class result_type { empty, value, exception };

			// the status of the value/exception storage.
			result_type m_result_type = result_type::empty;

			// this final awaiter runs the suspend logic in await_ready
			// due to a bug un msvc. Using symetric transfer and called
			// destroy when done = true causes a segfault. This alternative
			// implementation implicitly destroys the coro by returning
			// true from await_ready.
			struct final_awaitable
			{
				scoped_task_promise& m_promise;

				// a variable used to remember if we need to call the continuation.
				bool m_run_continutation = false;

				bool await_ready() noexcept;

#ifdef MACORO_CPP_20
				std::coroutine_handle<> await_suspend(
					std::coroutine_handle<scoped_task_promise> coro) noexcept
				{
					if (m_run_continutation)
						return m_promise.m_continuation.std_cast();
					else
						return std::noop_coroutine();
				}
#endif

				coroutine_handle<> await_suspend(
					coroutine_handle<scoped_task_promise> coro) noexcept
				{
					if (m_run_continutation)
						return m_promise.m_continuation;
					else
						return noop_coroutine();
				}

				void await_resume() noexcept {
				}
			};



		public:

			template<typename Awaiter>
			scoped_task_promise(async_scope& scope, Awaiter&&, std::source_location&) noexcept;

			~scoped_task_promise() {
			}

			suspend_never initial_suspend() noexcept { return {}; }
			final_awaitable final_suspend() noexcept { return { *this }; }

#ifdef MACORO_CPP_20
			completion_state try_set_continuation(std::coroutine_handle<> continuation) noexcept
			{
				return try_set_continuation(coroutine_handle<>(continuation));
			}
#endif
			completion_state try_set_continuation(coroutine_handle<> continuation) noexcept
			{
				m_continuation = continuation;
				return completion_state_fetch_or(
					completion_state::has_continuation_or_dropped,
					std::memory_order_acq_rel);
			}

			scoped_task<T> get_return_object() noexcept {
				return
					scoped_task<T>{
					coroutine_handle<scoped_task_promise>::from_promise(*this, coroutine_handle_type::std)
				};
			}
			scoped_task<T> macoro_get_return_object() noexcept;

			void unhandled_exception() noexcept
			{
				::new (static_cast<void*>(std::addressof(this->m_exception))) std::exception_ptr(
					std::current_exception());
				m_result_type = result_type::exception;
			}
		};


		template<typename T>
		struct scoped_task_awaiter
		{
			using promise_type = scoped_task_promise<T>;

			coroutine_handle<promise_type> m_coroutine;

			bool await_ready() const noexcept { return false; }

			scoped_task_awaiter(coroutine_handle<promise_type> c)
				:m_coroutine(c)
			{}

			~scoped_task_awaiter()
			{
				assert(m_coroutine);
				m_coroutine.destroy();
			}

#ifdef MACORO_CPP_20
			template<typename Promise>
			std::coroutine_handle<> await_suspend(
				std::coroutine_handle<Promise> awaitingCoroutine) noexcept
			{
				return await_suspend(
					coroutine_handle<>(awaitingCoroutine)).std_cast();
			}
#endif

			template<typename Promise>
			coroutine_handle<> await_suspend(
				coroutine_handle<Promise> awaiting_coroutine) noexcept
			{
				// release our continuation. acquire their result (if they beat us).
				auto b = m_coroutine.promise().try_set_continuation(awaiting_coroutine);

				// if b, then the result is ready and we should just resume the
				// awaiting coroutine. 
				if (b == promise_type::completion_state::has_completed)
				{
					// the coro has previously completed, we can just resume the 
					// continue the awaiting coro. When this awaiter is destroyed,
					// the awaited coro will be destroyed.
					return awaiting_coroutine;
				}
				else
				{
					// otherwise we have finished first and our continuation will be resumed
					// when the coroutine finishes.
					return noop_coroutine();
				}
			}

			decltype(auto) await_resume()
			{
				if (!this->m_coroutine)
				{
					throw broken_promise{};
				}

				promise_type& p = this->m_coroutine.promise();

				return static_cast<promise_type&&>(p).result();
			}
		};


	}
}
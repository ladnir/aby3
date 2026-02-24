#pragma once
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See github.com/lewissbaker/cppcoro LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include "macoro/type_traits.h"
#include <tuple>
#include <atomic>
#include "macoro/coroutine_handle.h"
#include "when_all_counter.h"
#include "macoro/trace.h"

#ifdef MACORO_CPP_20
#include <coroutine>
#endif

namespace macoro
{
	namespace detail
	{


		class when_all_ready_awaitable_base : public traceable
		{
		public:

			when_all_ready_awaitable_base(std::size_t count) noexcept
				: m_count(count + 1)
				, m_awaitingCoroutine(nullptr)
			{}

			bool is_ready() const noexcept
			{
				// We consider this complete if we're asking whether it's ready
				// after a coroutine has already been registered.
				return static_cast<bool>(m_awaitingCoroutine);
			}

			bool try_await(coroutine_handle<> awaitingCoroutine) noexcept
			{
				m_awaitingCoroutine = awaitingCoroutine;
				return m_count.fetch_sub(1, std::memory_order_acq_rel) > 1;
			}

			void notify_awaitable_completed() noexcept
			{
				if (m_count.fetch_sub(1, std::memory_order_acq_rel) == 1)
				{
					m_awaitingCoroutine.resume();
				}
			}

		protected:

			std::atomic<std::size_t> m_count;
			coroutine_handle<> m_awaitingCoroutine;

		};

		template <typename> struct is_tuple : std::false_type {};

		template <typename ...T> struct is_tuple<std::tuple<T...>> : std::true_type {};

		template<typename TASK_CONTAINER>
		class when_all_ready_awaitable;

		template<>
		class when_all_ready_awaitable<std::tuple<>>
		{
		public:

			constexpr when_all_ready_awaitable() noexcept {}
			explicit constexpr when_all_ready_awaitable(std::tuple<>) noexcept {}

			constexpr bool await_ready() const noexcept { return true; }
			void await_suspend(coroutine_handle<>) noexcept {}
#ifdef MACORO_CPP_20
			void await_suspend(std::coroutine_handle<>) noexcept {}
#endif
			std::tuple<> await_resume() const noexcept { return {}; }

		};

		// the when_all_read awaitable that holds all the tasks.
		// TASK_CONTAINER should be either an std::tuple or vector like 
		// container that contains when_all_task<T>.
		template<typename TASK_CONTAINER>
		class when_all_ready_awaitable final : public when_all_ready_awaitable_base
		{
		public:

			size_t tasks_size(TASK_CONTAINER& tasks)
			{
				if constexpr (is_tuple<TASK_CONTAINER>::value)
					return std::tuple_size<TASK_CONTAINER>::value;
				else
					return tasks.size();
			}


			explicit when_all_ready_awaitable(TASK_CONTAINER&& tasks) noexcept
				: when_all_ready_awaitable_base(tasks_size(tasks))
				, m_tasks(std::forward<TASK_CONTAINER>(tasks))
			{}

			when_all_ready_awaitable(when_all_ready_awaitable&& other)
				noexcept(std::is_nothrow_move_constructible<TASK_CONTAINER>::value)
				: when_all_ready_awaitable_base(tasks_size(other.m_tasks))
				, m_tasks(std::move(other.m_tasks))
			{}

			when_all_ready_awaitable(const when_all_ready_awaitable&) = delete;
			when_all_ready_awaitable& operator=(const when_all_ready_awaitable&) = delete;

			class lval_awaiter
			{
			public:

				lval_awaiter(when_all_ready_awaitable& awaitable)
					: m_awaitable(awaitable)
				{}

				bool await_ready() const noexcept
				{
					return m_awaitable.is_ready();
				}

#ifdef MACORO_CPP_20
				template<typename Promise>
				bool await_suspend(std::coroutine_handle<Promise> awaitingCoroutine, std::source_location loc = std::source_location::current()) noexcept
				{
					m_awaitable.set_parent(get_traceable(awaitingCoroutine), loc);
					return await_suspend(coroutine_handle<>(awaitingCoroutine), loc);
				}
#endif
				template<typename Promise>
				bool await_suspend(coroutine_handle<Promise> awaitingCoroutine, std::source_location loc = std::source_location::current()) noexcept
				{
					m_awaitable.set_parent(get_traceable(awaitingCoroutine), loc);
					return m_awaitable.try_await(awaitingCoroutine, loc);
				}

				TASK_CONTAINER& await_resume() noexcept
				{
					return m_awaitable.m_tasks;
				}

			private:

				when_all_ready_awaitable& m_awaitable;

			};

			auto MACORO_OPERATOR_COAWAIT() & noexcept
			{
				return lval_awaiter{ *this };
			}


			class rval_awaiter
			{
			public:

				rval_awaiter(when_all_ready_awaitable& awaitable)
					: m_awaitable(awaitable)
				{}

				bool await_ready() const noexcept
				{
					return m_awaitable.is_ready();
				}

#ifdef MACORO_CPP_20
				template<typename Promise>
				bool await_suspend(std::coroutine_handle<Promise> awaitingCoroutine, std::source_location loc = std::source_location::current()) noexcept
				{
					m_awaitable.set_parent(get_traceable(awaitingCoroutine), loc);
					return m_awaitable.try_await(coroutine_handle<>(awaitingCoroutine));

					//return await_suspend(coroutine_handle<>(awaitingCoroutine), loc);
				}
#endif
				template<typename Promise>
				bool await_suspend(coroutine_handle<Promise> awaitingCoroutine, std::source_location loc = std::source_location::current()) noexcept
				{
					m_awaitable.set_parent(get_traceable(awaitingCoroutine), loc);
					return m_awaitable.try_await(awaitingCoroutine);
				}

				TASK_CONTAINER&& await_resume() noexcept
				{
					return std::move(m_awaitable.m_tasks);
				}

			private:

				when_all_ready_awaitable& m_awaitable;

			};

			auto MACORO_OPERATOR_COAWAIT() && noexcept
			{
				return rval_awaiter{ *this };
			}



			void add_child(traceable* child) final override
			{
				assert(0);
			}

			void remove_child(traceable* child) final override
			{
				assert(0);
			}

		//private:

			//bool is_ready() const noexcept
			//{
			//	return m_counter.is_ready();
			//}

			template<std::size_t... INDICES>
			void start_tasks(std::integer_sequence<std::size_t, INDICES...>) noexcept
			{
				if constexpr (is_tuple<TASK_CONTAINER>::value)
				{
					(void)std::initializer_list<int>{
						(std::get<INDICES>(m_tasks).start(*this), 0)...
					};
				}
			}

			bool try_await(coroutine_handle<> awaitingCoroutine) noexcept
			{
				if constexpr (is_tuple<TASK_CONTAINER>::value)
				{
					start_tasks(
						std::make_integer_sequence<std::size_t, std::tuple_size<TASK_CONTAINER>::value>{});
				}
				else
				{
					for (auto&& task : m_tasks)
					{
						task.start(*this);
					}
				}

				return when_all_ready_awaitable_base::try_await(awaitingCoroutine);
			}

			TASK_CONTAINER m_tasks;

		};
	}
}


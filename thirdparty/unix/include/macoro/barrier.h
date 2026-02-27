#pragma once

#include "macoro/config.h"
#include "macoro/coroutine_handle.h"
#include "macoro/trace.h"

#include <atomic>
#include <source_location>

namespace macoro
{
	class barrier;

	class barrier_awaiter : basic_traceable
	{
		barrier& m_barrier;

	public:

		barrier_awaiter(barrier& barrier)
			: m_barrier(barrier)
		{}

		bool await_ready() const noexcept { return false; }

		template <typename Promise>
		coroutine_handle<> await_suspend(
			coroutine_handle<Promise> continuation,
			std::source_location loc = std::source_location::current()) noexcept;

		template <typename Promise>
		std::coroutine_handle<> await_suspend(
			std::coroutine_handle<Promise> continuation,
			std::source_location loc = std::source_location::current()) noexcept
		{
			return await_suspend(coroutine_handle<Promise>(continuation), loc).std_cast();
		}

		void await_resume() const noexcept {}
	};
	
	// a barrier is used to release a waiting coroutine
	// once a count reaches 0. The count can be initialy
	// set and the incremented and decremented. Once it 
	// hits zero, an await coroutine is resumed.
	// 
	// for exmaple:
	// 
	// barrier b(1); // count = 1
	// b.add(2);     // count = 3
	// b.increment();// count = 4
	//                             co_await b; // suspend
	// b.decrement();// count = 3
	// b.decrement();// count = 2
	// b.decrement();// count = 1
	// b.decrement();// count = 0
	//                             resumed...
	//
	// The barrior can be reused once it reaches zero.
	//
	class barrier
	{
		friend barrier_awaiter;

		// the coro that is awaiting the barrier
		coroutine_handle<> m_continuation;

		// the current count, once it decrements to 0, 
		// m_continuation is called if set.
		std::atomic<std::size_t> m_count;
	public:

		// construct a new barrier with the given initial value.
		explicit barrier(std::size_t initial_count = 0) noexcept
			: m_count(initial_count)
		{}

		// increament the count by amount and return the new count.
		std::size_t add(size_t amount) noexcept
		{
			auto old = m_count.fetch_add(amount, std::memory_order_relaxed);
			auto ret = old + amount;
			assert(ret >= old);
			return ret;
		}

		// increment the count by 1
		std::size_t increment() noexcept
		{
			return add(1);
		}

		// returns the current count.
		std::size_t count() const noexcept 
		{
			return m_count.load(std::memory_order_acquire);
		}

		// decrease the count by 1. Returns the new count.
		std::size_t decrement() noexcept
		{
			const std::size_t old_count = m_count.fetch_sub(1, std::memory_order_acq_rel);

			assert(old_count);

			if (old_count == 1 && m_continuation)
			{
				std::exchange(m_continuation, nullptr).resume();
			}
			return old_count - 1;
		}
		
		// suspend until the count reached zero. Only
		// one caller can await the barrier at a time.
		// if needed, the impl could be extended to support
		// more callers, see async_manual_reset_event.
		auto MACORO_OPERATOR_COAWAIT() noexcept
		{
			return barrier_awaiter{ *this };
		}
	};

	template <typename Promise>
	coroutine_handle<> barrier_awaiter::await_suspend(
		coroutine_handle<Promise> continuation,
		std::source_location loc) noexcept
	{
		coroutine_handle<> ret = continuation;
		if (m_barrier.increment() > 1)
		{
			set_parent(get_traceable(continuation), loc);
			assert(m_barrier.m_continuation == nullptr);
			m_barrier.m_continuation = std::exchange(ret, noop_coroutine());
		}
		m_barrier.decrement();

		return ret;
	}

}
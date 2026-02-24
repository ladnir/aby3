///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_ASYNC_MANUAL_RESET_EVENT_HPP_INCLUDED
#define CPPCORO_ASYNC_MANUAL_RESET_EVENT_HPP_INCLUDED

#include "macoro/coroutine_handle.h"
#include <atomic>
#include <cstdint>

namespace macoro
{
	class async_manual_reset_event_operation;

	/// An async manual-reset event is a coroutine synchronisation abstraction
	/// that allows one or more coroutines to wait until some thread calls
	/// set() on the event.
	///
	/// When a coroutine awaits a 'set' event the coroutine continues without
	/// suspending. Otherwise, if it awaits a 'not set' event the coroutine is
	/// suspended and is later resumed inside the call to 'set()'.
	///
	/// \seealso async_auto_reset_event
	class async_manual_reset_event
	{
	public:

		/// Initialise the event to either 'set' or 'not set' state.
		///
		/// \param initiallySet
		/// If 'true' then initialises the event to the 'set' state, otherwise
		/// initialises the event to the 'not set' state.
		async_manual_reset_event(bool initiallySet = false) noexcept;

		~async_manual_reset_event();

		/// Wait for the event to enter the 'set' state.
		///
		/// If the event is already 'set' then the coroutine continues without
		/// suspending.
		///
		/// Otherwise, the coroutine is suspended and later resumed when some
		/// thread calls 'set()'. The coroutine will be resumed inside the next
		/// call to 'set()'.
		async_manual_reset_event_operation MACORO_OPERATOR_COAWAIT() const noexcept;

		/// Query if the event is currently in the 'set' state.
		bool is_set() const noexcept;

		/// Set the state of the event to 'set'.
		///
		/// If there are pending coroutines awaiting the event then all
		/// pending coroutines are resumed within this call.
		/// Any coroutines that subsequently await the event will continue
		/// without suspending.
		///
		/// This operation is a no-op if the event was already 'set'.
		void set() noexcept;

		/// Set the state of the event to 'not-set'.
		///
		/// Any coroutines that subsequently await the event will suspend
		/// until some thread calls 'set()'.
		///
		/// This is a no-op if the state was already 'not set'.
		void reset() noexcept;

	private:

		friend class async_manual_reset_event_operation;

		// This variable has 3 states:
		// - this    - The state is 'set'.
		// - nullptr - The state is 'not set' with no waiters.
		// - other   - The state is 'not set'.
		//             Points to an 'async_manual_reset_event_operation' that is
		//             the head of a linked-list of waiters.
		mutable std::atomic<void*> m_state;

	};

	class async_manual_reset_event_operation
	{
	public:

		explicit async_manual_reset_event_operation(const async_manual_reset_event& event) noexcept;

		bool await_ready() const noexcept;
		bool await_suspend(coroutine_handle<> awaiter) noexcept;
		bool await_suspend(std::coroutine_handle<> awaiter) noexcept;
		void await_resume() const noexcept {}

	private:

		friend class async_manual_reset_event;

		const async_manual_reset_event& m_event;
		async_manual_reset_event_operation* m_next;
		coroutine_handle<> m_awaiter;

	};



	inline async_manual_reset_event::async_manual_reset_event(bool initiallySet) noexcept
		: m_state(initiallySet ? static_cast<void*>(this) : nullptr)
	{}

	inline async_manual_reset_event::~async_manual_reset_event()
	{
		// There should be no coroutines still awaiting the event.
		assert(
			m_state.load(std::memory_order_relaxed) == nullptr ||
			m_state.load(std::memory_order_relaxed) == static_cast<void*>(this));
	}

	inline bool async_manual_reset_event::is_set() const noexcept
	{
		return m_state.load(std::memory_order_acquire) == static_cast<const void*>(this);
	}

	inline async_manual_reset_event_operation
		async_manual_reset_event::MACORO_OPERATOR_COAWAIT() const noexcept
	{
		return async_manual_reset_event_operation{ *this };
	}

	inline void async_manual_reset_event::set() noexcept
	{
		void* const setState = static_cast<void*>(this);

		// Needs 'release' semantics so that prior writes are visible to event awaiters
		// that synchronise either via 'is_set()' or 'operator co_await()'.
		// Needs 'acquire' semantics in case there are any waiters so that we see
		// prior writes to the waiting coroutine's state and to the contents of
		// the queued async_manual_reset_event_operation objects.
		void* oldState = m_state.exchange(setState, std::memory_order_acq_rel);
		if (oldState != setState)
		{
			auto* current = static_cast<async_manual_reset_event_operation*>(oldState);
			while (current != nullptr)
			{
				auto* next = current->m_next;
				current->m_awaiter.resume();
				current = next;
			}
		}
	}

	inline void async_manual_reset_event::reset() noexcept
	{
		void* oldState = static_cast<void*>(this);
		m_state.compare_exchange_strong(oldState, nullptr, std::memory_order_relaxed);
	}

	inline async_manual_reset_event_operation::async_manual_reset_event_operation(
		const async_manual_reset_event& event) noexcept
		: m_event(event)
	{
	}

	inline bool async_manual_reset_event_operation::await_ready() const noexcept
	{
		return m_event.is_set();
	}

	inline bool async_manual_reset_event_operation::await_suspend(
		coroutine_handle<> awaiter) noexcept
	{
		m_awaiter = awaiter;

		const void* const setState = static_cast<const void*>(&m_event);

		void* oldState = m_event.m_state.load(std::memory_order_acquire);
		do
		{
			if (oldState == setState)
			{
				// State is now 'set' no need to suspend.
				return false;
			}

			m_next = static_cast<async_manual_reset_event_operation*>(oldState);
		} while (!m_event.m_state.compare_exchange_weak(
			oldState,
			static_cast<void*>(this),
			std::memory_order_release,
			std::memory_order_acquire));

		// Successfully queued this waiter to the list.
		return true;
	}


	inline bool async_manual_reset_event_operation::await_suspend(
		std::coroutine_handle<> awaiter) noexcept
	{
		return await_suspend(coroutine_handle<>(awaiter));
	}
}

#endif

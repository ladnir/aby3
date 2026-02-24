///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See github.com/lewissbaker/cppcoro LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#pragma once
#include "macoro/config.h"
#ifndef MACORO_HAS_STD_STOP_TOKEN

#include "macoro/detail/stop_token.h"

#include <thread>
#include <atomic>
#include <cstdint>

namespace macoro
{
	namespace detail
	{
		struct stop_callback_state;

		class stop_state
		{
		public:

			/// Allocates a new stop_state object.
			///
			/// \throw std::bad_alloc
			/// If there was insufficient memory to allocate one.
			static stop_state* create();

			~stop_state();

			/// Increment the reference count of stop_token and
			/// stop_callback objects referencing this state.
			void add_token_ref() noexcept;

			/// Decrement the reference count of stop_token and
			/// stop_callback objects referencing this state.
			void release_token_ref() noexcept;

			/// Increment the reference count of stop_source objects.
			void add_source_ref() noexcept;

			/// Decrement the reference count of cancellation_souce objects.
			///
			/// The stop_state will no longer be cancellable once the
			/// stop_source ref count reaches zero.
			void release_source_ref() noexcept;

			/// Query if the stop_state can have cancellation requested.
			///
			/// \return
			/// Returns true if there are no more references to a stop_source
			/// object.
			bool stop_possible() const noexcept;

			/// Query if some thread has called request_stop().
			bool stop_requested() const noexcept;

			/// Flag state has having cancellation_requested and execute any
			/// registered callbacks.
			void request_stop();

			/// Try to register the stop_callback as a callback to be executed
			/// when cancellation is requested.
			///
			/// \return
			/// true if the callback was successfully registered, false if the callback was
			/// not registered because cancellation had already been requested.
			///
			/// \throw std::bad_alloc
			/// If callback was unable to be registered due to insufficient memory.
			bool try_register_callback(stop_callback* registration);

			/// Deregister a callback previously registered successfully in a call to try_register_callback().
			///
			/// If the callback is currently being executed on another
			/// thread that is concurrently calling request_stop()
			/// then this call will block until the callback has finished executing.
			void deregister_callback(stop_callback* registration) noexcept;

		private:

			stop_state() noexcept;

			bool is_cancellation_notification_complete() const noexcept;

			static constexpr std::uint64_t cancellation_requested_flag = 1;
			static constexpr std::uint64_t cancellation_notification_complete_flag = 2;
			static constexpr std::uint64_t stop_source_ref_increment = 4;
			static constexpr std::uint64_t stop_token_ref_increment = UINT64_C(1) << 33;
			static constexpr std::uint64_t stop_possible_mask = stop_token_ref_increment - 1;
			static constexpr std::uint64_t cancellation_ref_count_mask =
				~(cancellation_requested_flag | cancellation_notification_complete_flag);

			// A value that has:
			// - bit 0 - indicates whether cancellation has been requested.
			// - bit 1 - indicates whether cancellation notification is complete.
			// - bits 2-32 - ref-count for stop_source instances.
			// - bits 33-63 - ref-count for stop_token/stop_callback instances.
			std::atomic<std::uint64_t> m_state;

			std::atomic<stop_callback_state*> m_registrationState;

		};
	}

}

#endif
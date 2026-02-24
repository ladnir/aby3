///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See github.com/lewissbaker/cppcoro LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#pragma once
#include "macoro/config.h"
#ifndef MACORO_HAS_STD_STOP_TOKEN

namespace macoro
{
	class stop_token;

	namespace detail
	{
		class stop_state;
	}

	class stop_source
	{
	public:

		/// Construct to a new cancellation source.
		stop_source();

		/// Create a new reference to the same underlying cancellation
		/// source as \p other.
		stop_source(const stop_source& other) noexcept;

		stop_source(stop_source&& other) noexcept;

		~stop_source();

		stop_source& operator=(const stop_source& other) noexcept;

		stop_source& operator=(stop_source&& other) noexcept;

		/// Query if this cancellation source can be cancelled.
		///
		/// A cancellation source object will not be cancellable if it has
		/// previously been moved into another stop_source instance
		/// or was copied from a stop_source that was not cancellable.
		bool stop_possible() const noexcept;

		/// Obtain a cancellation token that can be used to query if
		/// cancellation has been requested on this source.
		///
		/// The cancellation token can be passed into functions that you
		/// may want to later be able to request cancellation.
		stop_token get_token() const noexcept;

		/// Request cancellation of operations that were passed an associated
		/// cancellation token.
		///
		/// Any cancellation callback registered via a stop_callback
		/// object will be called inside this function by the first thread to
		/// call this method.
		///
		/// This operation is a no-op if stop_possible() returns false.
		void request_stop();

		/// Query if some thread has called 'request_stop()' on this
		/// stop_source.
		bool stop_requested() const noexcept;

	private:

		detail::stop_state* m_state;

	};
}

#endif
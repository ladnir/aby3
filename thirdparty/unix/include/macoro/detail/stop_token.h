///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See github.com/lewissbaker/cppcoro LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#pragma once
#include "macoro/config.h"
#ifndef MACORO_HAS_STD_STOP_TOKEN

namespace macoro
{
	class stop_source;
	class stop_callback;

	namespace detail
	{
		class stop_state;
	}

	class stop_token
	{
	public:

		/// Construct to a cancellation token that can't be cancelled.
		stop_token() noexcept;

		/// Copy another cancellation token.
		///
		/// New token will refer to the same underlying state.
		stop_token(const stop_token& other) noexcept;

		stop_token(stop_token&& other) noexcept;

		~stop_token();

		stop_token& operator=(const stop_token& other) noexcept;

		stop_token& operator=(stop_token&& other) noexcept;

		void swap(stop_token& other) noexcept;

		/// Query if it is possible that this operation will be cancelled
		/// or not.
		///
		/// Cancellable operations may be able to take more efficient code-paths
		/// if they don't need to handle cancellation requests.
		bool stop_possible() const noexcept;

		/// Query if some thread has requested cancellation on an associated
		/// stop_source object.
		bool stop_requested() const noexcept;

		/// Throws macoro::operation_cancelled exception if cancellation
		/// has been requested for the associated operation.
		void throw_if_stop_requested() const;


	private:

		friend class stop_source;

		//template<typename Callback>
		friend class stop_callback;

		stop_token(detail::stop_state* state) noexcept;

		detail::stop_state* m_state;

	};

	inline void swap(stop_token& a, stop_token& b) noexcept
	{
		a.swap(b);
	}
}

#endif
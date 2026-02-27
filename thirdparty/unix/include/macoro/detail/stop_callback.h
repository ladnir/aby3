///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See github.com/lewissbaker/cppcoro LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#pragma once
#include "macoro/config.h"
#ifndef MACORO_HAS_STD_STOP_TOKEN

#include "macoro/detail/stop_token.h"
#include "macoro/detail/stop_state.h"

#include <functional>
#include <utility>
#include "macoro/type_traits.h"
#include "macoro/optional.h"
#include <atomic>
#include <cstdint>
#include <memory>

namespace macoro
{
	namespace detail
	{
		class stop_state;
		struct stop_callback_list_chunk;
		struct stop_callback_state;
	}

	//template<typename Callback>
	class stop_callback
	{
	public:
		using Callback = std::function<void()>;

		/// Registers the callback to be executed when cancellation is requested
		/// on the stop_token.
		///
		/// The callback will be executed if cancellation is requested for the
		/// specified cancellation token. If cancellation has already been requested
		/// then the callback will be executed immediately, before the constructor
		/// returns. If cancellation has not yet been requested then the callback
		/// will be executed on the first thread to request cancellation inside
		/// the call to stop_source::request_stop().
		///
		/// \param token
		/// The cancellation token to register the callback with.
		///
		/// \param callback
		/// The callback to be executed when cancellation is requested on the
		/// the stop_token. Note that callback must not throw an exception
		/// if called when cancellation is requested otherwise std::terminate()
		/// will be called.
		///
		/// \throw std::bad_alloc
		/// If registration failed due to insufficient memory available.
		template<
			typename FUNC,
			typename = enable_if_t<std::is_constructible<Callback, FUNC&&>::value>>
			stop_callback(stop_token token, FUNC&& callback)
			: m_callback(std::forward<FUNC>(callback))
		{
			register_callback(std::move(token));
		}

		stop_callback(const stop_callback& other) = delete;
		stop_callback& operator=(const stop_callback& other) = delete;
		stop_callback(stop_callback&& other) = delete;
		stop_callback& operator=(stop_callback&& other) = delete;

		/// Deregisters the callback.
		///
		/// After the destructor returns it is guaranteed that the callback
		/// will not be subsequently called during a call to request_stop()
		/// on the stop_source.
		///
		/// This may block if cancellation has been requested on another thread
		/// is it will need to wait until this callback has finished executing
		/// before the callback can be destroyed.
		~stop_callback()
		{
			if (m_state != nullptr)
			{
				m_state->deregister_callback(this);
				m_state->release_token_ref();
			}
		}

	private:

		friend class detail::stop_state;
		friend struct detail::stop_callback_state;

		void register_callback(stop_token&& token)
		{
			auto* state = token.m_state;
			if (state != nullptr && state->stop_possible())
			{
				m_state = state;
				if (state->try_register_callback(this))
				{
					token.m_state = nullptr;
				}
				else
				{
					m_state = nullptr;
					m_callback();
				}
			}
			else
			{
				m_state = nullptr;
			}
		}

		detail::stop_state* m_state;
		Callback m_callback;
		detail::stop_callback_list_chunk* m_chunk;
		std::uint32_t m_entryIndex;
	};
}

#endif
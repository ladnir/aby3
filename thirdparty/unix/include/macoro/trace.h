#pragma once


#include "macoro/config.h"
#include <string>
#include <sstream>
#include "macoro/coroutine_handle.h"
#include <source_location>
#include <mutex>

namespace macoro
{
	struct traceable
	{
		// where in the parent this frame was started.
		std::source_location m_location;

		// the caller. Can be null. We will set this value
		traceable* m_parent = nullptr;

		void set_parent(traceable* parent, const std::source_location& l)
		{
			m_location = l;
			m_parent = parent;
		}

		virtual void get_call_stack(std::vector<std::source_location>& stack)
		{
			stack.push_back(m_location);
			if (m_parent)
				m_parent->get_call_stack(stack);
		}


		virtual void add_child(traceable* child) = 0;
		virtual void remove_child(traceable* child) = 0;
	};

	// a traceable that can only have one parent and child. 
	// This is not thread safe.
	struct basic_traceable : public traceable
	{
		traceable* m_child = nullptr;

		void get_call_stack(std::vector<std::source_location>& stack) final override
		{
			stack.push_back(m_location);
			if (m_parent)
				m_parent->get_call_stack(stack);
		}

		void add_child(traceable* child) final override
		{
			assert(m_child == nullptr);
			m_child = child;
		}

		void remove_child(traceable* child) final  override
		{
			assert(m_child == child);
			m_child = nullptr;
		}

	};


	template <typename T>
	concept is_traceable =
		requires(T t) {
			{ t } -> std::convertible_to<macoro::traceable&>;
	};

	template <typename T>
	concept has_traceable =
		requires(T t) {
			{ t.m_trace } -> std::convertible_to<macoro::traceable&>;
	};

	template <typename T>
	concept has_traceable_fn =
		requires(T t) {
			{ t.get_traceable() } -> std::convertible_to<macoro::traceable*>;
	};

	template <typename T>
	concept has_traceable_ptr =
		requires(T t) {
			{ t.m_trace } -> std::convertible_to<macoro::traceable*>;
	};

	namespace detail
	{

		template<typename Promise>
		traceable* get_traceable(Promise& p)
		{
			if constexpr (is_traceable<Promise>)
				return &p;
			if constexpr (has_traceable_fn<Promise>)
				return p.get_traceable();
			if constexpr (has_traceable<Promise>)
				return &p.m_trace;
			if constexpr (has_traceable_ptr<Promise>)
				return p.m_trace;
			else
				return nullptr;
		}

		template<typename Promise>
		traceable* get_traceable(std::coroutine_handle<Promise> p)
		{
			if constexpr (!std::is_same_v<Promise, void>)
				return get_traceable(p.promise());
			else
				return nullptr;

		}

		template<typename Promise>
		traceable* get_traceable(coroutine_handle<Promise> p)
		{
			if constexpr (!std::is_same_v<Promise, void>)
				return get_traceable(p.promise());
			else
				return nullptr;

		}
	}



	struct trace {

		trace(std::source_location l, traceable& c)
		{ 
			stack.push_back(l);
			c.get_call_stack(stack);
		}

		std::vector<std::source_location> stack;
		
		std::string str()
		{
			std::stringstream ss;
			for(auto i = 0ull; i < stack.size(); ++i)
			{
				ss << i << " " << stack[i].file_name() << ":" << stack[i].line() << "\n";
			}
			return ss.str();
		}
	};

	struct get_trace {

		get_trace(std::source_location l = std::source_location::current())
			:location(l)
		{ }
		std::source_location location;

		struct awaitable
		{
			awaitable(trace&& t)
				: mTrace(std::move(t))
			{}

			bool await_ready() const noexcept { return true; }
#ifdef MACORO_CPP_20
			void await_suspend(std::coroutine_handle<> coro) noexcept
			{
				assert(0);
			}
#endif

			void await_suspend(
				coroutine_handle<> coro) noexcept
			{
				assert(0);
			}


			trace await_resume() noexcept { return mTrace; }
			trace mTrace;
		};
	};
	//template<typename Promise>
	//void set_trace(Promise& p, async_stack_frame* a)
	//{
	//	if constexpr (has_async_stack_frame<Promise>)
	//		return p.async_stack_frame = a;
	//	else
	//		return;
	//}

	//template<typename Promise1, typename Promise2>
	//void connect_trace(Promise1& patent, Promise2 child) noexcept
	//{
	//	set_trace(child, get_trace(patent);
	//}

	//thread_local async_stack_frame* current_async_stack_frame = nullptr;
}

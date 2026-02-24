#pragma once

//#include "macoro/Context.h"
#include "coro_frame.h"
#include <iostream>

#define MACORO_CAT(a, b)   MACORO_PP_CAT_I(a, b)
#define MACORO_PP_CAT_I(a, b) MACORO_PP_CAT_II(~, a ## b)
#define MACORO_PP_CAT_II(p, res) res
#define MACORO_EXPAND(x) MACORO_PP_EXPAND_I(x)
#define MACORO_PP_EXPAND_I(x) x

// This macro expands out to generated the awaiter/awaitable constructors, and
// then the await_ready and await_suspend calls. This should be used with other 
// macros which perform the await_resume call and define the suspend case.
// 
// The first thing of note is that when we are constructing the AwaitContext
// we use a meta function is_lvalue to decide if the expression is an
// lvalue. If so, we capture it by reference. Otherwise we capture it as what
// ever type it is.
// 
// Next we construct the awaiter from the expression. We do this by placement
// new'ing the expression. Then calling get_awaitable(...) followed by 
// get_awaiter(...). Each of these results is given storage if needed, otherwise
// the placement new is just constructing a pointer-like type that references
// the value returned from the function.
// 
// Then we have the awaiter, we call await_ready and await_suspend.
#define IMPL_MC_AWAIT_CORE_NS(EXPRESSION, SUSPEND_IDX)																	\
	using _macoro_promise_type = decltype(_macoro_frame_->promise);														\
	using MACORO_CAT(_macoro_AwaitContext,SUSPEND_IDX) = ::macoro::AwaitContext<_macoro_promise_type, SUSPEND_IDX,		\
		::macoro::store_as_t<decltype(::macoro::as_reference(EXPRESSION)), decltype(EXPRESSION)>>;						\
	using _macoro_Handle = ::macoro::coroutine_handle<_macoro_promise_type>;											\
	{																													\
		auto& _macoro_promise = _macoro_frame_->promise;																\
		auto _macoro_handle = _macoro_Handle::from_promise(_macoro_promise, ::macoro::coroutine_handle_type::macoro);	\
		auto& _macoro_ctx = _macoro_frame_->template makeAwaitContext<MACORO_CAT(_macoro_AwaitContext, SUSPEND_IDX)>();	\
		_macoro_ctx.expr.ptr      = new (_macoro_ctx.expr.v())      typename decltype(_macoro_ctx.expr)::Constructor(EXPRESSION);															\
		_macoro_ctx.awaitable.ptr = new (_macoro_ctx.awaitable.v()) typename decltype(_macoro_ctx.awaitable)::Constructor(macoro::get_awaitable(_macoro_promise, _macoro_ctx.expr.get()));	\
		_macoro_ctx.awaiter.ptr   = new (_macoro_ctx.awaiter.v())   typename decltype(_macoro_ctx.awaiter)::Constructor(macoro::get_awaiter(_macoro_ctx.awaitable.get()));					\
		auto& _macoro_awaiter = _macoro_ctx.awaiter.getRef();													\
		*_macoro_ctx.awaiter_ptr = (void*)&_macoro_awaiter;														\
		if (!_macoro_awaiter.await_ready())																		\
		{																										\
			/*<suspend-coroutine>*/																				\
			_macoro_frame_->_suspension_idx_ = (::macoro::SuspensionPoint)SUSPEND_IDX;							\
																												\
			/*<call awaiter.await_suspend(). If it's void return, then return true.>*/							\
			/*perform symmetric transfer or return to caller (for noop_coroutine) */							\
			auto _macoro_s = ::macoro::await_suspend(_macoro_awaiter, _macoro_handle);							\
			if (_macoro_s)																						\
				return _macoro_s.get_handle();																	\
		}																										\
	}																					

// This macro expands out to generated the await_ready and await_suspend calls.
// It defines a suspension point but does not call await_resume. This should be
// used with other macros which perform the await_resume call.
#define IMPL_MC_AWAIT_CORE(EXPRESSION, SUSPEND_IDX)	IMPL_MC_AWAIT_CORE_NS(EXPRESSION, SUSPEND_IDX)				\
			MACORO_FALLTHROUGH; case (std::size_t)::macoro::SuspensionPoint(SUSPEND_IDX):		

// a helper macro that defines a basic co_await expression. 
// RETURN_SLOT can be used to set a variable to the result of the co_await expression.
// SUSPEND_IDX should be a unique integer literal. Typically obtained using __COUNTER__.
#define IMPL_MC_AWAIT(EXPRESSION, RETURN_SLOT, SUSPEND_IDX)														\
				do {																							\
					IMPL_MC_AWAIT_CORE(EXPRESSION, SUSPEND_IDX)													\
					/*<resume-point>*/																			\
					RETURN_SLOT _macoro_frame_->template getAwaiter<MACORO_CAT(_macoro_AwaitContext,SUSPEND_IDX)>().await_resume();		\
					_macoro_frame_->template destroyAwaiter<MACORO_CAT(_macoro_AwaitContext,SUSPEND_IDX)>();		\
				} while(0)

// a helper macro that defines a try{ co_await expression } catch(...) {}. 
// RETURN_SLOT can be used to set a variable to the result of the co_await expression.
// SUSPEND_IDX should be a unique integer literal. Typically obtained using __COUNTER__.
#define IMPL_MC_AWAIT_TRY(EXPRESSION, RETURN_SLOT, SUSPEND_IDX)													\
				do{																								\
					IMPL_MC_AWAIT_CORE(EXPRESSION, SUSPEND_IDX)													\
					/*<resume-point>*/																			\
					::macoro::detail::try_resume(RETURN_SLOT, 													\
						_macoro_frame_->template getAwaiter<MACORO_CAT(_macoro_AwaitContext,SUSPEND_IDX)>());	\
					_macoro_frame_->template destroyAwaiter<MACORO_CAT(_macoro_AwaitContext,SUSPEND_IDX)>();	\
				} while(0)

// A helper macro that implements the MC_YIELD_AWAIT. We first perform a normal co_await
// and then we call another co_await on the result of promise.yield_value(awaiter.await_resume()).
#define IMPL_MC_YIELD_AWAIT(EXPRESSION, SUSPEND_IDX)															\
				do {	IMPL_MC_AWAIT_CORE(EXPRESSION, SUSPEND_IDX)												\
					/*<resume-point>*/																			\
					IMPL_MC_AWAIT(_macoro_frame_->promise.yield_value(_macoro_frame_->template getAwaiter<MACORO_CAT(_macoro_AwaitContext,SUSPEND_IDX)>().await_resume()), ,__COUNTER__);\
					_macoro_frame_->template destroyAwaiter<MACORO_CAT(_macoro_AwaitContext,SUSPEND_IDX)>();		\
				} while(0)


// A helper macro that implements the MC_AWAIT_AWAIT. We first perform a normal co_await
// and then we call another co_await on the result of awaiter.await_resume().
#define IMPL_MC_AWAIT_AWAIT(EXPRESSION, SUSPEND_IDX)															\
				do{	IMPL_MC_AWAIT_CORE(EXPRESSION, SUSPEND_IDX)													\
					/*<resume-point>*/																			\
					IMPL_MC_AWAIT(_macoro_frame_->template getAwaiter<MACORO_CAT(_macoro_AwaitContext,SUSPEND_IDX)>().await_resume(), ,__COUNTER__);\
					_macoro_frame_->template destroyAwaiter<MACORO_CAT(_macoro_AwaitContext,SUSPEND_IDX)>();		\
				} while(0)


// A helper macro that implements the MC_AWAIT_FN. We first perform a normal co_await
// and then we call the function with the await_resume result as the parameter.
#define IMPL_MC_AWAIT_FN(FN_SLOT, EXPRESSION, OPTIONAL_GOTO, SUSPEND_IDX)										\
				do{																								\
					IMPL_MC_AWAIT_CORE(EXPRESSION, SUSPEND_IDX)													\
					/*<resume-point>*/																			\
					FN_SLOT(_macoro_frame_->template getAwaiter<MACORO_CAT(_macoro_AwaitContext,SUSPEND_IDX)>().await_resume());							\
					_macoro_frame_->template destroyAwaiter<MACORO_CAT(_macoro_AwaitContext,SUSPEND_IDX)>();		\
					OPTIONAL_GOTO																				\
				} while(0)


// Begin a lambda based coroutine which returns the given type.
#define MC_BEGIN(ReturnType, ...)																				\
do { auto _macoro_frame_ = ::macoro::makeFrame<typename ::macoro::coroutine_traits<ReturnType>::promise_type>(			\
	[__VA_ARGS__](::macoro::FrameBase<typename ::macoro::coroutine_traits<ReturnType>::promise_type>* _macoro_frame_) mutable ->::macoro::coroutine_handle<void>\
	{																											\
		try {																									\
																												\
			switch ((std::size_t)_macoro_frame_->_suspension_idx_)															\
			{																									\
			case (std::size_t)::macoro::SuspensionPoint::InitialSuspendBegin:												\
				do {																								\
					IMPL_MC_AWAIT_CORE(_macoro_frame_->promise.initial_suspend(), MACORO_INITIAL_SUSPEND_IDX)	\
					_macoro_frame_->_initial_suspend_await_resumed_called_ = true;								\
					_macoro_frame_->template getAwaiter<MACORO_CAT(_macoro_AwaitContext,MACORO_INITIAL_SUSPEND_IDX)>().await_resume();			\
					_macoro_frame_->template destroyAwaiter<MACORO_CAT(_macoro_AwaitContext,MACORO_INITIAL_SUSPEND_IDX)>();						\
				} while(0)


// Perform a co_yield co_await expression. This is equivalent to
// `co_yield (co_await EXPRESSION)`
#define MC_YIELD_AWAIT(EXPRESSION) IMPL_MC_YIELD_AWAIT(EXPRESSION, __COUNTER__)


// Perform a co_await co_await expression. This is equivalent to
// `co_await (co_await EXPRESSION)`
#define MC_AWAIT_AWAIT(EXPRESSION) IMPL_MC_AWAIT_AWAIT(EXPRESSION, __COUNTER__)


// Calls the given function on the result of the co_await expression. This is equivalent to
// `f(co_await EXPRESSION)` where f is the function name provided.
#define MC_AWAIT_FN(FN_SLOT, EXPRESSION)	IMPL_MC_AWAIT_FN(FN_SLOT,EXPRESSION, , __COUNTER__)					

// Returns void. Equivalent to `co_return;`
#define MC_RETURN_VOID()																						\
				do{ _macoro_frame_->promise.return_void();														\
				goto MACORO_FINAL_SUSPEND_BEGIN; } while(0)

// Returns a value. Equivalent to `co_return X;`
#define MC_RETURN(X)																							\
				do { _macoro_frame_->promise.return_value(X);													\
				goto MACORO_FINAL_SUSPEND_BEGIN; } while(0)


// perform a basic co_await expression. This is equivalent to
// `co_await EXPRESSION`
#define MC_AWAIT(EXPRESSION)				IMPL_MC_AWAIT(EXPRESSION, , __COUNTER__)


// perform a basic co_await expression and set VARIABLE to the result. This is equivalent to
// `VARIABLE = co_await EXPRESSION`
#define MC_AWAIT_SET(VARIABLE, EXPRESSION)	IMPL_MC_AWAIT(EXPRESSION, VARIABLE = , __COUNTER__)


// perform a basic co_await expression and set VARIABLE to the Ok(result) or Err(std::current_exception()). This is equivalent to
// `try { VARIABLE = Ok(co_await EXPRESSION); } catch (...) {VARIABLE = Err(std::current_exception()); }`
#define MC_AWAIT_TRY(VARIABLE, EXPRESSION)	IMPL_MC_AWAIT_TRY(EXPRESSION, VARIABLE , __COUNTER__)


// perform a basic co_await expression and return to the result. This is equivalent to
// `co_return co_await EXPRESSION`
#define MC_RETURN_AWAIT(EXPRESSION)			IMPL_MC_AWAIT_FN(_macoro_frame_->promise.return_value, EXPRESSION, goto MACORO_FINAL_SUSPEND_BEGIN;,__COUNTER__)


// perform a basic co_yeild expression. This is equivalent to
// `co_yeild EXPRESSION`
#define MC_YIELD(EXPRESSION)				MC_AWAIT(_macoro_frame_->promise.yield_value(EXPRESSION));


// perform a basic co_yeild expression and set VARIABLE to the result. This is equivalent to
// `VARIABLE = co_yeild EXPRESSION`
#define MC_YIELD_SET(VARIABLE, EXPRESSION)	MC_AWAIT_SET(VARIABLE , _macoro_frame_->promise.yield_value(EXPRESSION));

// perform a basic co_yeild expression and return to the result. This is equivalent to
// `co_return co_yeild EXPRESSION`
#define MC_RETURN_YIELD(EXPRESSION)			MC_RETURN_AWAIT(_macoro_frame_->promise.yield_value(EXPRESSION));

#define MC_UNHANDLED_EXCEPTION _macoro_frame_->promise.unhandled_exception()

// End the coroutine body.
#define MC_END()																								\
				break; 																							\
			case (std::size_t)::macoro::SuspensionPoint::FinalSuspend:														\
				goto MACORO_FINAL_SUSPEND_RESUME;																\
			default:																							\
				std::cout << "coroutine frame corrupted. " <<__FILE__ <<":" <<__LINE__ << std::endl;			\
				std::terminate();																				\
				break;																							\
			}																									\
		}																										\
		catch (...) {																							\
			_macoro_frame_->destroyAwaiters();																	\
			if (!_macoro_frame_->_initial_suspend_await_resumed_called_)										\
				throw; 																							\
			MC_UNHANDLED_EXCEPTION;																				\
		}																										\
		/*final suspend*/																						\
goto MACORO_FINAL_SUSPEND_BEGIN; /*prevents unused warning*/													\
MACORO_FINAL_SUSPEND_BEGIN:																						\
		_macoro_frame_->final_suspend();																		\
		IMPL_MC_AWAIT_CORE_NS(_macoro_frame_->promise.final_suspend(), MACORO_FINAL_SUSPEND_IDX)				\
MACORO_FINAL_SUSPEND_RESUME:																					\
		_macoro_frame_->template getAwaiter<MACORO_CAT(_macoro_AwaitContext, MACORO_FINAL_SUSPEND_IDX)>().await_resume();	\
		_macoro_frame_->template destroyAwaiter<MACORO_CAT(_macoro_AwaitContext, MACORO_FINAL_SUSPEND_IDX)>();	\
		_macoro_frame_->destroy(_macoro_frame_);																\
		return ::macoro::noop_coroutine();																				\
	});																											\
																												\
	auto _macoro_ret_ = _macoro_frame_->promise.macoro_get_return_object();										\
	using macoro_promise_type = decltype(_macoro_frame_->promise);												\
	using Handle = ::macoro::coroutine_handle<macoro_promise_type>;												\
	macoro_promise_type& promise = _macoro_frame_->promise;														\
	auto handle = Handle::from_promise(promise, ::macoro::coroutine_handle_type::macoro);						\
	handle.resume();																							\
	return _macoro_ret_;																						\
} while (0)



#define MACORO_TRY std::exception_ptr macoro_ePtr; try
#define MACORO_CATCH(NAME)  catch(...) { macoro_ePtr = std::current_exception(); } if(auto NAME = std::exchange(macoro_ePtr, nullptr))  

#pragma once
#include <type_traits>
#include "macoro/config.h"

namespace macoro
{

	template< class T >
	struct remove_cvref {
		using type = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
	};


	template< class T >
	using remove_cvref_t = typename remove_cvref<T>::type;


	template <bool Cond, typename T = void>
	using enable_if_t = typename std::enable_if<Cond, T>::type;

	template <typename ...>
	struct _void_t_impl
	{
		typedef void type;
	};

	template <typename ... T>
	using void_t = typename _void_t_impl<T...>::type;

	using false_type = std::false_type;
	using true_type = std::true_type;

	template<class...> struct conjunction : true_type { };
	template<class B1> struct conjunction<B1> : B1 { };
	template<class B1, class... Bn>
	struct conjunction<B1, Bn...>
		: std::conditional<bool(B1::value), conjunction<Bn...>, B1>::type {};

	template<typename Promise, typename Expression, typename = void>
	struct has_await_transform_member : false_type
	{};

	template <typename Promise, typename Expression>
	struct has_await_transform_member <Promise, Expression, void_t<

		// must have a await_transform(...) member fn
		decltype(std::declval<Promise>().await_transform(std::declval<Expression>()))

		>>
		: true_type{};

	template<typename T, typename = void>
	struct has_member_operator_co_await : false_type
	{};

	template <typename T>
	struct has_member_operator_co_await <T, void_t<

#ifdef MACORO_CPP_20
		// must have a operator co_await() member fn
		decltype(std::declval<T>().operator co_await())
#else
		// must have a operator_co_await() member fn
		decltype(std::declval<T>().operator_co_await())
#endif
		>>
		: true_type{};


	template<typename T, typename = void>
	struct has_free_operator_co_await : false_type
	{};


	template <typename T>
	struct has_free_operator_co_await <T, void_t<
#ifdef MACORO_CPP_20
		// must have a operator_co_await() member fn
		decltype(operator co_await(std::declval<T>()))
#else
		// must have a operator_co_await() member fn
		decltype(operator_co_await(std::declval<T>()))
#endif
		>>
		: true_type{};


	template<typename Awaiter, typename T, typename = void>
	struct has_void_await_suspend : false_type
	{};

	template <typename Awaiter, typename T>
	struct has_void_await_suspend <Awaiter, T, void_t<

		// must have a await_suspend(...) member fn
		decltype(std::declval<Awaiter>().await_suspend(std::declval<T>())),

		// must return void
		enable_if_t<std::is_same<
			decltype(std::declval<Awaiter>().await_suspend(std::declval<T>())),
			void
		>::value, void>

		>>
		: true_type{};


	template<typename Awaiter, typename T, typename = void>
	struct has_bool_await_suspend : false_type
	{};

	template <typename Awaiter, typename T>
	struct has_bool_await_suspend <Awaiter, T, void_t<

		// must have a await_suspend(...) member fn
		decltype(std::declval<Awaiter>().await_suspend(std::declval<T>())),

		// must return bool
		enable_if_t<std::is_same<
			decltype(std::declval<Awaiter>().await_suspend(std::declval<T>())),
			bool
		>::value, void>

		>>
		: true_type{};


	template<typename Awaiter, typename T, typename = void>
	struct has_any_await_suspend : false_type
	{};

	template <typename Awaiter, typename T>
	struct has_any_await_suspend <Awaiter, T, void_t<

		// must have a await_suspend(...) member fn
		decltype(std::declval<Awaiter>().await_suspend(std::declval<T>()))
		>>
		: true_type{};

	struct empty_state {};

	template<typename P, typename T>
	inline decltype(auto) get_awaitable(
		P& promise, T&& expr, enable_if_t<has_await_transform_member<P,T&&>::value, empty_state> m = {})
	{
		return promise.await_transform(static_cast<T&&>(expr));
	}

	template<typename P, typename T>
	inline decltype(auto) get_awaitable(
		P& promise, T&& expr, enable_if_t<!has_await_transform_member<P,T&&>::value, empty_state> = {})
	{
		return static_cast<T&&>(expr);
	}

	template<typename Awaitable>
	inline decltype(auto) get_awaiter(
		Awaitable&& awaitable, 
		enable_if_t<
			has_member_operator_co_await<Awaitable>::value, empty_state> = {})
	{
#ifdef MACORO_CPP_20
		return static_cast<Awaitable&&>(awaitable).operator co_await();
#else
		return static_cast<Awaitable&&>(awaitable).operator_co_await();
#endif
	}

	template<typename Awaitable>
	inline decltype(auto) get_awaiter(
		Awaitable&& awaitable, 
		enable_if_t<
			!has_member_operator_co_await<Awaitable>::value&&
			has_free_operator_co_await<Awaitable>::value, empty_state> = {})
	{
#ifdef MACORO_CPP_20
		return operator co_await(static_cast<Awaitable&&>(awaitable));
#else
		return operator_co_await(static_cast<Awaitable&&>(awaitable));
#endif
	}

	template<typename Awaitable>
	inline decltype(auto) get_awaiter(
		Awaitable&& awaitable,
		enable_if_t<
			!has_member_operator_co_await<Awaitable>::value &&
			!has_free_operator_co_await<Awaitable>::value, empty_state> = {})
	{
		return static_cast<Awaitable&&>(awaitable);
	}

	template<typename promise_type, typename Expr>
	struct awaiter_for
	{
		using expr = Expr;
		using awaitable = decltype(get_awaitable(std::declval<promise_type&>(), std::declval<Expr&&>()));
		using awaiter = decltype(get_awaiter(std::declval<awaitable>()));
		using type = awaiter;

		using value_type = typename std::remove_reference<awaiter>::type;
		using pointer = value_type*;
		using reference = value_type&;
	};


	template<typename T, typename = void_t<>>
	struct is_awaitable : std::false_type {};

	template<typename awaitable>
	struct is_awaitable<awaitable, void_t<decltype(get_awaiter(std::declval<awaitable>()))>>
		: std::true_type
	{};

	template<typename T>
	constexpr bool is_awaitable_v = is_awaitable<T>::value;

	template<typename T, typename = void>
	struct awaitable_traits
	{};

	//template<typename T>
	//struct awaitable_traits<T, std::void_t<decltype(macoro::detail::get_awaiter(std::declval<T>()))>>
	//{
	//	using awaiter_t = decltype(macoro::detail::get_awaiter(std::declval<T>()));

	//	using await_result_t = decltype(std::declval<awaiter_t>().await_resume());
	//};

	template<typename awaitable>
	struct awaitable_traits<awaitable, void_t<decltype(get_awaiter(std::declval<awaitable>()))>>
	{
		using awaiter = decltype(get_awaiter(std::declval<awaitable>()));
		using await_result = decltype(std::declval<awaiter>().await_resume());

		using awaiter_t = awaiter;
		using await_result_t = await_result;
	};

	template<typename awaitable>
	using awaitable_result_t = typename awaitable_traits<awaitable>::await_result;

	template<typename C, typename T, typename = void>
	struct has_set_continuation_member : false_type
	{};

	template <typename C, typename T>
	struct has_set_continuation_member <C, T, void_t<

		// must have a promise().set_continuation(T) member fn
		decltype(std::declval<C>().promise().set_continuation(std::declval<T>()))

		>>
		: true_type{};


	template<typename T>
	using remove_reference_t = typename std::remove_reference<T>::type;

	template<typename T>
	using remove_rvalue_reference_t = typename std::conditional<std::is_rvalue_reference<T>::value,
		remove_reference_t<T>,
		T >::type;


	template<typename T>
	struct unwrap_reference
	{
		using type = T;
	};

	template<typename T>
	struct unwrap_reference<std::reference_wrapper<T>>
	{
		using type = T;
	};

	template<typename T>
	using remove_reference_wrapper_t = typename unwrap_reference<T>::type;


	template<typename T>
	using remove_reference_and_wrapper_t = remove_reference_wrapper_t<remove_reference_t<T>>;
}
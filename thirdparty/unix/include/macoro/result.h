#pragma once
#include "type_traits.h"
#include "variant.h"
#include "error_code.h"
#include "coroutine_handle.h"
#include "awaiter.h"
#include "variant.h"

namespace macoro
{

	template<typename T, typename Error>
	class result;
	namespace detail
	{


		template<typename T>
		struct OkTag {
			const T& mV;

			OkTag(T& e)
				:mV(e)
			{}

			operator const T& ()
			{
				return mV;
			}
		};

		template<>
		struct OkTag<void> { };


		template<typename T>
		struct OkMvTag {
			T& mV;


			OkMvTag(T& e)
				:mV(e)
			{}


			operator T && ()
			{
				return (T&&)mV;
			}
		};

		template<typename E>
		struct ErrorTag {
			const E& mE;

			ErrorTag(E& e)
				:mE(e)
			{}

			operator const E& ()
			{
				return mE;
			}
		};

		template<typename E>
		struct ErrorMvTag {
			E& mE;

			ErrorMvTag(E& e)
				:mE(e)
			{}

			operator E && ()
			{
				return std::move(mE);
			}
		};

		template<typename T, typename Error>
		enable_if_t<std::is_default_constructible<T>::value, variant<T, Error>> resultDefaultConstruct()
		{
			return variant<T, Error>{ MACORO_VARIANT_NAMESPACE::in_place_index<0> };
		}

		template<typename T, typename Error>
		enable_if_t<!std::is_default_constructible<T>::value, variant<T, Error>> resultDefaultConstruct()
		{
			static_assert(std::is_default_constructible<Error>::value, "either T or Error must be default constructible to default construct a result.");
			return variant<T, Error>{ MACORO_VARIANT_NAMESPACE::in_place_index<1> };
		}


	}


	template<typename T>
	detail::ErrorTag<remove_cvref_t<T>> Err(const T& t)
	{
		return detail::ErrorTag<remove_cvref_t<T>>(t);
	}

	template<typename T>
	detail::ErrorMvTag<remove_cvref_t<T>> Err(T&& t)
	{
		return detail::ErrorMvTag<remove_cvref_t<T>>(t);
	}


	inline detail::OkTag<void> Ok()
	{
		return {};
	}

	template<typename T>
	detail::OkTag<remove_cvref_t<T>> Ok(const T& t)
	{
		return detail::OkTag<remove_cvref_t<T>>(t);
	}

	template<typename T>
	detail::OkMvTag<remove_cvref_t<T>> Ok(T&& t)
	{
		return detail::OkMvTag<remove_cvref_t<T>>(t);
	}


	template<typename E>
	void result_rethrow(E&& e)
	{
		if constexpr (std::is_same_v<std::remove_cvref_t<E>, std::exception_ptr>)
			std::rethrow_exception(e);
		else
			throw e;
	}

	namespace detail
	{
		template<typename R, typename AWAITER>
		enable_if_t<std::is_void< awaitable_result_t<AWAITER>>::value>
			try_resume(R&& r, AWAITER&& a)
		{
			try {
				a.await_resume();
				r = Ok();
			}
			catch (...) { r = Err(std::current_exception()); }
		}

		template<typename R, typename AWAITER>
		enable_if_t<std::is_void< awaitable_result_t<AWAITER>>::value == false>
			try_resume(R&& r, AWAITER&& a)
		{
			try {
				r = Ok(a.await_resume());
			}
			catch (...) { r = Err(std::current_exception()); }
		}

		template<typename T, typename E>
		auto result_unhandled_exception()
		{
			static_assert(std::is_same<std::exception_ptr, E>::value,
				"for custom result error types with coroutines, the user must implement result_unhandled_exception. This could be to just be rethrow.");
			return std::current_exception();
		}

		template<typename T, typename E>
		struct result_awaiter
		{
			result<T, E>* mResult;

			bool await_ready() noexcept { return true; }

#ifdef MACORO_CPP_20
			template<typename P>
			void await_suspend(const std::coroutine_handle<P>&) noexcept {}
#endif
			template<typename P>
			void await_suspend(const coroutine_handle<P>&) noexcept {}
			T await_resume()
			{
				if (mResult->has_error())
					result_rethrow(mResult->error());

				return std::move(mResult->value());
			}
		};
		template<typename E>
		struct result_awaiter<void, E>
		{
			result<void, E>* mResult;

			bool await_ready() noexcept { return true; }

#ifdef MACORO_CPP_20
			template<typename P>
			void await_suspend(const std::coroutine_handle<P>&) noexcept {}
#endif
			template<typename P>
			void await_suspend(const coroutine_handle<P>&) noexcept {}
			void await_resume()
			{
				if (mResult->has_error())
					result_rethrow(mResult->error());
			}
		};

		template<typename T, typename E>
		struct result_promise
		{
			optional<result<T,E>> mResult;

			suspend_never initial_suspend() const noexcept { return {}; }
			suspend_always final_suspend() const noexcept { return {}; }

			struct Ret
			{
				coroutine_handle<result_promise> handle;

				operator result<T, E>() const
				{

					auto& promise = handle.promise();
					auto r = std::move(promise.mResult.value());
					handle.destroy();
					return r;
				}
			};

			Ret get_return_object()
			{
				return { coroutine_handle<result_promise>::from_promise(*this, coroutine_handle_type::std) };
			}
			Ret macoro_get_return_object()
			{
				return { coroutine_handle<result_promise>::from_promise(*this, coroutine_handle_type::macoro) };
			}

			template<typename TT>
			suspend_never await_transform(TT&&) = delete;

			template<typename TT>
			decltype(auto) await_transform(result<TT, E>&& r)
			{
				return std::forward<result<TT, E>>(r);
			}

			template<typename TT>
			decltype(auto) await_transform(result<TT, E>& r)
			{
				return std::forward<result<TT, E>>(r);
			}

			void unhandled_exception()
			{
				mResult.emplace(Err(result_unhandled_exception<T, E>()));
			}

			template<typename TT>
			void return_value(TT&& t)
			{
				mResult.emplace(std::forward<TT>(t));
			}
		};

	}


	template<typename T, typename Error = std::exception_ptr>
	class result
	{
	public:
		using promise_type = detail::result_promise<T, Error>;
		using value_type = remove_cvref_t<T>;
		using error_type = remove_cvref_t<Error>;


		result()
			: mVar(detail::resultDefaultConstruct<T,Error>())
		{
		}


		result(const result&) = default;
		result(result&&) = default;
		result& operator=(const result&) = default;
		result& operator=(result&&) = default;

		result(detail::OkTag<value_type>&& v) :mVar(v.mV) {}
		result(detail::OkMvTag<value_type>&& v) : mVar(std::move(v.mV)) {}
		result(detail::ErrorTag<error_type>&& e) : mVar(e.mE) {}
		result(detail::ErrorMvTag<error_type>&& e) :mVar(std::move(e.mE)) {}


		variant<value_type, error_type> mVar;
		variant<value_type, error_type>& var() {
			return mVar;
		};
		const variant<value_type, error_type>& var() const {
			return mVar;
		};


		bool has_value() const {
			return var().index() == 0;
		}

		bool has_error() const {
			return !has_value();
		}

		explicit operator bool() {
			return has_value();
		}

		value_type& value()
		{
			if (has_error())
				result_rethrow(error());

			return MACORO_VARIANT_NAMESPACE::get<0>(var());
		}

		const value_type& value() const
		{
			if (has_error())
				result_rethrow(error());

			return value();
		}

		const value_type& operator*() const { return value(); }

		value_type& operator*() { return value(); }

		error_type& error()
		{
			if (has_value())
				throw std::runtime_error("error() was called on a Result<T,E> which stores an value_type");

			return MACORO_VARIANT_NAMESPACE::get<1>(var());
		}

		const error_type& error() const
		{
			if (has_value())
				throw std::runtime_error("error() was called on a Result<T,E> which stores an value_type");

			return MACORO_VARIANT_NAMESPACE::get<1>(var());
		}


		value_type value_or(value_type&& alt)
		{
			if (has_error())
				return alt;
			return value();
		}


		value_type& value_or(value_type& alt)
		{
			if (has_error())
				return alt;
			return value();
		}

		const value_type& value_or(const value_type& alt) const
		{
			if (has_error())
				return alt;
			return value();
		}


		error_type error_or(error_type&& alt)
		{
			if (has_error())
				return error();
			return alt;
		}

		error_type& error_or(error_type& alt)
		{
			if (has_error())
				return error();
			return alt;
		}

		const error_type& error_or(const error_type& alt) const
		{
			if (has_error())
				return error();
			return alt;
		}


		template<typename V>
		void operator=(detail::OkMvTag<V>&& v)
		{
			var() = variant<value_type, error_type>{ MACORO_VARIANT_NAMESPACE::in_place_index<0>, std::move(v.mV) };
		}
		template<typename V>
		void operator=(detail::OkTag<V>&& v)
		{
			var() = variant<value_type, error_type>{ MACORO_VARIANT_NAMESPACE::in_place_index<0>, v.mV };
		}

		template<typename E>
		void operator=(detail::ErrorMvTag<E>&& v)
		{
			var() = variant<value_type, error_type>{ MACORO_VARIANT_NAMESPACE::in_place_index<1>, std::move(v.mE) };
		}
		template<typename E>
		void operator=(detail::ErrorTag<E>&& v)
		{
			var() = variant<value_type, error_type>{ MACORO_VARIANT_NAMESPACE::in_place_index<1>, v.mE };
		}


		template<typename T2>
		bool operator==(detail::OkTag<T2>&& v) const
		{
			return (has_value() && value() == v.mV);
		}

		template<typename T2>
		bool operator==(detail::OkMvTag<T2>&& v)const
		{
			return (has_value() && value() == v.mV);
		}

		template<typename E2>
		bool operator==(detail::ErrorTag<E2>&& v)const
		{
			return (has_error() && error() == v.mE);
		}
		template<typename E2>
		bool operator==(detail::ErrorMvTag<E2>&& v)const
		{
			return (has_error() && error() == v.mE);
		}

		template<typename T2>
		bool operator!=(detail::OkTag<T2>&& v)const
		{
			return !(*this == v);
		}

		template<typename T2>
		bool operator!=(detail::OkMvTag<T2>&& v)const
		{
			return !(*this == v);
		}

		template<typename E2>
		bool operator!=(detail::ErrorTag<E2>&& v)const
		{
			return !(*this == v);
		}
		template<typename E2>
		bool operator!=(detail::ErrorMvTag<E2>&& v)const
		{
			return !(*this == v);
		}


		detail::result_awaiter<T, Error> MACORO_OPERATOR_COAWAIT()
		{
			return { this };
		}
	};



	template<typename Error>
	class result<void, Error>
	{
	public:
		using promise_type = detail::result_promise<void, Error>;
		using value_type = void;
		using error_type = remove_cvref_t<Error>;


		result() = default;
		result(const result&) = default;
		result(result&& r) = default;
		result& operator=(const result&) = default;
		result& operator=(result&&) = default;

		result(const detail::OkTag<value_type>&) {}
		result(detail::ErrorTag<error_type>&& e) : mVar(e.mE) {}
		result(detail::ErrorMvTag<error_type>&& e) :mVar(std::move(e.mE)) {}


		optional<error_type> mVar;
		optional<error_type>& var() {
			return mVar;
		};
		const optional<error_type>& var() const {
			return mVar;
		};


		bool has_value() const {
			return !var();
		}

		bool has_error() const {
			return !has_value();
		}

		explicit operator bool() {
			return has_value();
		}

		value_type value() const
		{
			if (has_error())
				result_rethrow(error());
		}

		value_type operator*() const { return value(); }

		error_type& error()
		{
			if (has_value())
				throw std::runtime_error("error() was called on a Result<T,E> which stores an value_type");

			return *var();
		}

		const error_type& error() const
		{
			if (has_value())
				throw std::runtime_error("error() was called on a Result<T,E> which stores an value_type");

			return *var();
		}

		error_type error_or(error_type&& alt)
		{
			if (has_error())
				return error();
			return alt;
		}

		error_type& error_or(error_type& alt)
		{
			if (has_error())
				return error();
			return alt;
		}

		const error_type& error_or(const error_type& alt) const
		{
			if (has_error())
				return error();
			return alt;
		}

		void operator=(const detail::OkTag<value_type>& v)
		{
			var() = {};
		}

		void operator=(detail::ErrorMvTag<error_type>&& v)
		{
			var() = std::move(v.mE);
		}
		void operator=(detail::ErrorTag<error_type>&& v)
		{
			var() = v.mE;
		}


		bool operator==(const detail::OkTag<void>& v) const
		{
			return has_value();
		}

		template<typename E2>
		bool operator==(detail::ErrorTag<E2>&& v)const
		{
			return (has_error() && error() == v.mE);
		}
		template<typename E2>
		bool operator==(detail::ErrorMvTag<E2>&& v)const
		{
			return (has_error() && error() == v.mE);
		}

		bool operator!=(const detail::OkTag<void>& v)const
		{
			return !(*this == v);
		}


		template<typename E2>
		bool operator!=(detail::ErrorTag<E2>&& v)const
		{
			return !(*this == v);
		}
		template<typename E2>
		bool operator!=(detail::ErrorMvTag<E2>&& v)const
		{
			return !(*this == v);
		}

		detail::result_awaiter<void, Error> MACORO_OPERATOR_COAWAIT()
		{
			return { this };
		}
	};


}
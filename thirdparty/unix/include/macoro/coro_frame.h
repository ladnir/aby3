#pragma once
#include <cstdint>
#include "type_traits.h"
#include <typeinfo>

#include "optional.h"
#include <cassert>

#include "macoro/config.h"
#include "macoro/coroutine_handle.h"
#include <array>
#include <vector>
#include <cstddef>
#include <memory>
#ifdef MACORO_CPP_20
#include <coroutine>
#endif

namespace macoro
{
#define MACORO_INITIAL_SUSPEND_BEGIN_IDX 4294967295
#define MACORO_INITIAL_SUSPEND_IDX 4294967294
#define MACORO_FINAL_SUSPEND_IDX 4294967293

#ifdef __llvm__
#define MACORO_SUSPEND_POINT_ATTRIBUTE  __attribute__((enum_extensibility(open), flag_enum))
#else
#define MACORO_SUSPEND_POINT_ATTRIBUTE  
#endif

	enum class MACORO_SUSPEND_POINT_ATTRIBUTE SuspensionPoint : std::size_t
	{
		InitialSuspendBegin = MACORO_INITIAL_SUSPEND_BEGIN_IDX,
		InitialSuspend = MACORO_INITIAL_SUSPEND_IDX,
		FinalSuspend = MACORO_FINAL_SUSPEND_IDX
	};

	template<typename PromiseType = void>
	struct FrameBase;

	template<typename handle>
	struct await_suspend_t
	{

		handle value;
		bool suspend;

		explicit operator bool() const
		{
			return suspend;
		}

		auto get_handle()
		{
			assert(value);
			return value;
		}

		auto return_value() { return get_handle(); }
	};

	template<>
	struct await_suspend_t<bool>
	{
		bool value;

		explicit operator bool() const
		{
			return value;
		}
		coroutine_handle<> get_handle()
		{
			return noop_coroutine();
		}

		auto return_value() { return value; }

	};

	template<bool macoro, typename T>
	enable_if_t<macoro, coroutine_handle<T>>
		implicit_cast(const coroutine_handle<T>& h)
	{
		return h;
	}

#ifdef MACORO_CPP_20
	template<bool macoro, typename T>
	enable_if_t<!macoro, std::coroutine_handle<void>>
		implicit_cast(const coroutine_handle<T>& h)
	{
		return h.std_cast();
	}
#endif

	template<typename Awaiter, typename T>
	await_suspend_t<bool> await_suspend(Awaiter& a, coroutine_handle<T> h,
		enable_if_t<
		has_void_await_suspend<Awaiter, coroutine_handle<T>>::value 
#ifdef MACORO_CPP_20
		||
		has_void_await_suspend<Awaiter, std::coroutine_handle<void>>::value
#endif
		, empty_state> = {}) noexcept(noexcept(a.await_suspend(h)))
	{
		auto h2 = implicit_cast<has_void_await_suspend<Awaiter, coroutine_handle<T>>::value, T>(h);
		a.await_suspend(h2);
		return { true };
	}

	template<typename Awaiter, typename T>
	await_suspend_t<bool> await_suspend(Awaiter& a, coroutine_handle<T> h,
		enable_if_t<
		has_bool_await_suspend<Awaiter, coroutine_handle<T>>::value 
#ifdef MACORO_CPP_20
		||
		has_bool_await_suspend<Awaiter, std::coroutine_handle<void>>::value
#endif
		, empty_state> = {}) noexcept(noexcept(await_suspend_t<bool>{ a.await_suspend(h) }))
	{

		auto h2 = implicit_cast<has_bool_await_suspend<Awaiter, coroutine_handle<T>>::value, T>(h);
		return { a.await_suspend(h2) };
	}


	template<typename Awaiter, typename T>
	constexpr bool noexcept_await_suspend()
	{

		if constexpr (requires(Awaiter a, coroutine_handle<T> h) { a.await_suspend(h); })
		{
			return noexcept(std::declval<Awaiter>().await_suspend(coroutine_handle<T>{}));
		}
		else
		{
			return noexcept(std::declval<Awaiter>().await_suspend(std::coroutine_handle<void>{}));
		}
	}

	template<typename Awaiter, typename T>
	auto await_suspend(Awaiter& a, coroutine_handle<T> h,
		enable_if_t<
		!has_void_await_suspend<Awaiter, coroutine_handle<T>>::value &&
		!has_bool_await_suspend<Awaiter, coroutine_handle<T>>::value 
#ifdef MACORO_CPP_20
		&&
		!has_void_await_suspend<Awaiter, std::coroutine_handle<void>>::value &&
		!has_bool_await_suspend<Awaiter, std::coroutine_handle<void>>::value
#endif
		, empty_state> = {}) noexcept(noexcept_await_suspend<Awaiter, T>())
	{

		//auto h2 = ;
		using H2 = decltype(implicit_cast<has_any_await_suspend<Awaiter, coroutine_handle<T>>::value, T>(h));

		static_assert(
			std::is_same<decltype(a.await_suspend(H2{})), bool > ::value ||
			std::is_convertible<decltype(a.await_suspend(H2{})), macoro::coroutine_handle<>>::value
#ifdef MACORO_CPP_20
			||
			std::is_convertible<decltype(a.await_suspend(H2{})), std::coroutine_handle<>>::value
#endif
			,
			"await_suspend() must return 'void', 'bool', or convertible to macoro::coroutine_handle<> or std::coroutine_handle<>.");

		if constexpr (requires(Awaiter a) { a.await_suspend(h); })
		{
			using promise_type = typename coroutine_handle_traits<decltype(a.await_suspend(h))>::promise_type;
			auto ret = a.await_suspend(h);
			return await_suspend_t<coroutine_handle<promise_type>>{ ret, h != ret  };
		}
		else
		{
			auto hstd = h.std_cast();
			auto ret = a.await_suspend(hstd);
			return await_suspend_t<coroutine_handle<void>>{ coroutine_handle<void>(ret), hstd != ret  };
		}
	}

	// Intended to be equivalent to auto&&. When given
	// an lvalue it works as a reference. For an xvalue
	// (i.e. a non-reference return from a function (T) 
	// or a move type(T&&)), then it will store the actual
	// expression result.
	template<typename T>
	struct ExpressionStorage
	{
		static_assert(std::is_reference<T>::value == false, "");
		using type = T;
		using ST = typename std::aligned_storage<sizeof(T), alignof(T)>::type;
		using PT = typename std::remove_reference<T>::type;
		using Constructor = T;

		// the storage location of the T
		ST st;

		// a pointer to where the T was inplace-new'ed at.
		void* ptr = nullptr;

		// return the start of our inline storage.
		void* v() { return &st; }

		// get the constructed object.
		T&& get() { 
			assert(ptr);
			return (T&&)*(T*)ptr; 
		}

		// get a reference to the constructed object.
		typename std::remove_reference<T>::type& getRef()
		{
			assert(ptr);
			return *(T*)ptr;
		}

		// destroy the object
		void destroy()
		{
			if(ptr)
				((type*)ptr)->~type();
		}
	};


	template<typename T>
	struct ExpressionStorage<T&>
	{
		struct reference_wrapper
		{
			T* p;
			reference_wrapper() = default;
			reference_wrapper(T& r) : p(&r) {}
			reference_wrapper(T&& r) : p(&r) {}
		};


		using type = T;
		using PT = typename std::remove_reference<T>::type;
		using Constructor = reference_wrapper;

		Constructor st;
		void* ptr = nullptr;
		void* v() { return &st; }
		T& get() { return (T&)*st.p; }
		T& getRef() { return (T&)*st.p; }
		void destroy() { }
	};

	template<typename T>
	struct ExpressionStorage<T&&> : public ExpressionStorage<T&>
	{
		T&& get() { return (T&&)*this->st.p; }
	};



	template<typename Promise, size_t idx, typename Expr>
	struct AwaitContext
	{
		using promise_type = Promise;
		using expression_type = Expr;
		static const size_t suspend_index = idx;
		using awaitable_type = decltype(get_awaitable(std::declval<promise_type&>(), std::declval<ExpressionStorage<expression_type>>().get()));
		using awaiter_type = decltype(get_awaiter(std::declval<awaitable_type>()));

		ExpressionStorage<expression_type> expr;
		ExpressionStorage<awaitable_type> awaitable;
		ExpressionStorage<awaiter_type> awaiter;

		using expr_constructor = typename ExpressionStorage<expression_type>::Constructor;
		using awaitable_constructor = typename ExpressionStorage<awaitable_type>::Constructor;
		using awaiter_constructor = typename ExpressionStorage<awaiter_type>::Constructor;

		void** awaiter_ptr;
	};

	//template<typename T>
	//std::true_type is_lvalue(T&) { return {}; }
	//template<typename T>
	//std::false_type is_lvalue(T&&) { return {}; }


	template<typename Ref, typename Dec>
	struct store_as
	{
		using type = std::conditional_t<std::is_lvalue_reference<Ref>::value && !std::is_lvalue_reference<Dec>::value
			,
			Ref,
			Dec
		>;
	};

	template<typename Ref, typename Dec>
	using store_as_t = typename store_as<Ref, Dec>::type;

	template<typename T>
	decltype(auto) as_reference(T&& mTask)
	{
		return static_cast<decltype(mTask)>(mTask);
	}

	template<>
	struct FrameBase<void>
	{
		FrameBase() = default;
		FrameBase(const FrameBase<void>&) = delete;
		FrameBase(FrameBase<void>&&) = delete;

		// The current suspend location of the coroutines
		SuspensionPoint _suspension_idx_ = SuspensionPoint::InitialSuspendBegin;

		bool done() const
		{
			return _suspension_idx_ == SuspensionPoint::FinalSuspend;
		}
#ifdef MACORO_CPP_20
		std::coroutine_handle<void>(*get_std_handle)(FrameBase<void>* ptr) = nullptr;
#endif
		coroutine_handle<void>(*resume)(FrameBase<void>* ptr) = nullptr;
		void (*destroy)(FrameBase<void>* ptr) noexcept = nullptr;
	};


	// The object which allows lambda based coroutines
	// to set their current suspend point, and return a
	// value. This base object has to be independent from
	// the lambda function so we can pass it in as a parameter.
	template<typename PromiseType>
	struct FrameBase
		: public FrameBase<void>
	{
		using promise_type = PromiseType;



		promise_type promise;


		bool _initial_suspend_await_resumed_called_ = false;


		struct awaiters
		{
			size_t suspend_index;
#ifndef NDEBUG
			const std::type_info* _awaiter_typeid_ = nullptr;
#endif
			// The deleter for the current awaitable.
			void (*_awaiter_deleter)(void* ptr) = nullptr;
			void* _ctx_ptr = nullptr;

			// A pointer to a pointer that points to the awaiter
			void* _awaiter_ptr = nullptr;
		};
		std::vector<awaiters> awaiters;


		~FrameBase()
		{
			destroyAwaiters();
		}

		void destroyAwaiters()
		{
			while (awaiters.size())
			{
				awaiters.back()._awaiter_deleter(awaiters.back()._ctx_ptr);
				awaiters.pop_back();
			}
		}


		template<typename CTX>
		auto& makeAwaitContext()
		{

#ifndef NDEBUG
			for (auto& aa : awaiters)
				assert(aa.suspend_index != CTX::suspend_index);
#endif

			awaiters.emplace_back();
			auto& d = awaiters.back();
#ifndef NDEBUG
			d._awaiter_typeid_ = &typeid(CTX);
#endif
			d.suspend_index = CTX::suspend_index;

			auto ctx = new CTX;
			d._ctx_ptr = ctx;

			d._awaiter_deleter = [](void* ptr)
			{
				auto p = (CTX*)(ptr);
				p->awaiter.destroy();
				p->awaitable.destroy();
				p->expr.destroy();
				delete p;
			};

			ctx->awaiter_ptr = &d._awaiter_ptr;

			return *ctx;
		}



		template<typename CTX>
		auto& getAwaiter()
		{

			assert(awaiters.size());
			auto d = --awaiters.end();
			while (d->suspend_index != CTX::suspend_index)
			{
				assert(d != awaiters.begin());
				--d;
			}
			assert(d->suspend_index == CTX::suspend_index);
			assert(d->_awaiter_ptr);
			assert(d->_awaiter_typeid_ == &typeid(CTX));

			auto ptr = (typename std::remove_reference<typename CTX::awaiter_type>::type*)d->_awaiter_ptr;
			return *ptr;
		}

		template<typename CTX>
		auto destroyAwaiter()
		{
			assert(awaiters.size());
			auto d = --awaiters.end();
			assert(d->suspend_index == CTX::suspend_index);
			assert(d->_awaiter_ptr);
			assert(d->_awaiter_typeid_ == &typeid(CTX));
			auto p = (CTX*)d->_ctx_ptr;
			p->awaiter.destroy();
			p->awaitable.destroy();
			p->expr.destroy();
			delete p;

			awaiters.pop_back();
		}

#ifdef MACORO_CPP_20

		//using inline_storage_adapter = std::aligned_storage_t<400, alignof(std::max_align_t)>;
		//inline_storage_adapter frame_adapter_storage;

		//using outter_promise_type = promise_type;
		//struct std_handle_adapter
		//{
		//	//struct yield_awaiter
		//	//{
		//	//	std::coroutine_handle<void> continuation;

		//	//	bool await_ready() noexcept { return false; }
		//	//	std::coroutine_handle<void> await_suspend(std::coroutine_handle<void> v) noexcept { return continuation; }
		//	//	void await_resume() noexcept {}

		//	//};

		//	struct promise_type
		//	{
		//		suspend_always initial_suspend() noexcept { return {}; }
		//		suspend_always final_suspend() const noexcept { return {}; }

		//		void unhandled_exception()
		//		{
		//			std::rethrow_exception(std::current_exception());
		//		}

		//		bool mDone = false;
		//		coroutine_handle<outter_promise_type> outer_handle;


		//		std_handle_adapter get_return_object() { return { this }; }

		//		~promise_type()
		//		{
		//			if (outer_handle)
		//				outer_handle.destroy();
		//		}

		//		void* operator new(std::size_t size)
		//		{
		//			void* ptr = new char[size];
		//			if (!ptr) throw std::bad_alloc{};
		//			return ptr;
		//		}

		//		template<typename TT>
		//		void* operator new(std::size_t size, TT& base)
		//		{
		//			if (size <= sizeof(inline_storage_adapter))
		//			{
		//				return &base.frame_adapter_storage;
		//			}
		//			else
		//			{
		//				void* ptr = new char[size];
		//				if (!ptr) throw std::bad_alloc{};
		//				return ptr;
		//			}
		//		}

		//		void operator delete(void* ptr, std::size_t size)
		//		{

		//			if (size <= sizeof(inline_storage_adapter))
		//			{
		//				//delete base.frame_adapter_storage;
		//			}
		//			else
		//			{
		//				delete[](char*)ptr;
		//			}
		//		}

		//		void return_void() {}

		//	};


		//	~std_handle_adapter()
		//	{
		//		std::coroutine_handle<promise_type>::from_promise(*promise).destroy();
		//	}

		//	promise_type* promise;
		//};

		//using inline_storage_adapter = std::aligned_storage_t<400, alignof(std::max_align_t)>;
		//inline_storage_adapter frame_adapter_storage;
		std::shared_ptr<char> frame_adapter_storage;

		using outter_promise_type = promise_type;
		struct std_handle_adapter
		{

			struct promise_type
			{
				suspend_always initial_suspend() noexcept { return {}; }
				suspend_always final_suspend() const noexcept { return {}; }

				void unhandled_exception()
				{
					std::rethrow_exception(std::current_exception());
				}

				bool mDone = false;
				coroutine_handle<outter_promise_type> outer_handle;


				std_handle_adapter get_return_object() { return { this }; }

				~promise_type()
				{
					if (outer_handle)
						outer_handle.destroy();
				}

				template<typename TT>
				void* operator new(std::size_t size, TT& base)
				{
					assert(base.frame_adapter_storage == nullptr);
					base.frame_adapter_storage =
						std::shared_ptr<char>(new char[size], std::default_delete<char[]>());
					return base.frame_adapter_storage.get();
				}

				void operator delete(void* ptr, std::size_t size)
				{
				}

				void return_void() {}

			};


			~std_handle_adapter()
			{
				std::coroutine_handle<promise_type>::from_promise(*promise).destroy();
			}

			promise_type* promise;
		};



		std::coroutine_handle<typename std_handle_adapter::promise_type> std_handle;
		std::coroutine_handle<typename std_handle_adapter::promise_type> get_std_handle()
		{
			return std_handle;
		}

		static std::coroutine_handle<void> get_std_handle_void(FrameBase<void>* basev)
		{
			auto base = reinterpret_cast<FrameBase<promise_type>*>(basev);
			return base->get_std_handle();
		}
#endif

		void final_suspend()
		{
#ifdef MACORO_CPP_20
			std_handle.promise().mDone = true;
			std_handle.resume();
#endif
		}
	};


	// Frame is a Resumable which invokes a lambda each
	// time it is resumed. This lambda takes as input a  
	// FrameBase<ReturnType>* which allows the lambda
	// to tell the runtime/Resumable if the protocol is done 
	// or should be suspended, etc. The return value is also set
	// via FrameBase<ReturnType>. The Resumable::resume_
	// function is implemented by ProtoResumeCRTP. resume_ will 
	// cal resumeHandle and we will in turn vall the lambda function
	// and pass in ourself (FrameBase<ReturnType>) as the parameter.
	template<typename LambdaType, typename Promise>
	struct Frame :
		public LambdaType,
		public FrameBase<Promise>
	{
		using LambdaType::operator();
		using lambda_type = LambdaType;
		using promise_type = Promise;

		using FrameBase<Promise>::promise;
		using FrameBase<Promise>::done;


		static coroutine_handle<void> resume_impl(FrameBase<void>* ptr)
		{
			auto This = static_cast<Frame<LambdaType, Promise>*>(ptr);
			return (*This)(static_cast<FrameBase<Promise>*>(This));
		}
		static_assert(static_cast<Frame<LambdaType, Promise>*>(nullptr) == nullptr,"");
		static void destroy_impl(FrameBase<void>* ptr) noexcept
		{
			auto self = static_cast<Frame<LambdaType, Promise>*>(ptr);
			delete self;
		}


#ifdef MACORO_CPP_20
		using FrameBase<Promise>::std_handle;
		using std_handle_adapter = typename FrameBase<Promise>::std_handle_adapter;
		std_handle_adapter std_adapter;
#endif
		Frame(const Frame&) = delete;
		Frame(Frame&&) = delete;

		Frame(LambdaType&& l)
			: LambdaType(std::forward<LambdaType>(l))
#ifdef MACORO_CPP_20
			, std_adapter(adapter())
#endif
		{
			FrameBase<void>::resume = &Frame::resume_impl;
			//FrameBase<void>::resume = &Frame::operator();
			FrameBase<void>::destroy = &Frame::destroy_impl;
#ifdef MACORO_CPP_20

			FrameBase<void>::get_std_handle = &FrameBase<promise_type>::get_std_handle_void;
			;
			auto outer_handle = coroutine_handle<promise_type>::from_promise(promise, coroutine_handle_type::macoro);
			std_adapter.promise->outer_handle = outer_handle;
			std_handle = std::coroutine_handle<typename std_handle_adapter::promise_type>::from_promise(*std_adapter.promise);
#endif
		}

#ifdef MACORO_CPP_20
		~Frame()
		{
			// For when std_adapter isn'mTask the one who called our destructor,
			// prevent it from calling it recursively by setting the handle
			// back to this frame to null.
			std_adapter.promise->outer_handle = std::nullptr_t{};
		}
	private:

		std_handle_adapter adapter()
		{
			using P = typename std_handle_adapter::promise_type;
			while (!std_handle.promise().mDone)
			{
				struct ThisAwaiter
				{
					Frame* frame;

					bool await_ready() { return false; }

					void await_suspend(std::coroutine_handle<P>)
					{
						auto ll = frame->frame_adapter_storage;
						// resume our own coro
						auto h = (*frame)(static_cast<FrameBase<promise_type>*>(frame));
						assert(h);
						h.resume();
					}
					//std::coroutine_handle<> await_suspend(std::coroutine_handle<P>)
					//{
						//// resume our own coro
						//auto h = (*frame)(static_cast<FrameBase<promise_type>*>(frame));
						//assert(h);

						//// Ideally we would perform sysmetric transfer here but there is
						//// a compiler bug in MSVC. Doing anothing but returning here may cause
						////  ASAN error on MSVC. See 
						//// https://developercommunity.visualstudio.com/mTask/c20-coroutine-resume-thread-safety/1668687?entry=myfeedback&space=62&ref=native&refTime=1645079575781&refUserId=36961f1e-1836-4f6a-af82-ffcfa64d75a3&viewtype=all
						//// In particular, the adpater might have already been destroyed
						//// and MSVC for some unknown reason updates the adapter frame even
						//// though the coro has already been suspended. 
						//return h.std_cast();

						//// we have to perform the symmetric transfer loop here for
						//// macoro coroutines since we can only return std::coroutine_handle<>.
						//while (h != noop_coroutine() && h.is_std() == false)
						//{
						//	auto realAddr = reinterpret_cast<FrameBase<void>*>((std::size_t)h.address() ^ 1);
						//	auto _address = realAddr->resume(realAddr).address();
						//	h = coroutine_handle<void>::from_address(_address);
						//}

						//// we either have a std coro or a noop. We can
						//// have the std perform symmetric transfer on that. 
						//return h.std_cast();
					//}

					void await_resume() {}
				};

				co_await ThisAwaiter{ this };


			}
		};
#endif

	};

	// makes a Proto from the given lambda. The lambda
	// should take as input a FrameBase<ReturnType>* 
	template<typename Promise, typename LambdaType>
	inline auto makeFrame(LambdaType&& l)
	{
		return new Frame<LambdaType, Promise>(std::forward<LambdaType>(l));
	}

#ifdef MACORO_CPP_20
	inline bool _macoro_coro_is_std(void* _address) noexcept
	{
		return !bool((std::size_t)_address & 1);
	}
#endif
	template<typename Promise>
	Promise* _macoro_coro_promise(void* _address) noexcept
	{
#ifdef MACORO_CPP_20
		if (!_macoro_coro_is_std(_address))
		{
			auto realAddr = reinterpret_cast<FrameBase<void>*>((std::size_t)_address ^ 1);
			auto ptrV = reinterpret_cast<FrameBase<void>*>(realAddr);
			auto ptr = static_cast<FrameBase<Promise>*>(ptrV);
			return &ptr->promise;
		}
		else
		{
			return &std::coroutine_handle<Promise>::from_address(_address).promise();
		}
#else
		auto ptrV = reinterpret_cast<FrameBase<void>*>(_address);
		auto ptr = static_cast<FrameBase<Promise>*>(ptrV);
		return &ptr->promise;
#endif
	}

	template<typename Promise>
	void* _macoro_coro_frame(Promise* promise, coroutine_handle_type te)noexcept
	{
#ifdef MACORO_CPP_20
		if (te == coroutine_handle_type::macoro)
		{
			std::size_t offset = ((std::size_t) & reinterpret_cast<char const volatile&>((((FrameBase<Promise>*)0)->promise)));
			auto frame = (FrameBase<Promise>*)((char*)promise - offset);
			auto base = static_cast<FrameBase<void>*>(frame);
			auto tag = (std::size_t)base;
			assert((tag & 1) == 0);
			tag ^= 1;
			base = reinterpret_cast<FrameBase<void>*>(tag);
			return base;
		}
		else
		{
			auto ret = std::coroutine_handle<Promise>::from_promise(*promise).address();
			assert(((std::size_t)ret & 1) == 0);
			return ret;
		}
#else
		std::size_t offset = ((std::size_t) & reinterpret_cast<char const volatile&>((((FrameBase<Promise>*)0)->promise)));
		auto frame = (FrameBase<Promise>*)((char*)promise - offset);
		auto base = static_cast<FrameBase<void>*>(frame);
		return base;
#endif
	}

	inline bool _macoro_coro_done(void* _address)noexcept
	{
#ifdef MACORO_CPP_20
		if (!_macoro_coro_is_std(_address))
		{
			auto realAddr = reinterpret_cast<FrameBase<void>*>((std::size_t)_address ^ 1);
			return realAddr->done();
		}
		return std::coroutine_handle<void>::from_address(_address).done();
#else
		auto realAddr = reinterpret_cast<FrameBase<void>*>((std::size_t)_address);
		return realAddr->done();
#endif
	}
	inline void coroutine_handle<void>::resume() const {
		auto _address = _Ptr;
		while (true)
		{
#ifdef MACORO_CPP_20

			if (!_macoro_coro_is_std(_address))
			{
				if (_address == noop_coroutine().address())
					return;
				auto realAddr = reinterpret_cast<FrameBase<void>*>((std::size_t)_address ^ 1);
				_address = realAddr->resume(realAddr).address();
				assert(_address);
			}
			else
			{
				std::coroutine_handle<void>::from_address(_address).resume();
				return;
			}
#else
			if (_address == noop_coroutine().address())
				return;
			auto realAddr = reinterpret_cast<FrameBase<void>*>((std::size_t)_address);
			_address = realAddr->resume(realAddr).address();
#endif
		}
		//_macoro_coro_resume(_Ptr);
	}
	template<typename T>
	inline void coroutine_handle<T>::resume() const {
		auto _address = _Ptr;
		while (true)
		{
#ifdef MACORO_CPP_20

			if (!_macoro_coro_is_std(_address))
			{
				if (_address == noop_coroutine().address())
					return;
				auto realAddr = reinterpret_cast<FrameBase<void>*>((std::size_t)_address ^ 1);
				_address = realAddr->resume(realAddr).address();
				assert(_address);
			}
			else
			{
				std::coroutine_handle<void>::from_address(_address).resume();
				return;
			}
#else
			if (_address == noop_coroutine().address())
				return;
			auto realAddr = reinterpret_cast<FrameBase<void>*>((std::size_t)_address);
			_address = realAddr->resume(realAddr).address();
#endif
		}
	}
//
//	inline void _macoro_coro_resume(void* _address)
//	{
//		while (true)
//		{
//#ifdef MACORO_CPP_20
//
//			if (!_macoro_coro_is_std(_address))
//			{
//				if (_address == noop_coroutine().address())
//					return;
//				auto realAddr = reinterpret_cast<FrameBase<void>*>((std::size_t)_address ^ 1);
//				_address = realAddr->resume(realAddr).address();
//				assert(_address);
//			}
//			else
//			{
//				std::coroutine_handle<void>::from_address(_address).resume();
//				return;
//			}
//#else
//			auto realAddr = reinterpret_cast<FrameBase<void>*>((std::size_t)_address);
//			_address = realAddr->resume(realAddr).address();
//			if (_address == noop_coroutine().address())
//				return;
//#endif
//		}
//	}

	inline void _macoro_coro_destroy(void* _address)noexcept
	{
#ifdef MACORO_CPP_20
		if (!_macoro_coro_is_std(_address))
		{
			auto realAddr = reinterpret_cast<FrameBase<void>*>((std::size_t)_address ^ 1);
			return realAddr->destroy(realAddr);
		}
		else
		{
			std::coroutine_handle<void>::from_address(_address).destroy();
		}
#else
		auto realAddr = reinterpret_cast<FrameBase<void>*>((std::size_t)_address);
		return realAddr->destroy(realAddr);
#endif
	}

#ifdef MACORO_CPP_20
	inline std::coroutine_handle<void> coroutine_handle<void>::std_cast() const
	{
		if (*this == noop_coroutine())
		{
			return std::noop_coroutine();
		}
		if (!is_std())
		{
			auto realAddr = reinterpret_cast<FrameBase<void>*>((std::size_t)_Ptr ^ 1);
			return realAddr->get_std_handle(realAddr);
		}
		else
		{
			return std::coroutine_handle<void>::from_address(_Ptr);
		}
	}

	template<class _Promise>
	inline std::coroutine_handle<void> coroutine_handle<_Promise>::std_cast() const
	{

		if (*this == noop_coroutine())
		{
			return std::noop_coroutine();
		}
		if (!_macoro_coro_is_std(_Ptr))
		{
			auto realAddr = reinterpret_cast<FrameBase<_Promise>*>((std::size_t)_Ptr ^ 1);
			return realAddr->get_std_handle();
		}
		else
		{
			return std::coroutine_handle<_Promise>::from_address(_Ptr);
		}
	}

#endif
}
#pragma once

#include "macoro/config.h"
#include "macoro/type_traits.h"
#include <functional>
#include <cassert>
#include <utility>
#ifdef MACORO_CPP_20
#include <coroutine>
#endif // MACORO_CPP_20

namespace macoro
{

#ifdef MACORO_CPP_20

    template<typename T>
    using coroutine_traits = std::coroutine_traits<T>;
#else
    // STRUCT TEMPLATE coroutine_traits
    template <class _Ret, class = void>
    struct _Coroutine_traits {};

    template <class _Ret>
    struct _Coroutine_traits<_Ret, void_t<typename _Ret::promise_type>> {
        using promise_type = typename _Ret::promise_type;
    };
    template <class _Ret, class...>
    struct coroutine_traits : _Coroutine_traits<_Ret> {};
#endif


    template<typename Promise = void>
    struct coroutine_handle;

    template<typename nested_promise>
    struct std_handle_adapter;


    template<typename T>
    struct coroutine_handle_traits
    {
    };

#ifdef MACORO_CPP_20
    template<typename T>
    struct coroutine_handle_traits<std::coroutine_handle<T>>
    {
        using promise_type = T;
        template<typename P = void>
        using coroutine_handle = std::coroutine_handle<P>;
    };
#endif

    template<typename T>
    struct coroutine_handle_traits<coroutine_handle<T>>
    {
        using promise_type = T;
        template<typename P = void>
        using coroutine_handle = coroutine_handle<P>;
    };

#ifdef MACORO_CPP_20
    static_assert(std::is_same<coroutine_handle_traits<std::coroutine_handle<int>>::promise_type, int>::value);
    static_assert(std::is_same<coroutine_handle_traits<std::coroutine_handle<int>>::template coroutine_handle<char>, std::coroutine_handle<char>>::value);
#endif
    static_assert(std::is_same<coroutine_handle_traits<coroutine_handle<int>>::promise_type, int>::value, "");
    static_assert(std::is_same<coroutine_handle_traits<coroutine_handle<int>>::template coroutine_handle<char>, coroutine_handle<char>>::value, "");


    enum class coroutine_handle_type
    {
        std,
        macoro
    };

#ifdef MACORO_CPP_20
    inline bool _macoro_coro_is_std(void* _address) noexcept;
#endif

    template<typename Promise>
    Promise* _macoro_coro_promise(void* address)noexcept;

    template<typename Promise>
    void* _macoro_coro_frame(void* address, coroutine_handle_type te) noexcept;

    bool _macoro_coro_done(void* address)noexcept;
    //void _macoro_coro_resume(void* address);
    void _macoro_coro_destroy(void* address)noexcept;


    template <>
    struct coroutine_handle<void> {
        constexpr coroutine_handle() noexcept = default;
        constexpr coroutine_handle(std::nullptr_t) noexcept {}

#ifdef MACORO_CPP_20
        template<typename T>
        explicit coroutine_handle(const std::coroutine_handle<T>& h) noexcept {
            _Ptr = h.address();
            assert(((std::size_t)_Ptr & 1) == 0);
        };
        template<typename T>
        explicit coroutine_handle(std::coroutine_handle<T>&& h) noexcept {
            _Ptr = h.address();
            assert(((std::size_t)_Ptr & 1) == 0);
        };
#endif // MACORO_CPP_20

        coroutine_handle& operator=(std::nullptr_t) noexcept {
            _Ptr = nullptr;
            return *this;
        }

        MACORO_NODISCARD constexpr void* address() const noexcept {
            return _Ptr;
        }

        MACORO_NODISCARD static constexpr coroutine_handle from_address(void* const _Addr) noexcept { // strengthened
            coroutine_handle _Result;
            _Result._Ptr = _Addr;
            return _Result;
        }

        constexpr explicit operator bool() const noexcept {
            return _Ptr != nullptr;
        }

        MACORO_NODISCARD bool done() const noexcept { // strengthened
            return _macoro_coro_done(_Ptr);
        }

        void operator()() const {
            resume();
        }

        void resume() const;
        void destroy() const noexcept { // strengthened
            _macoro_coro_destroy(_Ptr);
        }

#ifdef MACORO_CPP_20
        bool is_std() const
        {
            return _macoro_coro_is_std(_Ptr);
        }

        std::coroutine_handle<void> std_cast() const;

        explicit operator std::coroutine_handle<void>() const { return std_cast(); }
#endif // MACORO_CPP_20

    private:
        void* _Ptr = nullptr;
    };



    template <class _Promise>
    struct coroutine_handle {
        constexpr coroutine_handle() noexcept = default;
        constexpr coroutine_handle(std::nullptr_t) noexcept {}

#ifdef MACORO_CPP_20
        explicit coroutine_handle(const std::coroutine_handle<_Promise>& h) noexcept {
            _Ptr = h.address();
            assert(((std::size_t)_Ptr & 1) == 0);
        };
        explicit coroutine_handle(std::coroutine_handle<_Promise>&& h) noexcept {
            _Ptr = h.address();
            assert(((std::size_t)_Ptr & 1) == 0);
        };
#endif 

        coroutine_handle& operator=(std::nullptr_t) noexcept {
            _Ptr = nullptr;
            return *this;
        }

        MACORO_NODISCARD constexpr void* address() const noexcept {
            return _Ptr;
        }

        MACORO_NODISCARD static constexpr coroutine_handle from_address(void* const _Addr) noexcept { // strengthened
            coroutine_handle _Result;
            _Result._Ptr = _Addr;
            return _Result;
        }

        constexpr operator coroutine_handle<>() const noexcept {
            return coroutine_handle<>::from_address(_Ptr);
        }

        constexpr explicit operator bool() const noexcept {
            return _Ptr != nullptr;
        }

        MACORO_NODISCARD bool done() const noexcept { // strengthened
            return _macoro_coro_done(_Ptr);
        }

        void operator()() const {
            resume();
        }

        void resume() const;

        void destroy() const noexcept { // strengthened
            _macoro_coro_destroy(_Ptr);
        }

        MACORO_NODISCARD _Promise& promise() const noexcept { // strengthened
            return *_macoro_coro_promise<_Promise>(_Ptr);
        }

#ifdef MACORO_CPP_20
        std::coroutine_handle<void> std_cast() const;
        explicit operator std::coroutine_handle<void>() const { return std_cast(); }
#endif // MACORO_CPP_20


        MACORO_NODISCARD static coroutine_handle from_promise(_Promise& _Prom, coroutine_handle_type te) noexcept { // strengthened
            const auto _Frame_ptr = _macoro_coro_frame<_Promise>(std::addressof(_Prom), te);
            coroutine_handle _Result;
            _Result._Ptr = _Frame_ptr;
            return _Result;
        }

    private:
        void* _Ptr = nullptr;
    };

    MACORO_NODISCARD constexpr bool operator==(const coroutine_handle<> _Left, const coroutine_handle<> _Right) noexcept {
        return _Left.address() == _Right.address();
    }


    MACORO_NODISCARD constexpr bool operator!=(const coroutine_handle<> _Left, const coroutine_handle<> _Right) noexcept {
        return !(_Left == _Right);
    }

    MACORO_NODISCARD constexpr bool operator<(const coroutine_handle<> _Left, const coroutine_handle<> _Right) noexcept {
        return std::less<void*>{}(_Left.address(), _Right.address());
    }

    MACORO_NODISCARD constexpr bool operator>(const coroutine_handle<> _Left, const coroutine_handle<> _Right) noexcept {
        return _Right < _Left;
    }

    MACORO_NODISCARD constexpr bool operator<=(const coroutine_handle<> _Left, const coroutine_handle<> _Right) noexcept {
        return !(_Left > _Right);
    }

    MACORO_NODISCARD constexpr bool operator>=(const coroutine_handle<> _Left, const coroutine_handle<> _Right) noexcept {
        return !(_Left < _Right);
    }

    //template <class _Promise>
    //struct hash<coroutine_handle<_Promise>> {
    //    MACORO_NODISCARD size_t operator()(const coroutine_handle<_Promise>& _Coro) const noexcept {
    //        return _Hash_representation(_Coro.address());
    //    }
    //};

    // STRUCT noop_coroutine_promise
    struct noop_coroutine_promise {};

    // STRUCT coroutine_handle<noop_coroutine_promise>
    template <>
    struct coroutine_handle<noop_coroutine_promise> {
        friend coroutine_handle noop_coroutine() noexcept;

        constexpr operator coroutine_handle<>() const noexcept {
            //throw std::runtime_error("not implemented");
            //std::terminate();
            return coroutine_handle<>::from_address(_Ptr);
        }

        constexpr explicit operator bool() const noexcept {
            return true;
        }
        MACORO_NODISCARD constexpr bool done() const noexcept {
            return false;
        }

        constexpr void operator()() const noexcept {}
        constexpr void resume() const noexcept {}
        constexpr void destroy() const noexcept {}

        MACORO_NODISCARD noop_coroutine_promise& promise() const noexcept {
            // Returns a reference to the associated promise
            static noop_coroutine_promise prom;
            return prom;
        }

        MACORO_NODISCARD constexpr void* address() const noexcept {
            return _Ptr;
        }

#ifdef MACORO_CPP_20
        std::noop_coroutine_handle std_cast() const { return std::noop_coroutine(); }
        explicit operator std::coroutine_handle<void>() const { return std_cast(); }
        explicit operator std::noop_coroutine_handle() const { return std_cast(); }
#endif // MACORO_CPP_20

    private:
        coroutine_handle() noexcept = default;

        void * const _Ptr = (void*)43;
    };

    // ALIAS noop_coroutine_handle
    using noop_coroutine_handle = coroutine_handle<noop_coroutine_promise>;

    // FUNCTION noop_coroutine
    MACORO_NODISCARD inline noop_coroutine_handle noop_coroutine() noexcept {
        // Returns a handle to a coroutine that has no observable effects when resumed or destroyed.
        return noop_coroutine_handle{};
    }

    // STRUCT suspend_never
    struct suspend_never {
        MACORO_NODISCARD constexpr bool await_ready() const noexcept {
            return true;
        }
        constexpr void await_suspend(coroutine_handle<>) const noexcept {}
        constexpr void await_resume() const noexcept {}

#ifdef MACORO_CPP_20
        
        constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}
#endif
    };

    // STRUCT suspend_always
    struct suspend_always {
        MACORO_NODISCARD constexpr bool await_ready() const noexcept {
            return false;
        }
        constexpr void await_suspend(coroutine_handle<>) const noexcept {}
        constexpr void await_resume() const noexcept {}

#ifdef MACORO_CPP_20
        
        constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}
#endif
    };

}

#include "coro_frame.h"
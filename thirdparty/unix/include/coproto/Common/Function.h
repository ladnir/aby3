#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "function2/function2.hpp"

namespace coproto
{
    /// An owning copyable function wrapper for arbitrary callable types.
    template <typename... Signatures>
    using function = fu2::function<Signatures...>;

    /// An owning non copyable function wrapper for arbitrary callable types.
    template <typename... Signatures>
    using unique_function = fu2::unique_function<Signatures...>;

    /// A non owning copyable function wrapper for arbitrary callable types.
    template <typename... Signatures>
    using function_view = fu2::function_view<Signatures...>;
}

// namespace coproto
// {
    // template<typename T>
    // using function = std::function<T>;

    // template<typename T>
    // class unique_function : public std::function<T>
    // {
    //     template<typename Fn, typename En = void>
    //     struct wrapper;

    //     // specialization for CopyConstructible Fn
    //     template<typename Fn>
    //     struct wrapper<Fn, std::enable_if_t< std::is_copy_constructible<Fn>::value >>
    //     {
    //         Fn fn;

    //         template<typename... Args>
    //         auto operator()(Args&&... args) { return fn(std::forward<Args>(args)...); }
    //     };

    //     // specialization for MoveConstructible-only Fn
    //     template<typename Fn>
    //     struct wrapper<Fn,typename std::enable_if<
    //         !  std::is_copy_constructible<Fn>::value
    //         && std::is_move_constructible<Fn>::value >::type>
    //     {
    //         Fn fn;

    //         wrapper(Fn&& fn) : fn(std::forward<Fn>(fn)) { }

    //         wrapper(wrapper&&) = default;
    //         wrapper& operator=(wrapper&&) = default;

    //         // these two functions are instantiated by std::function
    //         // and are never called
    //         wrapper(const wrapper& rhs) : fn(const_cast<Fn&&>(rhs.fn)) { throw 0; } // hack to initialize fn for non-DefaultContructible types
    //         wrapper& operator=(wrapper&) { throw 0; }

    //         template<typename... Args>
    //         auto operator()(Args&&... args) { return fn(std::forward<Args>(args)...); }
    //     };

    //     using base = std::function<T>;

    // public:
    //     unique_function() noexcept = default;
    //     unique_function(std::nullptr_t) noexcept : base(nullptr) { }

    //     template<typename Fn>
    //     unique_function(Fn&& f) : base(wrapper<Fn>{ std::forward<Fn>(f) }) { }

    //     unique_function(unique_function&& f) 
    //         : base(std::move(static_cast<base&>(f)))
    //     {}
    //     unique_function& operator=(unique_function&& f)
    //     {
    //         static_cast<base&>(*this) = std::move(static_cast<base&>(f));
    //         return *this;
    //     }

    //     unique_function& operator=(std::nullptr_t) { base::operator=(nullptr); return *this; }

    //     template<typename Fn>
    //     unique_function& operator=(Fn&& f)
    //     {
    //         base::operator=(wrapper<Fn>{ std::forward<Fn>(f) }); return *this;
    //     }

    //     using base::operator();
    // };

// }


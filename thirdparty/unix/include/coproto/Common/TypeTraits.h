#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include <type_traits>

namespace coproto
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

	template<typename T, typename = void>
	struct has_size_type : false_type
	{};

	template <typename T>
	struct has_size_type <T, void_t<typename T::size_type>>
		: true_type {};


	template<typename T, typename = void>
	struct has_value_type : false_type
	{};

	template <typename T>
	struct has_value_type <T, void_t<typename T::value_type>>
		: true_type {};


	template<typename T, typename = void>
	struct has_size_member_func : false_type
	{};

	template <typename T>
	struct has_size_member_func <T, void_t<
		// must have size type
		typename T::size_type,

		// must have a size() member fn
		decltype(std::declval<T>().size()),

		// must return size_type
		enable_if_t<
		std::is_same<
		decltype(std::declval<T>().size()),
		typename T::size_type
		>::value
		>
		>>
		: true_type{};


	template<typename T, typename = void>
	struct has_resize_member_func : false_type
	{};

	template <typename T>
	struct has_resize_member_func <T, void_t<
		// must have size type
		typename T::size_type,

		// must have a resize(size_type) member fn
		decltype(std::declval<T>().resize(std::declval<typename T::size_type>())),

		// must return void
		enable_if_t<
		std::is_same<
		decltype(std::declval<T>().resize(std::declval<typename T::size_type>())),
		void
		>::value
		>
		>>
		: true_type{};



	template<typename T, typename = void>
	struct has_data_member_func : false_type
	{};

	template <typename T>
	struct has_data_member_func <T, void_t<
		// must have value_type
		typename T::value_type,

		// must have a data() member fn
		decltype(std::declval<T>().data()),

		// must return value_type* or const value_type*
		enable_if_t<
		std::is_same<
		decltype(std::declval<T>().data()),
		typename T::value_type*
		>::value 
		||
		// pre CPP 17 std::string returns a const pointer. So we
		// will allow this case.
		std::is_same<
		decltype(std::declval<T>().data()),
		const typename T::value_type*
		>::value
		>
		>>
		: true_type{};


	template<typename T, typename = void>
	struct has_trvial_value_type : false_type
	{};

	template <typename T>
	struct has_trvial_value_type <T, void_t<
		// must have value_type
		typename T::value_type,

		// must be trivial
		enable_if_t<
		std::is_trivial<
		typename T::value_type
		>::value
		>
		>>
		: true_type{};


	template<class Container, typename = void>
	struct is_trivial_container : false_type
	{};


	template<class Container>
	struct is_trivial_container < Container, void_t <
		enable_if_t<has_data_member_func<Container>::value>,
		enable_if_t<has_size_member_func<Container>::value>,
		enable_if_t<has_trvial_value_type<Container>::value>,
		enable_if_t<std::is_trivial<Container>::value == false>
		>> :
		true_type {};
		 

	template<class Container, typename = void>
	struct is_resizable_trivial_container : false_type
	{};


	template<class Container>
	struct is_resizable_trivial_container < Container, void_t <
		enable_if_t<is_trivial_container<Container>::value>,
		enable_if_t<has_resize_member_func<Container>::value>
		>> :
		true_type {};


	template<typename /* _ */, typename I, typename U, typename... Args>
	struct is_poly_emplaceable_ : false_type
	{};


	template<typename I, typename U, typename... Args>
	struct is_poly_emplaceable_<void_t<
		enable_if_t<std::is_base_of<I, U>::value>,
		enable_if_t<std::is_constructible<U, Args...>::value>
	>, I, U, Args...> : true_type
	{};

	template <typename C, typename... Args>
	using is_poly_emplaceable = is_poly_emplaceable_<void, C, Args...>;


	template<typename T, typename = void>
	struct CallDestructor
	{
		CallDestructor(T&) {}
	};

	template<typename T>
	struct CallDestructor<T, void_t<
		enable_if_t<std::is_trivially_destructible<T>::value == false>
		>>
	{
		CallDestructor(T& t) {
			t.~T();
		}
	};


}
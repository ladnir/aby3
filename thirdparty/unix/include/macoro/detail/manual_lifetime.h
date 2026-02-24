///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <type_traits>
#include <memory>

namespace macoro
{
	namespace detail {
		template<typename T>
		struct manual_lifetime
		{
		public:
			manual_lifetime() noexcept {}
			~manual_lifetime() noexcept {}

			manual_lifetime(const manual_lifetime&) = delete;
			manual_lifetime(manual_lifetime&&) = delete;
			manual_lifetime& operator=(const manual_lifetime&) = delete;
			manual_lifetime& operator=(manual_lifetime&&) = delete;

			template<typename... Args>
			enable_if_t<std::is_constructible<T, Args&&...>::value> construct(Args&&... args)
				noexcept(std::is_nothrow_constructible<T, Args&&...>::value)
			{
				::new (static_cast<void*>(std::addressof(m_value))) T(static_cast<Args&&>(args)...);
			}

			void destruct() noexcept(std::is_nothrow_destructible<T>::value)
			{
				m_value.~T();
			}

			std::add_pointer_t<T> operator->() noexcept { return std::addressof(**this); }
			std::add_pointer_t<const T> operator->() const noexcept { return std::addressof(**this); }

			T& operator*() & noexcept { return m_value; }
			const T& operator*() const& noexcept { return m_value; }
			T&& operator*() && noexcept { return static_cast<T&&>(m_value); }
			const T&& operator*() const&& noexcept { return static_cast<const T&&>(m_value); }

		private:
			union {
				T m_value;
			};
		};

		template<typename T>
		struct manual_lifetime<T&>
		{
		public:
			manual_lifetime() noexcept {}
			~manual_lifetime() noexcept {}

			manual_lifetime(const manual_lifetime&) = delete;
			manual_lifetime(manual_lifetime&&) = delete;
			manual_lifetime& operator=(const manual_lifetime&) = delete;
			manual_lifetime& operator=(manual_lifetime&&) = delete;

			void construct(T& value) noexcept
			{
				m_value = std::addressof(value);
			}

			void destruct() noexcept {}

			T* operator->() noexcept { return m_value; }
			const T* operator->() const noexcept { return m_value; }

			T& operator*() noexcept { return *m_value; }
			const T& operator*() const noexcept { return *m_value; }

		private:
			T* m_value;
		};

		template<typename T>
		struct manual_lifetime<T&&>
		{
		public:
			manual_lifetime() noexcept {}
			~manual_lifetime() noexcept {}

			manual_lifetime(const manual_lifetime&) = delete;
			manual_lifetime(manual_lifetime&&) = delete;
			manual_lifetime& operator=(const manual_lifetime&) = delete;
			manual_lifetime& operator=(manual_lifetime&&) = delete;

			void construct(T&& value) noexcept
			{
				m_value = std::addressof(value);
			}

			void destruct() noexcept {}

			T* operator->() noexcept { return m_value; }
			const T* operator->() const noexcept { return m_value; }

			T& operator*() & noexcept { return *m_value; }
			const T& operator*() const& noexcept { return *m_value; }
			T&& operator*() && noexcept { return static_cast<T&&>(*m_value); }
			const T&& operator*() const&& noexcept { return static_cast<const T&&>(*m_value); }

		private:
			T* m_value;
		};

		template<>
		struct manual_lifetime<void>
		{
			void construct() noexcept {}
			void destruct() noexcept {}
			void operator*() const noexcept {}
		};
	}
}


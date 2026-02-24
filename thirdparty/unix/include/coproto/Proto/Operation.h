#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "coproto/Common/Defines.h"
#include "coproto/Common/TypeTraits.h"
#include "coproto/Common/span.h"
#include "coproto/Common/InlinePoly.h"
#include "coproto/Common/error_code.h"
#include "coproto/Common/Function.h"

namespace coproto
{
	namespace internal
	{

		struct AnyNoCopy
		{
			using Storage = std::aligned_storage_t<256, alignof(std::max_align_t)>;
			Storage mStorage;
			using Deleter = void (*)(AnyNoCopy*);
			Deleter deleter = nullptr;

			AnyNoCopy() = default;
			AnyNoCopy(const AnyNoCopy&) = delete;
			AnyNoCopy(AnyNoCopy&&) = delete;
			~AnyNoCopy()
			{
				if (deleter)
					(*deleter)(this);
			}
			template<typename T>
			enable_if_t<sizeof(T) <= sizeof(Storage), T*>
				emplace(T&& t)
			{
				if (deleter)
					deleter(this);
				using type = typename std::remove_reference<T>::type;
				auto r = new (&mStorage) type(std::forward<T>(t));

				deleter = getDeleter<type>();
				return r;
			}
			template<typename T>
			enable_if_t<(sizeof(T) > sizeof(Storage)), T*>
				emplace(T&& t)
			{
				if (deleter)
					deleter(this);
				using type = typename std::remove_reference<T>::type;
				auto& ptr = *(type**)&mStorage;
				ptr = new type(std::forward<T>(t));
				deleter = getDeleter<type>();
				return ptr;
			}

			template<typename T>
			static enable_if_t<
				std::is_trivially_destructible<T>::value &&
				sizeof(T) <= sizeof(Storage)
				, Deleter> getDeleter()
			{
				return nullptr;
			}

			template<typename T>
			static enable_if_t<
				std::is_trivially_destructible<T>::value == false &&
				sizeof(T) <= sizeof(Storage)
				, Deleter> getDeleter()
			{
				return [](AnyNoCopy* This)
				{
					((T*)&This->mStorage)->~T();
				};
			}

			template<typename T>
			static enable_if_t<(sizeof(T) > sizeof(Storage))
				, Deleter> getDeleter()
			{
				return [](AnyNoCopy* This)
				{
					auto t = *(T**)&This->mStorage;
					delete t;
				};
			}
		};

		struct SendOp
		{
			virtual ~SendOp() {}
			virtual span<u8> asSpan() = 0;

			virtual void setError(std::exception_ptr e) = 0;

		};

		template<typename Container>
		enable_if_t<has_size_member_func<Container>::value, u64>
			u8Size(Container& cont)
		{
			return cont.size() * sizeof(typename Container::value_type);
		}

		template<typename Container>
		enable_if_t<
			!has_size_member_func<Container>::value&&
			std::is_trivial<Container>::value, u64>
			u8Size(Container& cont)
		{
			return sizeof(Container);
		}

		template<typename Container>
		enable_if_t<is_trivial_container<Container>::value, span<u8>>
			asSpan(Container& container)
		{
			return span<u8>((u8*)container.data(), u8Size(container));
		}

		template<typename ValueType>
		enable_if_t<std::is_trivial<ValueType>::value, span<u8>>
			asSpan(ValueType& container)
		{
			return span<u8>((u8*)&container, u8Size(container));
		}

		template<typename OTHER>
		enable_if_t<std::is_trivial<OTHER>::value == false && 
			is_trivial_container<OTHER>::value == false, span<u8>>
			asSpan(OTHER& container)
		{
			static_assert(
				std::is_trivial<OTHER>::value ||
				is_trivial_container<OTHER>::value,
				"Coproto does not know how to send & receiver your type. Coproto can send "
				"type T that satisfies \n\n\tstd::is_trivial<T>::value == true\n\tcoproto::is_trivial_container<T>::value == true\n\n"
				"types like int, char, u8 are trivial. Types like std::vector<int> are trivial container. The container must look "
				"like a vector. For a complete specification of coproto::is_trivial_container, see coproto/Common/TypeTraits.h");
		}

		template<typename Container>
		enable_if_t<is_resizable_trivial_container<Container>::value, span<u8>>
			tryResize(u64 size_, Container& container, bool allowResize)
		{
			if (allowResize && (size_ % sizeof(typename Container::value_type)) == 0)
			{
				auto s = size_ / sizeof(typename Container::value_type);
				try {
					container.resize(s);
				}
				catch (...)
				{
					return {};
				}
			}
			return asSpan(container);
		}

		template<typename Container>
		enable_if_t<!is_resizable_trivial_container<Container>::value, span<u8>>
			tryResize(u64, Container& container, bool)
		{
			return asSpan(container);
		}


		struct SendBuffer
		{
			InlinePoly<SendOp, sizeof(u64) * 8> mStorage;

			void setError(std::exception_ptr e) {
				mStorage->setError(e);
			}

			span<u8> asSpan() {
				return mStorage->asSpan();
			}
		};


		struct RecvBuffer
		{
			std::exception_ptr mExPtr;
			void setError(std::exception_ptr e) {
				if (!mExPtr)
					mExPtr = e;
			}
			virtual span<u8> asSpan(u64 resize) = 0;
		};
	}
}
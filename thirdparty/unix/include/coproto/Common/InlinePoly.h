#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "coproto/Common/Defines.h"
#include "coproto/Common/TypeTraits.h"
#include <iostream>
namespace coproto
{




	namespace internal
	{

		template<typename Interface, int StorageSize /* makes the whole thing 256 bytes */>
		class InlinePoly
		{

			// This controller interface will allow us to 
			// dectroy and move the underlying object.
			struct Controller
			{
				virtual ~Controller() {};

				// assumes dest is uninitialized and calls the 
				// placement new move constructor with this as 
				// dest destination.
				virtual void moveTo(InlinePoly<Interface, StorageSize>& dest) = 0;

				// return size actual size of the underlying object
				//virtual u64 sizeOf() const = 0;
			};

			// This type will contain a vtable which allows
			// us to dynamic dispatch to control actual instances
			template<typename U>
			struct ModelController : Controller
			{
				// construct the
				template<typename... Args, typename Enabled_If =
					enable_if_t<std::is_constructible<U, Args...>::value>>				
				ModelController(Args&& ... args)
					:mU(std::forward<Args>(args)...)
				{}

				void moveTo(InlinePoly<Interface, StorageSize>& dest) 
				{
					dest.emplace<U>(std::move(mU));
				}

				U mU;
			};

		public:

			using Storage = typename std::aligned_storage<StorageSize>::type;

			Storage mStorage;

			InlinePoly() = default;
			InlinePoly(const InlinePoly<Interface, StorageSize>&) = delete;

			InlinePoly(InlinePoly<Interface, StorageSize>&& m)
			{
				*this = std::forward<InlinePoly<Interface, StorageSize>>(m);
			}

			~InlinePoly()
			{
				destruct();
			}

			InlinePoly<Interface, StorageSize>& operator=(InlinePoly<Interface, StorageSize>&& m)
			{
				destruct();

				if (m.isStoredInline())
				{
					m.getController().moveTo(*this);
					m.destruct();
					m.mData = nullptr;
				}
				else
				{
					isOwning() = m.isOwning();
					mData = m.mData;

					m.mData = nullptr;
					m.isOwning() = false;
				}
				return *this;
			}

			bool& isOwning()
			{
				return *(bool*)&getController();
			}


			template<typename U, typename... Args>
			using is_emplaceable = is_poly_emplaceable<Interface, U, Args...>;

			template<typename U, typename... Args >
			enable_if_t<
				(sizeof(ModelController<U>) <= sizeof(Storage)) &&
				is_emplaceable<U, Args...>::value>
				emplace(Args&& ... args)
			{
				destruct();

				// Do a placement new to the local storage and then take the
				// address of the U type and store that in our data pointer.
				ModelController<U>* ptr = (ModelController<U>*) & getController();
				new (ptr) ModelController<U>(std::forward<Args>(args)...);
				mData = &(ptr->mU);
			}

			template<typename U, typename... Args >
			enable_if_t<
					(sizeof(ModelController<U>) > sizeof(Storage)) &&	
					is_emplaceable<U, Args...>::value>
				emplace(Args&& ... args)
			{
				destruct();

				// this object is too big, use the allocator. Local storage
				// will be unused as denoted by (isSBO() == false).
				mData = new U(std::forward<Args>(args)...);
				COPROTO_REG_NEW(mData, "InlinePoly");
				//std::cout << "new " << hexPtr(mData) << std::endl;
				isOwning() = true;
			}


			void setBorrowed(Interface* ptr)
			{
				mData = ptr;
				isOwning() = false;
			}
			void setOwned(Interface* ptr)
			{
				mData = ptr;
				isOwning() = true;
			}

			bool isStoredInline() const
			{
				auto begin = (u8*)this;
				auto end = begin + sizeof(InlinePoly<Interface, StorageSize>);
				return
					((u8*)get() >= begin) &&
					((u8*)get() < end);
			}




			void destruct()
			{
				if (isStoredInline())
					// manually call the virtual destructor.
					getController().~Controller();
				else if (get() && isOwning())
				{
					// let the compiler call the destructor
					//std::cout << "del " << hexPtr(get()) << std::endl;
					//--gNewDel;
					COPROTO_REG_DEL(get());
					delete get();
				}

				mData = nullptr;
			}

			Controller& getController()
			{
				return *(Controller*)&mStorage;
			}

			const Controller& getController() const
			{
				return *(Controller*)&mStorage;
			}


			Interface* operator->() { return get(); }
			Interface* get() { return mData; }

			const Interface* operator->() const { return get(); }
			const Interface* get() const { return mData; }
			Interface* mData = nullptr;

		};

	}


	namespace tests
	{
		void InlinePolyTest();
	}

}
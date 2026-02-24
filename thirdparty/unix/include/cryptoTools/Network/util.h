#pragma once
#include <cryptoTools/Common/config.h>
#ifdef ENABLE_BOOST

#include <boost/asio.hpp>
#include <boost/circular_buffer.hpp>
#include <memory>
#include <mutex>
#include <cassert>
#include <unordered_map>
//#include <cryptoTools/Network/IOService.h>

namespace osuCrypto
{

    class IOService;

    class Work
    {
    public:
        std::unique_ptr<boost::asio::io_service::work> mWork;
        std::string mReason;
        IOService& mIos;
        Work(IOService& ios, std::string reason);

        Work(Work&&) = default;

        ~Work();

        void reset();
    };



    template<typename T>
    class SpscQueue
    {
    public:

        struct BaseQueue
        {
            BaseQueue() = delete;
            BaseQueue(const BaseQueue&) = delete;
            BaseQueue(BaseQueue&&) = delete;

            //using StorageType = std::aligned_storage<sizeof(T)>;

            BaseQueue(u64 cap)
                : mPopIdx(0)
                , mPushIdx(0)
                , mData(new u8[cap * sizeof(T)])
                , mStorage((T*)mData.get(), cap)
            {};

            ~BaseQueue()
            {
                while (size())
                    pop_front();
            }

            u64 mPopIdx, mPushIdx;
            std::unique_ptr<u8[]> mData;
            span<T> mStorage;

            u64 capacity() const { return mStorage.size(); }
            u64 size() const { return mPushIdx - mPopIdx; }
            bool full() const { return size() == capacity(); }
            bool empty() const { return size() == 0; }

            void push_back(T&& v)
            {
                assert(size() < capacity());
                auto pushIdx = mPushIdx++ % capacity();
                void* data = &mStorage[pushIdx];
                new (data) T(std::forward<T>(v));
                //assert(tt && tt == (void*)(&mStorage[pushIdx]));
            }

            T& front()
            {
                assert(mPopIdx != mPushIdx);
                return *(T*)&mStorage[mPopIdx % capacity()];
            }

            void pop_front(T&out)
            {
                out = std::move(front());
                pop_front();
            }

            void pop_front()
            {
                front().~T();
                ++mPopIdx;
            }
        };

        //boost::circular_buffer_space_optimized<T> mQueue;
        std::list<BaseQueue> mQueues;
        mutable std::mutex mMtx;

        SpscQueue(u64 cap = 64)
        //    : mQueue(cap)
        { 
            mQueues.emplace_back(cap);
        }

        bool isEmpty() const {
            std::lock_guard<std::mutex> l(mMtx);
            //return mQueue.empty();
            return mQueues.front().empty();
        }

        void push_back(T&& v)
        {
            std::lock_guard<std::mutex> l(mMtx);
            //mQueue.push_back(std::forward<T>(v));
            if(mQueues.back().full())
                mQueues.emplace_back(mQueues.back().size() * 4);
            mQueues.back().push_back(std::forward<T>(v));            
        }

        T& front()
        {
            std::lock_guard<std::mutex> l(mMtx);
            //return mQueue.front();
            return mQueues.front().front();
        }

        void pop_front(T& out)
        {
            std::lock_guard<std::mutex> l(mMtx);
            //out = std::move(mQueue.front());
            //mQueue.pop_front();
            out = std::move(mQueues.front().front());
            mQueues.front().pop_front();
            if(mQueues.front().empty() && mQueues.size() > 1)
                mQueues.pop_front();
        }

        void pop_front()
        {
            T out;
            pop_front(out);
        }
    };


    template<typename T, int StorageSize = 248 /* makes the whole thing 256 bytes */>
    class SBO_ptr
    {
    public:
        struct SBOInterface
        {
            virtual ~SBOInterface() {};

            // assumes dest is uninitialized and calls the 
            // placement new move constructor with this as 
            // dest destination.
            virtual void moveTo(SBO_ptr<T, StorageSize>& dest) = 0;
        };

        template<typename U>
        struct Impl : SBOInterface
        {

            template<typename... Args,
                typename Enabled =
                typename std::enable_if<
                std::is_constructible<U, Args...>::value
                >::type
            >
                Impl(Args&& ... args)
                :mU(std::forward<Args>(args)...)
            {}

            void moveTo(SBO_ptr<T, StorageSize>& dest) override
            {
                assert(dest.get() == nullptr);
                dest.New<U>(std::move(mU));
            }

            U mU;
        };

        using base_type = T;
        using Storage = typename std::aligned_storage<StorageSize>::type;

        template<typename U>
        using Impl_type =  Impl<U>;

        T* mData = nullptr;
        Storage mStorage;

        SBO_ptr() = default;
        SBO_ptr(const SBO_ptr<T>&) = delete;

        SBO_ptr(SBO_ptr<T>&& m)
        {
            *this = std::forward<SBO_ptr<T>>(m);
        }

        ~SBO_ptr()
        {
            destruct();
        }


        SBO_ptr<T, StorageSize>& operator=(SBO_ptr<T>&& m)
        {
            destruct();

            if (m.isSBO())
            {
                m.getSBO().moveTo(*this);
            }
            else
            {
                std::swap(mData, m.mData);
            }
            return *this;
        }

        template<typename U, typename... Args >
        typename std::enable_if<
            (sizeof(Impl_type<U>) <= sizeof(Storage)) &&
            std::is_base_of<T, U>::value&&
            std::is_constructible<U, Args...>::value
        >::type
            New(Args&& ... args)
        {
            destruct();

            // Do a placement new to the local storage and then take the
            // address of the U type and store that in our data pointer.
            Impl<U>* ptr = (Impl<U>*) & getSBO();
            new (ptr) Impl<U>(std::forward<Args>(args)...);
            mData = &(ptr->mU);
        }

        template<typename U, typename... Args >
        typename std::enable_if<
            (sizeof(Impl_type<U>) > sizeof(Storage)) &&
            std::is_base_of<T, U>::value&&
            std::is_constructible<U, Args...>::value
                >::type
            New(Args&& ... args)
        {
            destruct();

            // this object is too big, use the allocator. Local storage
            // will be unused as denoted by (isSBO() == false).
            mData = new U(std::forward<Args>(args)...);
        }


        bool isSBO() const
        {
            auto begin = (u8*)this;
            auto end = begin + sizeof(SBO_ptr<T, StorageSize>);
            return
                ((u8*)get() >= begin) &&
                ((u8*)get() < end);
        }

        T* operator->() { return get(); }
        T* get() { return mData; }

        const T* operator->() const { return get(); }
        const T* get() const { return mData; }

        void destruct()
        {
            if (isSBO())
                // manually call the virtual destructor.
                getSBO().~SBOInterface();
            else if (get())
                // let the compiler call the destructor
                delete get();

            mData = nullptr;
        }
          
        SBOInterface& getSBO()
        {
            return *(SBOInterface*)& mStorage;
        }

        const SBOInterface& getSBO() const
        {
            return *(SBOInterface*)& mStorage;
        }
    };


    template<typename T, typename U, typename... Args>
    typename  std::enable_if<
        std::is_constructible<U, Args...>::value&&
        std::is_base_of<T, U>::value, SBO_ptr<T>>::type
        make_SBO_ptr(Args && ... args)
    {
        SBO_ptr<T> t;
        t.template New<U>(std::forward<Args>(args)...);
        return (t);
    }



    enum class SessionMode : bool { Client, Server };

}
#endif
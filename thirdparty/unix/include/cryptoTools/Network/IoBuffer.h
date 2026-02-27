#pragma once
// This file and the associated implementation has been placed in the public domain, waiving all copyright. No restrictions are placed on its use. 
#include <cryptoTools/Common/config.h>
#ifdef ENABLE_BOOST


#include <cryptoTools/Common/Defines.h>

#include <string> 
#include <future> 
#include <cassert> 
#include <functional> 
#include <memory> 
#include <boost/asio.hpp>
#include <system_error>
#include  <type_traits>
#include <list>
#include <boost/variant.hpp>
#ifdef ENABLE_NET_LOG
#include <cryptoTools/Common/Log.h>
#endif

//#define ENABLE_NET_LOG

namespace osuCrypto
{
    enum class Errc
    {
        success = 0
        //CloseChannel = 1 // error indicating we should call the close handler.
    };
}
namespace boost {
    namespace system {
        template <>
        struct is_error_code_enum<osuCrypto::Errc> : true_type {};
    }
}

namespace { // anonymous namespace

    struct osuCryptoErrCategory : boost::system::error_category
    {
        const char* name() const noexcept override
        {
            return "osuCrypto";
        }

        std::string message(int ev) const override
        {
            switch (static_cast<osuCrypto::Errc>(ev))
            {
            case osuCrypto::Errc::success:
                return "Success";

            // case osuCrypto::Errc::CloseChannel:
            //     return "the channel should be closed.";

            default:
                return "(unrecognized error)";
            }
        }
    };

    const osuCryptoErrCategory theCategory{};

} // anonymous namespace

namespace osuCrypto
{
    using error_code = boost::system::error_code;

    inline error_code make_error_code(Errc e)
    {
        return { static_cast<int>(e), theCategory };
    }

    class ChannelBase;
    using io_completion_handle = std::function<void(const error_code&, u64)>;
    using completion_handle = std::function<void(const error_code&)>;

    template<typename, typename T>
    struct has_resize {
        static_assert(
            std::integral_constant<T, false>::value,
            "Second template parameter needs to be of function type.");
    };

    // specialization that does the checking
    template<typename C, typename Ret, typename... Args>
    struct has_resize<C, Ret(Args...)> {
    private:
        template<typename T>
        static constexpr auto check(T*)
            -> typename
            std::is_same<
            decltype(std::declval<T>().resize(std::declval<Args>()...)),
            Ret    // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
            >::type;  // attempt to call it and see if the return type is correct

        template<typename>
        static constexpr std::false_type check(...);

        typedef decltype(check<C>(0)) type;

    public:
        static constexpr bool value = type::value;
    };


#define _SILENCE_CXX20_IS_POD_DEPRECATION_WARNING

    /// type trait that defines what is considered a STL like Container
    /// 
    /// Must have the following member types:  pointer, size_type, value_type
    /// Must have the following member functions:
    ///    * Container::pointer Container::data();
    ///    * Container::size_type Container::size();
    /// Must contain Plain Old Data:
    ///    * std::is_pod<Container::value_type>::value == true
    template<typename Container>
    using is_container =
        std::is_same<typename std::enable_if<
        std::is_convertible<
        typename Container::pointer,
        decltype(std::declval<Container>().data())>::value&&
        std::is_convertible<
        typename Container::size_type,
        decltype(std::declval<Container>().size())>::value&&
        std::is_pod<typename Container::value_type>::value&&
        std::is_pod<Container>::value == false>::type
        ,
        void>;

    // A class template that allows fewer than the specified number of bytes to be received. 
    template<typename T>
    class ReceiveAtMost
    {
    public:
        using pointer = T *;
        using value_type = T;
        using size_type = u64;

        T* mData;
        u64 mMaxReceiveSize, mTrueReceiveSize;


        // A constructor that takes the loction to be written to and 
        // the maximum number of T's that should be written. 
        // Call 
        ReceiveAtMost(T* dest, u64 maxReceiveCount)
            : mData(dest)
            , mMaxReceiveSize(maxReceiveCount)
            , mTrueReceiveSize(0)
        {}


        u64 size() const
        {
            if (mTrueReceiveSize)
                return mTrueReceiveSize;
            else
                return mMaxReceiveSize;
        }

        const T* data() const { return mData; }
        T* data() { return mData; }

        void resize(u64 size)
        {
            if (size > mMaxReceiveSize) throw std::runtime_error(LOCATION);
            mTrueReceiveSize = size;
        }

        u64 receivedSize() const
        {
            return mTrueReceiveSize;
        }
    };

    static_assert(is_container<ReceiveAtMost<u8>>::value, "sss");
    static_assert(has_resize<ReceiveAtMost<u8>, void(typename ReceiveAtMost<u8>::size_type)>::value, "sss");

    class BadReceiveBufferSize : public std::runtime_error
    {
    public:
        u64 mSize;
        
        BadReceiveBufferSize(const std::string& what, u64 length)
            :
            std::runtime_error(what),
            mSize(length)
        { }

        BadReceiveBufferSize(const BadReceiveBufferSize& src) = default;
        BadReceiveBufferSize(BadReceiveBufferSize&& src) = default;
    };

    class CanceledOperation : public std::runtime_error
    {
    public:
        CanceledOperation(const std::string& str)
            : std::runtime_error(str)
        {}
    };


    template<typename T>
    inline u8* channelBuffData(const T& container) { return (u8*)container.data(); }

    template<typename T>
    inline u64 channelBuffSize(const T& container) { return container.size() * sizeof(typename  T::value_type); }

    template<typename T>
    inline bool channelBuffResize(T& container, u64 size)
    {
        if (size % sizeof(typename  T::value_type)) return false;

        try {
            container.resize(size / sizeof(typename  T::value_type));
        }
        catch (...)
        {
            return false;
        }
        return true;
    }

    namespace details
    {
        class ChlOperation
        {
        public:
            ChlOperation() = default;
            ChlOperation(ChlOperation&& copy) = default;
            ChlOperation(const ChlOperation& copy) = default;

            virtual ~ChlOperation() {}

            // perform the operation
            virtual void asyncPerform(ChannelBase* base, io_completion_handle&& completionHandle) = 0;

            // cancel the operation, this is called durring or after asyncPerform 
            // has been called. If after then it should be a no-op.
            virtual void asyncCancelPending(ChannelBase* base, const error_code& ec) = 0;

            // cancel the operation, this is called before asyncPerform. 
            virtual void asyncCancel(ChannelBase* base, const error_code& ec, io_completion_handle&& completionHandle) = 0; 
            // { 
            //     auto ec2 = boost::system::errc::make_error_code(boost::system::errc::success);
            //     completionHandle(ec2, 0);
            // }

            virtual std::string toString() const = 0;

#ifdef ENABLE_NET_LOG
            u64 mIdx = 0;
            Log* mLog = nullptr;
            void log(std::string msg) { if (mLog) mLog->push(msg); }
#endif
        };

        class RecvOperation : public ChlOperation { };
        class SendOperation : public ChlOperation { };

        template<typename Base>
        struct BaseCallbackOp : public Base
        {
            std::function<void()> mCallback;

            BaseCallbackOp(const std::function<void()>& cb) : mCallback(cb){}
            BaseCallbackOp(std::function<void()>&& cb) : mCallback(std::move(cb)){}

            void asyncPerform(ChannelBase* base, io_completion_handle&& completionHandle) override {
                error_code ec = make_error_code(Errc::success);
                mCallback();
                completionHandle(ec, 0);
            }

            void asyncCancelPending(ChannelBase* base, const error_code& ec) override {}

            void asyncCancel(ChannelBase* base, const error_code&, io_completion_handle&& completionHandle) override
            {
                asyncPerform(base, std::forward<io_completion_handle>(completionHandle));
            }

            std::string toString() const override
            {
                return std::string("CallbackOp #") 
    #ifdef ENABLE_NET_LOG
                    + std::to_string(Base::mIdx)
    #endif
                    ;
            }
        };

        using RecvCallbackOp = BaseCallbackOp<RecvOperation>;
        using SendCallbackOp = BaseCallbackOp<SendOperation>;

        using size_header_type = u32;

        // A class for sending or receiving data over a channel. 
        // Datam sent/received with this type sent over the network 
        // with a header denoting its size in bytes.
        class BasicSizedBuff
        {
        public:

            BasicSizedBuff(BasicSizedBuff&& v)
            {
                mHeaderSize = v.mHeaderSize;
                mBuff = v.mBuff;
                v.mBuff = {};
            }
            BasicSizedBuff() = default;

            BasicSizedBuff(const u8* data, u64 size)
                : mHeaderSize(size_header_type(size))
                , mBuff{ (u8*)data,  span<u8>::size_type(size) }
            {
                assert(size < std::numeric_limits<size_header_type>::max());
            }

            void set(const u8* data, u64 size)
            {
                assert(size < std::numeric_limits<size_header_type>::max());
                mBuff = { (u8*)data, span<u8>::size_type(size) };
            }

            inline u64 getHeaderSize() const { return mHeaderSize; }
            inline u64 getBufferSize() const { return mBuff.size(); }
            inline u8* getBufferData() { return mBuff.data(); }

            inline std::array<boost::asio::mutable_buffer, 2> getSendBuffer()
            {
                assert(mBuff.size());
                mHeaderSize = size_header_type(mBuff.size());
                return { { getRecvHeaderBuffer(), getRecvBuffer() } };
            }

            inline boost::asio::mutable_buffer getRecvHeaderBuffer() {
                return boost::asio::mutable_buffer(&mHeaderSize, sizeof(size_header_type));
            }

            inline boost::asio::mutable_buffer getRecvBuffer() {
                return boost::asio::mutable_buffer(mBuff.data(), mBuff.size());
            }

        protected:
            size_header_type mHeaderSize = 0;
            span<u8> mBuff;
        };




        class FixedSendBuff : public BasicSizedBuff, public SendOperation
        {
        public:
            FixedSendBuff() = default;
            FixedSendBuff(const u8* data, u64 size)
                : BasicSizedBuff(data, size)
            {}

            FixedSendBuff(FixedSendBuff&& v) = default;

            void asyncPerform(ChannelBase* base, io_completion_handle&& completionHandle) override;
            
            void asyncCancelPending(ChannelBase* base, const error_code& ec) override {}

            void asyncCancel(ChannelBase* base, const error_code&, io_completion_handle&& completionHandle) override {
                error_code ec = make_error_code(Errc::success);
                completionHandle(ec , 0);
            }

            std::string toString() const override;
        };

        template <typename F>
        class MoveSendBuff : public FixedSendBuff {
        public:
            MoveSendBuff() = delete;
            F mObj;

            MoveSendBuff(F&& obj)
                : mObj(std::move(obj))
            {   // set must be called after the move in case channelBuffData(mObj) != channelBuffData(obj)
                set(channelBuffData(mObj), channelBuffSize(mObj));
            }

            MoveSendBuff(MoveSendBuff&& v)
                : MoveSendBuff(std::move(v.mObj))
            {}
        };

        template <typename T>
        class MoveSendBuff<std::unique_ptr<T>> :public FixedSendBuff {
        public:
            MoveSendBuff() = delete;
            typedef std::unique_ptr<T> F;
            F mObj;
            MoveSendBuff(F&& obj)
                : FixedSendBuff(channelBuffData(*obj), channelBuffSize(*obj))
                , mObj(std::move(obj))
            {}

            MoveSendBuff(MoveSendBuff<std::unique_ptr<T>>&& v)
                : MoveSendBuff(std::move(v.mObj))
            {}

        };

        template <typename T>
        class MoveSendBuff<std::shared_ptr<T>> :public FixedSendBuff {
        public:
            MoveSendBuff() = delete;
            typedef std::shared_ptr<T> F;
            F mObj;
            MoveSendBuff(F&& obj)
                : FixedSendBuff(channelBuffData(*obj), channelBuffSize(*obj))
                , mObj(std::move(obj))
            {}

            MoveSendBuff(MoveSendBuff<std::unique_ptr<T>>&& v)
                : MoveSendBuff(std::move(v.mObj))
            {}
        };

        template <typename F>
        class  RefSendBuff :public FixedSendBuff {
        public:
            const F& mObj;
            RefSendBuff(const F& obj)
                : FixedSendBuff(channelBuffData(*obj), channelBuffSize(*obj))
                , mObj(obj)
            {}

            RefSendBuff(RefSendBuff<F>&& v)
                :RefSendBuff(v.obj)
            {}
        };


        class FixedRecvBuff : public BasicSizedBuff, public RecvOperation
        {
        public:

            io_completion_handle mComHandle;
            ChannelBase* mBase = nullptr;
            std::promise<void> mPromise;


            FixedRecvBuff(FixedRecvBuff&& v)
                : BasicSizedBuff(v.getBufferData(), v.getBufferSize())
                , RecvOperation(static_cast<RecvOperation&&>(v))
                , mComHandle(std::move(v.mComHandle))
                , mBase(v.mBase)
                , mPromise(std::move(v.mPromise))
            {}

            FixedRecvBuff(std::future<void>& fu)
            {
                fu = mPromise.get_future();
            }

            FixedRecvBuff(const u8* data, u64 size, std::future<void>& fu)
                : BasicSizedBuff(data, size)
            {
                fu = mPromise.get_future();
            }

            void asyncPerform(ChannelBase* base, io_completion_handle&& completionHandle) override;

            void asyncCancel(ChannelBase* base, const error_code& ec, io_completion_handle&& completionHandle) override
            {
                mPromise.set_exception(std::make_exception_ptr(std::runtime_error(ec.message())));
                completionHandle(ec, 0);
            }
            
            void asyncCancelPending(ChannelBase* base, const error_code& ec) override {}


            std::string toString() const override;

            virtual void resizeBuffer(u64) {}
        };

        template <typename F>
        class  RefRecvBuff :public FixedRecvBuff {
        public:
            const F& mObj;
            RefRecvBuff(const F& obj, std::future<void>& fu)
                : FixedRecvBuff(fu)
                , mObj(obj)
            {   // set must be called after the move in case channelBuffData(mObj) != channelBuffData(obj)
                set(channelBuffData(mObj), channelBuffSize(mObj));
            }

            RefRecvBuff(RefRecvBuff<F>&& v)
                : FixedRecvBuff(std::move(v))
                , mObj(v.mObj)
            {}

        };

        template <typename F>
        class ResizableRefRecvBuff : public FixedRecvBuff {
        public:
            ResizableRefRecvBuff() = delete;
            F& mObj;

            ResizableRefRecvBuff(F& obj, std::future<void>& fu)
                : FixedRecvBuff(fu)
                , mObj(obj)
            {}

            ResizableRefRecvBuff(ResizableRefRecvBuff<F>&& v)
                : FixedRecvBuff(std::move(v))
                , mObj(v.mObj)
            {}

            virtual void resizeBuffer(u64 size) override
            {
                channelBuffResize(mObj, size);
                set((u8*)channelBuffData(mObj), channelBuffSize(mObj));
            }
        };




        boost::asio::io_context& getIOService(ChannelBase* base);

        template< typename T>
        class WithCallback : public T
        {
        public:

            template<typename CB, typename... Args>
            WithCallback(CB&& cb, Args&& ... args)
                : T(std::forward<Args>(args)...)
                , mCallback(std::forward<CB>(cb))
            {}

            WithCallback(WithCallback<T>&& v)
                : T(std::move(v))
                , mCallback(std::move(v.mCallback))
            {}
            boost::variant<
                std::function<void()>,
                std::function<void(const error_code&)>>mCallback;
            io_completion_handle mWithCBCompletionHandle;

            void asyncPerform(ChannelBase* base, io_completion_handle&& completionHandle) override
            {
                mWithCBCompletionHandle = std::move(completionHandle);

                T::asyncPerform(base, [this, base](const error_code& ec, u64 bytes) mutable
                    {
                        if (mCallback.which() == 0)
                        {
                            auto& c = boost::get<std::function<void()>>(mCallback);
                            if (c)
                            {
                                boost::asio::post(getIOService(base).get_executor(), std::move(c));
                            }
                        }
                        else
                        {
                            auto& c = boost::get<std::function<void(const error_code&)>>(mCallback);
                            if (c)
                            {
                                boost::asio::post(getIOService(base).get_executor(), [cc = std::move(c), ec](){
                                    cc(ec);
                                });
                            }
                        }

                        mWithCBCompletionHandle(ec, bytes);
                    });
            }

            void asyncCancel(ChannelBase* base, const error_code& ec, io_completion_handle&& completionHandle) override
            {

                if (mCallback.which() == 1)
                {
                    auto& c = boost::get<std::function<void(const error_code&)>>(mCallback);
                    if (c)
                        c(ec);
                    c = {};
                }

                T::asyncCancel(base, ec, std::forward<io_completion_handle>(completionHandle));
            }
            
            void asyncCancelPending(ChannelBase* base, const error_code& ec) override {}

        };

        template< typename T>
        class WithPromise : public T
        {
        public:

            template<typename... Args>
            WithPromise(std::future<void>& f, Args&& ... args)
                : T(std::forward<Args>(args)...)
            {
                f = mPromise.get_future();
            }

            WithPromise(WithPromise<T>&& v)
                : T(std::move(v))
                , mPromise(std::move(v.mPromise))
            {}


            std::promise<void> mPromise;
            io_completion_handle mWithPromCompletionHandle;

            void asyncPerform(ChannelBase* base, io_completion_handle&& completionHandle) override
            {
                mWithPromCompletionHandle = std::move(completionHandle);

                T::asyncPerform(base, [this](const error_code& ec, u64 bytes) mutable
                    {
                        if (ec) mPromise.set_exception(std::make_exception_ptr(
                            CanceledOperation("network send error: " + ec.message() + "\n" LOCATION)));
                        else mPromise.set_value();

                        mWithPromCompletionHandle(ec, bytes);
                    });
            }

            void asyncCancel(ChannelBase* base,const error_code& ec, io_completion_handle&& completionHandle) override
            {
                mPromise.set_exception(
                    std::make_exception_ptr(
                        CanceledOperation(ec.message())));

                T::asyncCancel(base, ec, std::forward<io_completion_handle>(completionHandle));
            }


        };


    }
}
#endif
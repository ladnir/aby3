#pragma once
// This file and the associated implementation has been placed in the public domain, waiving all copyright. No restrictions are placed on its use.
#include <cryptoTools/Common/config.h>
#ifdef ENABLE_BOOST

#include <cryptoTools/Common/Defines.h>
#include <cryptoTools/Network/IoBuffer.h>
#include <cryptoTools/Network/SocketAdapter.h>
#include <cryptoTools/Network/util.h>

#ifdef ENABLE_NET_LOG
#include <cryptoTools/Common/Log.h>
#endif
#include <future>
#include <cassert>
#include <ostream>
#include <list>
#include <deque>

#include "TLS.h"

namespace osuCrypto {

    class ChannelBase;
    class Session;
    class IOService;
    class SocketInterface;

    // Channel is the standard interface use to send data over the network.
    // See frontend_cryptoTools/Tutorial/Network.cpp for examples.
    class Channel
    {
    public:

        // The default constructors
        Channel() = default;
        Channel(const Channel& copy);
        Channel(Channel&& move) = default;

        // Special constructor used to construct a Channel from some socket.
        // Note, Channel takes ownership of the socket and will delete it
        // when done.
        Channel(IOService& ios, SocketInterface* sock);

        ~Channel();

        // Default assignment
        Channel& operator=(Channel&& move);

        // Default assignment
        Channel& operator=(const Channel& copy);




        //////////////////////////////////////////////////////////////////////////////
        //						   Sending interface								//
        //////////////////////////////////////////////////////////////////////////////

        // Sends length number of T pointed to by src over the network. The type T
        // must be POD. Returns once all the data has been sent.
        template<typename T>
        typename std::enable_if<std::is_pod<T>::value, void>::type
            send(const T* src, u64 length);

        // Sends the data in buf over the network. The type Container  must meet the
        // requirements defined in IoBuffer.h. Returns once all the data has been sent.
        template <class T>
        typename std::enable_if<std::is_pod<T>::value, void>::type
            send(const T& buf);

        // Sends the data in buf over the network. The type Container  must meet the
        // requirements defined in IoBuffer.h. Returns once all the data has been sent.
        template <class Container>
        typename std::enable_if<is_container<Container>::value, void>::type
            send(const Container& buf);


        // Sends the data in buf over the network. The type T must be POD.
        // Returns before the data has been sent. The life time of the data must be
        // managed externally to ensure it lives longer than the async operations.
        template<typename T>
        typename std::enable_if<std::is_pod<T>::value, void>::type
            asyncSend(const T* data, u64 length);

        // Sends the data in buf over the network. The type Container  must meet the
        // requirements defined in IoBuffer.h. Returns before the data has been sent.
        // The life time of the data must be managed externally to ensure it lives
        // longer than the async operations.  callback is a function that is called
        // from another thread once the send operation has succeeded.
        template<typename Container>
        typename std::enable_if<is_container<Container>::value, void>::type
            asyncSend(Container&& data, std::function<void()> callback);


        // Sends the data in buf over the network. The type Container  must meet the
        // requirements defined in IoBuffer.h. Returns before the data has been sent.
        // The life time of the data must be managed externally to ensure it lives
        // longer than the async operations.  callback is a function that is called
        // from another thread once the send operation has completed.
        template<typename Container>
        typename std::enable_if<is_container<Container>::value, void>::type
            asyncSend(Container&& data, std::function<void(const error_code&)> callback);


        // Sends the data in buf over the network. The type T must be POD.
        // Returns before the data has been sent. The life time of the data must be
        // managed externally to ensure it lives longer than the async operations.
        template<typename T>
        typename std::enable_if<std::is_pod<T>::value, void>::type
            asyncSend(const T& data);

        // Sends the data in buf over the network. The type T must be POD.
        // Returns before the data has been sent. The life time of the data must be
        // managed externally to ensure it lives longer than the async operations.
        template<typename Container>
        typename std::enable_if<is_container<Container>::value, void>::type
            asyncSend(const Container& data);

        // Sends the data in buf over the network. The type Container  must meet the
        // requirements defined in IoBuffer.h. Returns before the data has been sent.
        template <class Container>
        typename std::enable_if<is_container<Container>::value, void>::type
            asyncSend(Container&& c);

        // Sends the data in buf over the network. The type Container  must meet the
        // requirements defined in IoBuffer.h. Returns before the data has been sent.
        template <class Container>
        typename std::enable_if<is_container<Container>::value, void>::type
            asyncSend(std::unique_ptr<Container> buffer);

        // Sends the data in buf over the network. The type Container  must meet the
        // requirements defined in IoBuffer.h. Returns before the data has been sent.
        template <class Container>
        typename std::enable_if<is_container<Container>::value, void>::type
            asyncSend(std::shared_ptr<Container> buffer);


        // Sends the data in buf over the network. The type T must be POD.
        // Returns before the data has been sent. The life time of the data must be
        // managed externally to ensure it lives longer than the async operations.
        template<typename T>
        typename std::enable_if<std::is_pod<T>::value, std::future<void>>::type
            asyncSendFuture(const T* data, u64 length);


        // Performs a data copy and then sends the data in buf over the network.
        //  The type T must be POD. Returns before the data has been sent.
        template<typename T>
        typename std::enable_if<std::is_pod<T>::value, void>::type
            asyncSendCopy(const T& buff);

        // Performs a data copy and then sends the data in buf over the network.
        //  The type T must be POD. Returns before the data has been sent.
        template<typename T>
        typename std::enable_if<std::is_pod<T>::value, void>::type
            asyncSendCopy(const T* bufferPtr, u64 length);

        // Performs a data copy and then sends the data in buf over the network.
        // The type Container must meet the requirements defined in IoBuffer.h.
        // Returns before the data has been sent.
        template <typename  Container>
        typename std::enable_if<is_container<Container>::value, void>::type
            asyncSendCopy(const Container& buf);


        //////////////////////////////////////////////////////////////////////////////
        //						   Receiving interface								//
        //////////////////////////////////////////////////////////////////////////////

        // Receive data over the network. If possible, the container c will be resized
        // to fit the data. The function returns once all the data has been received.
        template <class Container>
        typename std::enable_if<
            is_container<Container>::value&&
            has_resize<Container, void(typename Container::size_type)>::value, void>::type
            recv(Container& c)
        {
            asyncRecv(c).get();
        }

        // Receive data over the network. The container c must be the correct size to
        // fit the data. The function returns once all the data has been received.
        template <class Container>
        typename std::enable_if<
            is_container<Container>::value &&
            !has_resize<Container, void(typename Container::size_type)>::value, void>::type
            recv(Container& c)
        {
            asyncRecv(c).get();
        }

        // Receive data over the network. The function returns once all the data
        // has been received.
        template<typename T>
        typename std::enable_if<std::is_pod<T>::value, void>::type
            recv(T* dest, u64 length);

        // Receive data over the network. The function returns once all the data
        // has been received.
        template<typename T>
        typename std::enable_if<std::is_pod<T>::value, void>::type
            recv(T& dest) { recv(&dest, 1); }

        // Receive data over the network asynchronously. The function returns right away,
        // before the data has been received. When all the data has benn received the
        // future is set.
        template<typename T>
        typename std::enable_if<std::is_pod<T>::value, std::future<void>>::type
            asyncRecv(T* dest, u64 length);

        // Receive data over the network asynchronously. The function returns right away,
        // before the data has been received. When all the data has benn received the
        // future is set and the callback fn is called.
        template<typename T>
        typename std::enable_if<std::is_pod<T>::value, std::future<void>>::type
            asyncRecv(T* dest, u64 length, std::function<void()> fn);

        // Receive data over the network asynchronously. The function returns right away,
        // before the data has been received. When all the data has benn received the
        // future is set.
        template<typename T>
        typename std::enable_if<std::is_pod<T>::value, std::future<void>>::type
            asyncRecv(T& dest) { return asyncRecv(&dest, 1); }

        // Receive data over the network asynchronously. The function returns right away,
        // before the data has been received. When all the data has benn received the
        // future is set. The container must be the correct size to fit the data received.
        template <class Container>
        typename std::enable_if<
            is_container<Container>::value &&
            !has_resize<Container, void(typename Container::size_type)>::value, std::future<void>>::type
            asyncRecv(Container & c);

        // Receive data over the network asynchronously. The function returns right away,
        // before the data has been received. When all the data has benn received the
        // future is set. The container is resized to fit the data.
        template <class Container>
        typename std::enable_if<
            is_container<Container>::value&&
            has_resize<Container, void(typename Container::size_type)>::value, std::future<void>>::type
            asyncRecv(Container & c);

        // Receive data over the network asynchronously. The function returns right away,
        // before the data has been received. When all the data has benn received the
        // future is set and the callback fn is called. The container must be the correct
        // size to fit the data received.
        template <class Container>
        typename std::enable_if<
            is_container<Container>::value&&
            has_resize<Container, void(typename Container::size_type)>::value, std::future<void>>::type
            asyncRecv(Container & c, std::function<void()> fn);

        // Receive data over the network asynchronously. The function returns right away,
        // before the data has been received. When all the data has benn received the
        // future is set and the callback fn is called. The container must be the correct
        // size to fit the data received.
        template <class Container>
        typename std::enable_if<
            is_container<Container>::value&&
            !has_resize<Container, void(typename Container::size_type)>::value, std::future<void>>::type
            asyncRecv(Container & c, std::function<void(const error_code&)> fn);

        // Receive data over the network asynchronously. The function returns right away,
        // before the data has been received. When all the data has benn received the 
        // future is set and the callback fn is called. The container must be the correct 
        // size to fit the data received.
        template <class Container>
        typename std::enable_if<
            is_container<Container>::value&&
            has_resize<Container, void(typename Container::size_type)>::value, std::future<void>>::type
            asyncRecv(Container & c, std::function<void(const error_code&)> fn);


        //////////////////////////////////////////////////////////////////////////////
        //						   Utility functions								//
        //////////////////////////////////////////////////////////////////////////////

        // Get the local endpoint for this channel.
        //Session& getSession();

        // The handle for this channel. Both ends will always have the same name.
        std::string getName() const;

        // Returns the name of the remote endpoint.
        std::string getRemoteName() const;

        // Return the name of the endpoint of this channel has once.
        Session getSession() const;

        // Sets the data send and recieved counters to zero.
        void resetStats();

        // Returns the amount of data that this channel has sent since it was created or when resetStats() was last called.
        u64 getTotalDataSent() const;

        // Returns the amount of data that this channel has sent since it was created or when resetStats() was last called.
        u64 getTotalDataRecv() const;

        // Returns the maximum amount of data that this channel has queued up to send since it was created or when resetStats() was last called.
        //u64 getMaxOutstandingSendData() const;

        // Returns whether this channel is open in that it can send/receive data
        bool isConnected();

        // A blocking call that waits until the channel is open in that it can send/receive data
        // Returns if the connection has been made. Always true if no timeout is provided.
        bool waitForConnection(std::chrono::milliseconds timeout);

        // A blocking call that waits until the channel is open in that it can send/receive data
        // Returns if the connection has been made.
        void waitForConnection();

        void onConnect(completion_handle handle);

        // Close this channel to denote that no more data will be sent or received.
        // blocks until all pending operations have completed.
        void close();

        // Aborts all current operations (connect, send, receive).
        void cancel(bool close = true);

        void asyncClose(std::function<void()> completionHandle);

        void asyncCancel(std::function<void()> completionHandle, bool close = true);


        enum class Status { Normal, Closing, Closed, Canceling};

        std::string commonName();

        std::shared_ptr<ChannelBase> mBase;

        operator bool() const
        {
            return static_cast<bool>(mBase);
        }

    private:

        friend class IOService;
        friend class Session;
        Channel(Session& endpoint, std::string localName, std::string remoteName);
    };


    inline std::ostream& operator<< (std::ostream& o, const Channel::Status& s)
    {
        switch (s)
        {
        case Channel::Status::Normal:
            o << "Status::Normal";
            break;
        case Channel::Status::Closing:
            o << "Status::Closing";
            break;
        case Channel::Status::Closed:
            o << "Status::Closed";
            break;
        case Channel::Status::Canceling:
            o << "Status::Canceling";
            break;
        default:
            o << "Status::??????";
            break;
        }
        return o;
    }



    class SocketConnectError : public std::runtime_error
    {
    public:
        SocketConnectError(const std::string& reason)
            :std::runtime_error(reason)
        {}
    };

    struct SessionBase;



    struct StartSocketOp
    {

        StartSocketOp(std::shared_ptr<ChannelBase> chl);

        void setHandle(io_completion_handle&& completionHandle, bool sendOp);
        void cancelPending(bool sendOp);
        void connectCallback(const error_code& ec);
        bool canceled() const;
        void asyncConnectToServer();
        void recvServerMessage();
        void sendConnectionString();
        void retryConnect(const error_code& ec);

        char mRecvChar;
        void setSocket(std::unique_ptr<BoostSocketInterface> socket, const error_code& ec);

        void finalize(std::unique_ptr<SocketInterface> sock, error_code ec);

        void addComHandle(completion_handle&& comHandle)
        {
            boost::asio::dispatch(mStrand, [this, ch = std::forward<completion_handle>(comHandle)]() mutable {
                if (mFinalized)
                {
                    ch(mEC);
                }
                else
                    mComHandles.emplace_back(std::forward<completion_handle>(ch));
            }
            );
        }


        boost::asio::deadline_timer mTimer;

        enum class ComHandleStatus { Uninit, Init, Eval };
        ComHandleStatus mSendStatus = ComHandleStatus::Uninit;
        ComHandleStatus mRecvStatus = ComHandleStatus::Uninit;

        boost::asio::strand<boost::asio::io_context::executor_type>& mStrand;
        std::vector<u8> mSendBuffer;
        std::unique_ptr<BoostSocketInterface> mSock;

#ifdef ENABLE_WOLFSSL
        void validateTLS(const error_code& ec);
        std::unique_ptr<TLSSocket> mTLSSock;
        block mTLSSessionIDBuf;
#endif

        double mBackoff = 1;
        bool mFinalized = false, mCanceled = false, mIsFirst;
        error_code mEC, mConnectEC;
        ChannelBase* mChl;
        io_completion_handle mSendComHandle, mRecvComHandle;
        std::list<completion_handle> mComHandles;

    };


    struct StartSocketSendOp : public details::SendOperation
    {
        StartSocketOp* mBase;

        StartSocketSendOp(StartSocketOp* base)
            : mBase(base) {}

        void asyncPerform(ChannelBase*, io_completion_handle&& completionHandle) override {
            mBase->setHandle(std::forward<io_completion_handle>(completionHandle), true);
        }
        void asyncCancelPending(ChannelBase* base, const error_code& ec) override {
            mBase->cancelPending(true);
        }

        void asyncCancel(ChannelBase* base, const error_code& ec, io_completion_handle&& completionHandle) override
        {
            mBase->setHandle(std::forward<io_completion_handle>(completionHandle), true);
            mBase->cancelPending(true);
        }

        std::string toString() const override {
            return std::string("StartSocketSendOp # ")
#ifdef ENABLE_NET_LOG
                + std::to_string(mIdx)
#endif
                ;
        }
    };

    struct StartSocketRecvOp : public details::RecvOperation
    {
        StartSocketOp* mBase;
        StartSocketRecvOp(StartSocketOp* base)
            : mBase(base) {}

        void asyncPerform(ChannelBase* base, io_completion_handle&& completionHandle) override {
            mBase->setHandle(std::forward<io_completion_handle>(completionHandle), false);
        }
        void asyncCancelPending(ChannelBase* base, const error_code& ec) override { mBase->cancelPending(false); }

        void asyncCancel(ChannelBase* base, const error_code& ec, io_completion_handle&& completionHandle) override
        {
            mBase->setHandle(std::forward<io_completion_handle>(completionHandle), false);
            mBase->cancelPending(false);
        }

        std::string toString() const override {
            return std::string("StartSocketRecvOp # ")
#ifdef ENABLE_NET_LOG
                + std::to_string(mIdx)
#endif
                ;
        }
    };


    // The Channel base class the actually holds a socket.
    class ChannelBase : public std::enable_shared_from_this<ChannelBase>
    {
    public:
        ChannelBase(Session& endpoint, std::string localName, std::string remoteName);
        ChannelBase(IOService& ios, SocketInterface* sock);
        ~ChannelBase();

        IOService& mIos;
        Work mWork;
        std::unique_ptr<StartSocketOp> mStartOp;

        std::shared_ptr<SessionBase> mSession;
        std::string mRemoteName, mLocalName;


        Channel::Status mStatus = Channel::Status::Normal;

        std::atomic<u32> mChannelRefCount;

        std::shared_ptr<ChannelBase> mRecvLoopLifetime, mSendLoopLifetime;

        bool recvSocketAvailable() const { return !mRecvLoopLifetime;}
        bool sendSocketAvailable() const { return !mSendLoopLifetime;}

        bool mRecvCancelNew = false;
        bool mSendCancelNew = false;

        std::unique_ptr<SocketInterface> mHandle;

        boost::asio::strand<boost::asio::io_context::executor_type> mStrand;

        u64 mTotalSentData = 0;
        u64 mTotalRecvData = 0;

        void cancelRecvQueue(const error_code& ec);
        void cancelSendQueue(const error_code& ec);

        void close();
        void cancel(bool close);
        void asyncClose(std::function<void()> completionHandle);
        void asyncCancel(std::function<void()> completionHandle, bool close);

        IOService& getIOService() { return mIos; }

        bool stopped() { return mStatus == Channel::Status::Closed; }

        //bool mActiveRecvSizeError = false;
        //bool activeRecvSizeError() const { return mActiveRecvSizeError; }


        SpscQueue<SBO_ptr<details::SendOperation>> mSendQueue;
        SpscQueue<SBO_ptr<details::RecvOperation>> mRecvQueue;
        void recvEnque(SBO_ptr<details::RecvOperation>&& op);
        void sendEnque(SBO_ptr<details::SendOperation>&& op);


        void asyncPerformRecv();
        void asyncPerformSend();

        std::string commonName();


        std::array<boost::asio::mutable_buffer, 2> mSendBuffers;
        boost::asio::mutable_buffer mRecvBuffer;

        void printError(std::string s);

#ifdef ENABLE_NET_LOG
        u32 mRecvIdx = 0, mSendIdx = 0;
        Log mLog;
#endif

    };



    template<class Container>
    typename std::enable_if<is_container<Container>::value, void>::type Channel::asyncSend(std::unique_ptr<Container> c)
    {
        // less that 32 bit max
        assert(channelBuffSize(*c) < u32(-1));

        if (channelBuffSize(*c) == 0) return;

        auto op = make_SBO_ptr<
            details::SendOperation,
            details::MoveSendBuff<
                std::unique_ptr<Container>
            >>(std::move(c));

        mBase->sendEnque(std::move(op));
    }

    template<class Container>
    typename std::enable_if<is_container<Container>::value, void>::type Channel::asyncSend(std::shared_ptr<Container> c)
    {
        // less that 32 bit max
        assert(channelBuffSize(*c) < u32(-1));

        if (channelBuffSize(*c) == 0) return;


        auto op = make_SBO_ptr<
            details::SendOperation,
            details::MoveSendBuff<
                std::shared_ptr<Container>
            >>(std::move(c));

        mBase->sendEnque(std::move(op));
    }


    template<class Container>
    typename std::enable_if<is_container<Container>::value, void>::type Channel::asyncSend(const Container& c)
    {
        // less that 32 bit max
        assert(channelBuffSize(c) < u32(-1));

        if (channelBuffSize(c) == 0) return;

        auto* buff = (u8*)c.data();
        auto size = c.size() * sizeof(typename Container::value_type);

        auto op = make_SBO_ptr<
            details::SendOperation,
            details::FixedSendBuff>(buff, size);

        mBase->sendEnque(std::move(op));
    }

    template<class Container>
    typename std::enable_if<is_container<Container>::value, void>::type Channel::asyncSend(Container&& c)
    {
        // less that 32 bit max
        assert(channelBuffSize(c) < u32(-1));

        if (channelBuffSize(c) == 0) return;

        auto op = make_SBO_ptr<
            details::SendOperation,
            details::MoveSendBuff<Container>>
            (std::move(c));

        mBase->sendEnque(std::move(op));
    }

    template <class Container>
    typename std::enable_if<
        is_container<Container>::value &&
        !has_resize<Container, void(typename Container::size_type)>::value, std::future<void>>::type
        Channel::asyncRecv(Container & c)
    {
        // less that 32 bit max
        assert(channelBuffSize(c) < u32(-1));

        if (channelBuffSize(c) == 0)
        {
            std::promise<void> trivialPromise;
            trivialPromise.set_value();
            return trivialPromise.get_future();
        }

        std::future<void> future;
        auto op = make_SBO_ptr<
            details::RecvOperation,
            details::RefRecvBuff<Container>>
            (c, future);

        mBase->recvEnque(std::move(op));

        return future;
    }

    template <class Container>
    typename std::enable_if<
        is_container<Container>::value&&
        has_resize<Container, void(typename Container::size_type)>::value, std::future<void>>::type
        Channel::asyncRecv(Container & c)
    {
        std::future<void> future;
        auto op = make_SBO_ptr<
            details::RecvOperation,
            details::ResizableRefRecvBuff<Container>>
            (c, future);

        mBase->recvEnque(std::move(op));
        return future;
    }


    template <class Container>
    typename std::enable_if<
        is_container<Container>::value&&
        has_resize<Container, void(typename Container::size_type)>::value, std::future<void>>::type
        Channel::asyncRecv(Container & c, std::function<void()> fn)
    {
        std::future<void> future;
        auto op = make_SBO_ptr<
            details::RecvOperation,
            details::WithCallback<
                details::ResizableRefRecvBuff<Container>
            >>
            (std::move(fn), c, future);

        mBase->recvEnque(std::move(op));

        return future;
    }


    template <class Container>
    typename std::enable_if<
        is_container<Container>::value&&
        !has_resize<Container, void(typename Container::size_type)>::value, std::future<void>>::type
        Channel::asyncRecv(Container & c, std::function<void(const error_code&)> fn)
    {
        std::future<void> future;
        auto op = make_SBO_ptr<
            details::RecvOperation,
            details::WithCallback<
                details::RefRecvBuff<Container>
            >>(std::move(fn), c, future);

        mBase->recvEnque(std::move(op));

        return future;
    }



    template <class Container>
    typename std::enable_if<
        is_container<Container>::value&&
        has_resize<Container, void(typename Container::size_type)>::value, std::future<void>>::type
        Channel::asyncRecv(Container & c, std::function<void(const error_code&)> fn)
    {
        std::future<void> future;
        auto op = make_SBO_ptr<
            details::RecvOperation,
            details::WithCallback<
                details::ResizableRefRecvBuff<Container>
            >>(std::move(fn), c, future);

        mBase->recvEnque(std::move(op));

        return future;
    }

    template<class Container>
    typename std::enable_if<is_container<Container>::value, void>::type Channel::send(const Container& buf)
    {
        send(channelBuffData(buf), channelBuffSize(buf));
    }

    template<typename Container>
    typename std::enable_if<is_container<Container>::value, void>::type Channel::asyncSendCopy(const Container& buf)
    {
        asyncSend(std::move(Container(buf)));
    }


    template<typename T>
    typename std::enable_if<std::is_pod<T>::value, void>::type
        Channel::send(const T* buffT, u64 sizeT)
    {
        asyncSendFuture(buffT, sizeT).get();
    }


    template<typename T>
    typename std::enable_if<std::is_pod<T>::value, std::future<void>>::type
        Channel::asyncSendFuture(const T* buffT, u64 sizeT)
    {
        u8* buff = (u8*)buffT;
        auto size = sizeT * sizeof(T);

        // less that 32 bit max
        assert(size < u32(-1));

        if (size == 0)
        {
            std::promise<void> trivialPromise;
            trivialPromise.set_value();
            return trivialPromise.get_future();
        }

        std::future<void> future;
        auto op = make_SBO_ptr<
            details::SendOperation,
            details::WithPromise<details::FixedSendBuff>>
            (future, buff, size);

        mBase->sendEnque(std::move(op));

        return future;
    }


    template<typename T>
    typename std::enable_if<std::is_pod<T>::value, void>::type
        Channel::send(const T& buffT)
    {
        send(&buffT, 1);
    }

    template<typename T>
    typename std::enable_if<std::is_pod<T>::value, std::future<void>>::type
        Channel::asyncRecv(T* buffT, u64 sizeT)
    {
        u8* buff = (u8*)buffT;
        auto size = sizeT * sizeof(T);

        // less that 32 bit max
        assert(size < u32(-1));

        if (size == 0)
        {
            std::promise<void> trivialPromise;
            trivialPromise.set_value();
            return trivialPromise.get_future();
        }

        std::future<void> future;
        auto op = make_SBO_ptr<
            details::RecvOperation,
            details::FixedRecvBuff>
            (buff, size, future);

        mBase->recvEnque(std::move(op));
        return future;
    }

    template<typename T>
    typename std::enable_if<std::is_pod<T>::value, std::future<void>>::type
        Channel::asyncRecv(T* buffT, u64 sizeT, std::function<void()> fn)
    {
        u8* buff = (u8*)buffT;
        auto size = sizeT * sizeof(T);

        // less that 32 bit max
        assert(size < u32(-1));

        if (size == 0)
        {
            std::promise<void> trivialPromise;
            trivialPromise.set_value();
            return trivialPromise.get_future();
        }

        std::future<void> future;
        auto op = make_SBO_ptr<
            details::RecvOperation,
            details::WithCallback<details::FixedRecvBuff>>
            (std::move(fn), buff, size, future);

        mBase->recvEnque(std::move(op));

        return future;
    }

    template<typename T>
    typename std::enable_if<std::is_pod<T>::value, void>::type
        Channel::asyncSend(const T* buffT, u64 sizeT)
    {
        u8* buff = (u8*)buffT;
        auto size = sizeT * sizeof(T);

        // less that 32 bit max
        assert(size < u32(-1));

        if (size == 0) return;

        auto op = make_SBO_ptr<
            details::SendOperation,
            details::FixedSendBuff>
            (buff, size);

        mBase->sendEnque(std::move(op));
    }



    template<typename T>
    typename std::enable_if<std::is_pod<T>::value, void>::type
        Channel::asyncSend(const T& v)
    {
        asyncSend(&v, 1);
    }



    template<class Container>
    typename std::enable_if<is_container<Container>::value, void>::type
        Channel::asyncSend(Container&& c, std::function<void()> callback)
    {
        // less that 32 bit max
        assert(channelBuffSize(c) < u32(-1));

        if (channelBuffSize(c) == 0)
        {
            callback();
            return;
        }

        auto op = make_SBO_ptr<
            details::SendOperation,
            details::WithCallback<
                details::MoveSendBuff<Container>
            >>(
                std::move(callback),
                std::forward<Container>(c));

        mBase->sendEnque(std::move(op));
    }

    template<class Container>
    typename std::enable_if<is_container<Container>::value, void>::type
        Channel::asyncSend(Container&& c, std::function<void(const error_code&)> callback)
    {
        // less that 32 bit max
        assert(channelBuffSize(c) < u32(-1));

        if (channelBuffSize(c) == 0)
        {
            callback(boost::system::errc::make_error_code(boost::system::errc::success));
            return;
        }

        auto op = make_SBO_ptr<
            details::SendOperation,
            details::WithCallback<
                details::MoveSendBuff<Container>
            >>(
                std::move(callback),
                std::forward<Container>(c));

        mBase->sendEnque(std::move(op));
    }


    template<typename T>
    typename std::enable_if<std::is_pod<T>::value, void>::type
        Channel::recv(T* buff, u64 size)
    {
        asyncRecv(buff, size).get();
    }

    template<typename T>
    typename std::enable_if<std::is_pod<T>::value, void>::type
        Channel::asyncSendCopy(const T* bufferPtr, u64 length)
    {
        std::vector<u8> bs((u8*)bufferPtr, (u8*)bufferPtr + length * sizeof(T));
        asyncSend(std::move(bs));
    }


    template<typename T>
    typename std::enable_if<std::is_pod<T>::value, void>::type
        Channel::asyncSendCopy(const T& buf)
    {
        asyncSendCopy(&buf, 1);
    }
}
#endif

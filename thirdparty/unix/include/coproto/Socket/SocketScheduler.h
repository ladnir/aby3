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
#include "coproto/Common/Queue.h"

#include "coproto/Proto/SessionID.h"
#include "coproto/Proto/Operation.h"

//#include "coproto/Proto/Buffers.h"
#include "coproto/Common/macoro.h"
#include "macoro/stop.h"

#include <mutex>
#include <atomic>
#include <cstring>
#include <unordered_map>
#include <list>

namespace coproto
{


    namespace internal
    {
        struct ExecutorRef
        {
            void* mScheduler = nullptr;
            function_view<void(void* scheduler, coroutine_handle<>)> mFn = nullptr;

            ExecutorRef() = default;
            ExecutorRef(ExecutorRef&&) = default;
            ExecutorRef& operator=(const ExecutorRef&) = default;

            ExecutorRef(const ExecutorRef& e)
                :mScheduler(e.mScheduler)
                , mFn(e.mFn)
            {
            }
            ExecutorRef(ExecutorRef& e)
                :mScheduler(e.mScheduler)
                , mFn(e.mFn)
            {
            }

            template<typename Scheduler>
            ExecutorRef(Scheduler& s)
                : mScheduler(&s)
                , mFn([](void* s, coroutine_handle<>h) {
                auto& scheduler = *(Scheduler*)s;
                scheduler.schedule(h);
                    })
            {}

                    operator bool() const
                    {
                        return mScheduler;
                    }

                    void operator()(coroutine_handle<>h)
                    {
                        mFn(mScheduler, h);
                    }
        };

        struct ExCoHandle
        {

            ExecutorRef mEx;
            coroutine_handle<> mH;

            void resume()
            {
                mEx.mFn(mEx.mScheduler, mH);
            }

        };

        // a list of callbacks with a small buffer optimization.
        template<typename T>
        struct CBQueue
        {
            CBQueue()
            {
                mVec = mArrayBacking;
            }
            CBQueue(CBQueue&& o)
            {
                *this = std::move(o);
            }

            CBQueue& operator=(CBQueue&& o)
            {
                mHead = (std::exchange(o.mHead, 0));
                mTail = (std::exchange(o.mTail, 0));
                if (o.mVec.data() == o.mArrayBacking.data())
                {
                    mArrayBacking = std::move(o.mArrayBacking);
                    mVec = mArrayBacking;
                }
                else
                {
                    COPROTO_ASSERT(o.mVec.data() == o.mVecBacking.data());
                    mVecBacking = std::move(o.mVecBacking);
                    mVec = mVecBacking;
                }

                o.mVec = o.mArrayBacking;
                return *this;
            }

            u64 mHead = 0;
            u64 mTail = 0;

            const u64 mLogTwoSize = 6;

            std::array<T, 8> mArrayBacking;
            std::vector<T> mVecBacking;
            span<T> mVec;

            void push_back(T h)
            {
                auto mask = mVec.size() - 1;

                if (mHead == mTail + mVec.size())
                {
                    std::vector<T> v(std::max<u64>(mVec.size() * 2, 1ull << mLogTwoSize));
                    if (mVec.size())
                    {
                        auto begin = mTail & mask;
                        auto end = mHead & mask;
                        for (u64 i = begin, j = 0; i != end; i = (i + 1) & mask, ++j)
                        {
                            v[j] = std::move(mVec[i]);
                        }

                        mVecBacking = std::move(v);
                        mVec = mVecBacking;
                        mHead = mHead - mTail;
                        mTail = 0;
                    }
                }
                //assert(mVec[mHead & mask] == nullptr);
                mVec[mHead++ & mask] = std::move(h);
            }

            u64 size() const { return mHead - mTail; }

            T pop_front()
            {
                if (size() == 0)
                    throw COPROTO_RTE;
                auto mask = mVec.size() - 1;
                return std::move(mVec[mTail++ & mask]);

            }

            operator bool() const
            {
                return size() > 0;
            }

            void run()
            {
                while (size())
                    pop_front().resume();
            }
        };

        struct DefaultExecutor
        {
            DefaultExecutor() = default;
            DefaultExecutor(const DefaultExecutor&) = delete;
            DefaultExecutor(DefaultExecutor&&) = delete;

            bool mHasRunner = false;
            CBQueue<coroutine_handle<>> mCBs;
            std::mutex* mMtx = nullptr;

            // this will try to acquired the callback queues.
            // if it does, it will hold onto them and run all
            // of the queued call backs. If more callbacks
            // are added while we are calling them, these
            // will also be run.
            struct Runner
            {
                DefaultExecutor* mEx = nullptr;
                CBQueue<coroutine_handle<>> mCBs;
                bool mAquired = false;


                Runner() = default;
                Runner(Runner&&) = default;
                Runner& operator=(Runner&&) = default;

                // check if no one else calling the callbacks and if so
                // take the callbacks.
                Runner(DefaultExecutor* e, std::unique_lock<std::mutex>& l)
                {
                    mEx = e;
                    //assert(l.mutex() == mEx->mMtx);
                    if (mEx->mHasRunner == false && mEx->mCBs.size() > 0)
                    {
                        mEx->mHasRunner = true;
                        mAquired = true;
                        mCBs = std::move(mEx->mCBs);
                    }
                }

                // run all of the callbacks that were acquired. 
                // once those are run, check if more have been added
                // and if so run those. repeat.
                MACORO_NODISCARD
                coroutine_handle<> run()
                {
                    assert(mEx->mMtx);
                    coroutine_handle<> next = nullptr;
                    while (mAquired)
                    {
                        while (mCBs.size())
                        {
                            auto h = mCBs.pop_front();

                            if (next)
                                next.resume();

                            next = h;

                            if (mCBs.size() == 0)
                            {
                                std::unique_lock<std::mutex> l(*mEx->mMtx);
                                std::swap(mEx->mCBs, mCBs);
                                mEx->mHasRunner = false;
                                mAquired = false;
                            }
                        }
                    }

                    if (next == nullptr)
                        next = macoro::noop_coroutine();

                    return next;
                }
            };

            Runner acquire(std::unique_lock<std::mutex>& l)
            {
                return Runner(this, l);
            }

        };

        // This class handles the logic with managing what message to
        // send and what fork/slot it corresponds to. Recall that we support running
        // multiple (semi) independent protocols on a single underlaying socket.
        // This is achieved by a small header to each message sent that can be used to
        // figure out who should receive any given message. This header will consist of
        // the size of the message and an id indicating what protocol this message 
        // corresponds to.
        // 
        // There are two types of messages:
        // 
        // Data - these messages have the format  [msg-size:32, slot-id:32, msg].
        //   * msg-size is a non-zero 32 bit value denoting the length in bytes of the message.
        //   * slot-id is a 32 bit value that is used to identify which slot this message corresponds to.
        //     This id should have previously been initialized, see below.
        //   * msg is the actual data of the message. This will consist of msg-size bytes.
        // 
        // Meta - these messages have the format [ zero:32, slot-id:32, meta-data]. 
        //   These messages are used to initialize new slots and other socket internal state.
        //   * zero is always value 0 and 32 bits long. This allows meta message to be 
        //     distinguished from data messages, which always start with a non-zero message.
        //   * slot-id is a 32 bit value to identify the slot-id that this meta message corresponds to.
        //   * meta-data is the data associated with this meta message. Currently there is only
        //     one option for this. In particular, the only meta message currently supported is 
        //     to create a new slot. This is done by sending a new/unused value for slot-id and the have
        //     meta-data be the 128-bit session ID corresponding to this slot/fork. Note that each party
        //     may associate a different slot-id with the same session ID. 
        // 
        //     Each fork/slot is associated with a unique/random-ish session ID. Instead of sending the 128 bit
        //     session ID with each message, we associate the session ID with a 32 bit slot-id.
        //     It is then sufficient. to only send the slot-id
        //   
        //
        // Whenever the first message is sent on a slot/fork, we must first initialize
        // the slot by sending a meta message as described above.
        // 
        // When a party requests to receive a message on a given fork/slot, we check 
        // if we have previously read the header corresponding to this receive. This
        // might happen if multiple slots have requested a receive. If we haven't
        // received the header, then first we to receive the header. If the received 
        // header is not for this slot then we will just suspend the receive task
        // until the user asks for the receive on that slot.
        // 
        // Another complicating detail is that we allow the user to move a buffer when 
        // sending it. In this case the async send operation will appear to complete
        // synchronously to the user. However, in reality it has just been buffered by
        // SockScheduler. As such, this can lead to the situation that the user may think
        // everything is done and destroy the socket. In this case we run into issue due to 
        // an async send still pending. As a workaround, we allow the user to "flush" the 
        // socket which will suspend the user until all messages have been sent.
        // 
        // Another complications is that the user can cancel send/recv operations.
        // In the event that one message is canceled, the user is still allowed to
        // send and receive messages on the socket. However, in the event that a message
        // is half sent we are forced to send the whole message because the receiver is 
        // expecting the whole message. One could partially fix this by breaking messages up  
        // into small chunks but the implementation does not do this. Instead, if the user
        // requests a cancel on a send, the operation is immediately canceled (assuming the 
        // underlaying socket cooperates). Later, if the message was half sent and the user
        // want to send some other message, the first message must be completed first.
        // 
        struct SockScheduler
        {
            using Lock = std::unique_lock<std::mutex>;
            struct Slot;
            using SlotIter = std::list<Slot>::iterator;

            struct FlushToken
            {

                FlushToken(coroutine_handle<> h) :mHandle(h) {
                    assert(mHandle);
                }
                coroutine_handle<> mHandle;
                ~FlushToken()
                {
                    assert(!mHandle);
                }
            };


            struct RecvOperation
            {
                RecvOperation(RecvBuffer* r, coroutine_handle<void> ch, SlotIter s, macoro::stop_token&& token)
                    : mSlot(s)
                    , mCH(ch)
                    , mRecvBuffer(r)
                    , mToken(token)
                {
                    mReg.emplace(mToken, [this] {

                        CBQueue<coroutine_handle<>> cb;
                        CBQueue<ExCoHandle> xcb;

                        macoro::stop_source cancelSrc;
                        {
                            auto& s = *mSlot->mSched;
                            Lock l(s.mMutex);
                            if (mInProgress == false)
                            {
                                mRecvBuffer->setError(std::make_exception_ptr(std::system_error(code::operation_aborted)));
                                COPROTO_ASSERT(mCH);
                                getCB(cb, xcb, l);
                                mSlot->mRecvOps2_.erase(mSelfIter);

                                COPROTO_ASSERT(s.mNumRecvs);
                                --s.mNumRecvs;
                                if (s.mNumRecvs == 0)
                                {
                                    s.mRecvStatus = Status::Idle;
                                }
                            }
                            else if (s.mRecvCancelSrc.stop_possible())
                            {
                                cancelSrc = std::move(s.mRecvCancelSrc);
                            }
                        }

                        while (xcb)
                            xcb.pop_front().resume();
                        while (cb)
                            cb.pop_front().resume();
                        if (cancelSrc.stop_possible())
                            cancelSrc.request_stop();

                        });
                }


                SlotIter mSlot;
                coroutine_handle<void> mCH;
                RecvBuffer* mRecvBuffer = nullptr;
                bool mInProgress = false;
                std::list<RecvOperation>::iterator mSelfIter;

                macoro::stop_token mToken;
                optional<macoro::stop_callback> mReg;

                std::vector<std::shared_ptr<FlushToken>> mFlushes;

                macoro::stop_token& cancellationToken()
                {
                    return mToken;
                }

                void getCB(CBQueue<coroutine_handle<>>& cbs, CBQueue<ExCoHandle>& xcbs, std::unique_lock<std::mutex>&)
                {
                    if (mSlot->mExecutor)
                        xcbs.push_back(ExCoHandle{ mSlot->mExecutor, std::exchange(mCH, nullptr) });
                    else
                        cbs.push_back(std::exchange(mCH, nullptr));

                    for (auto& f : mFlushes)
                    {
                        if (f.use_count() == 1)
                        {
                            if (mSlot->mExecutor)
                                xcbs.push_back(ExCoHandle{ mSlot->mExecutor, std::exchange(f->mHandle, nullptr) });
                            else
                                cbs.push_back(std::exchange(f->mHandle, nullptr));
                        }
                    }
                }
            };

            struct SendOperation
            {
                SendOperation() = delete;
                SendOperation(const SendOperation&) = delete;
                SendOperation(SendOperation&&) = delete;

                SendOperation(SlotIter ss, coroutine_handle<void> ch, SendBuffer&& s, macoro::stop_token&& token)
                    : mSlot(ss)
                    , mCH(ch)
                    , mSendBuff(std::move(s))
                    , mToken(std::move(token))
                {
                    if (mToken.stop_possible())
                    {
                        mReg.emplace(mToken, [this] {

                            CBQueue<coroutine_handle<>> cb;
                            CBQueue<ExCoHandle> xcb;
                            macoro::stop_source cancelSrc;
                            {
                                auto& s = *mSlot->mSched;
                                Lock l(s.mMutex);
                                if (mInProgress == false)
                                {
                                    mSendBuff.setError(std::make_exception_ptr(std::system_error(code::operation_aborted)));
                                    COPROTO_ASSERT(mCH);
                                    getCB(cb, xcb, l);

                                    auto& queue = s.mSendBuffers_;
                                    auto iter = std::find(queue.begin(), queue.end(), mSlot);
                                    COPROTO_ASSERT(iter != queue.end());
                                    queue.erase(iter);

                                    mSlot->mSendOps2_.erase(mSelfIter);

                                    if (queue.size() == 0)
                                    {
                                        s.mSendStatus = Status::Idle;
                                    }
                                }
                                else if (s.mSendCancelSrc.stop_possible())
                                {
                                    cancelSrc = std::move(s.mSendCancelSrc);
                                }
                            }

                            while (xcb)
                                xcb.pop_front().resume();
                            while (cb)
                                cb.pop_front().resume();
                            if (cancelSrc.stop_possible())
                                cancelSrc.request_stop();
                            });
                    }
                }

                // the buffer that should be sent out of.
                coroutine_handle<void> mCH = nullptr;
                SendBuffer mSendBuff;
                bool mInProgress = false;

                SlotIter mSlot;
                std::list<SendOperation>::iterator mSelfIter;

                macoro::stop_token mToken;
                optional<macoro::stop_callback> mReg;
                std::vector<std::shared_ptr<FlushToken>> mFlushes;

                macoro::stop_token& cancellationToken()
                {
                    return mToken;
                }

                void getCB(CBQueue<coroutine_handle<>>& cbs, CBQueue<ExCoHandle>& xcbs, std::unique_lock<std::mutex>&)
                {
                    if (mSlot->mExecutor)
                        xcbs.push_back(ExCoHandle{ mSlot->mExecutor, std::exchange(mCH, nullptr) });
                    else
                        cbs.push_back(std::exchange(mCH, nullptr));

                    for (auto& f : mFlushes)
                    {
                        if (f.use_count() == 1)
                        {

                            if (mSlot->mExecutor)
                                xcbs.push_back(ExCoHandle{ mSlot->mExecutor, std::exchange(f->mHandle, nullptr) });
                            else
                                cbs.push_back(std::exchange(f->mHandle, nullptr));

                        }
                    }
                }
            };


            using Header = std::array<u32, 2>;
            struct ControlBlock
            {
                std::array<u8, 16> data;
                enum class Type : u8
                {
                    NewSlot = 1
                };

                Type getType() { return Type::NewSlot; }
                SessionID getSessionID() {
                    SessionID ret;
                    memcpy(ret.mVal, data.data(), 16);
                    return ret;
                }

                void setType(Type t) {};
                void setSessionID(const SessionID& id) {
                    memcpy(data.data(), id.mVal, 16);
                }
            };

            struct SendControlBlock
            {
                Header mHeader;
                ControlBlock mCtrlBlk;
            };

            struct Slot
            {
                Slot(SockScheduler* s) : mSched(s) {}
                SockScheduler* mSched;
                SessionID mSessionID;
                u32 mLocalId = -1, mRemoteId = -1;
                bool mInitiated = false;
                bool mClosed = false;
                ExecutorRef mExecutor;
                std::string mName;

                std::list<RecvOperation> mRecvOps2_;
                std::list<SendOperation> mSendOps2_;
            };


            enum class Status
            {
                Idle,
                InUse,
                RequestedRecvOp,
                Closed

            };



            struct NextSendOp
            {
                NextSendOp(SockScheduler* ss, error_code ee) : s(ss), ec(ee) {}
                SockScheduler* s;
                error_code ec;

                bool await_ready();
#ifdef COPROTO_CPP20
                std::coroutine_handle<> await_suspend(std::coroutine_handle<> h);
#endif
                coroutine_handle<> await_suspend(coroutine_handle<> h);
                SlotIter await_resume();
            };


            struct anyRecvOp
            {
                anyRecvOp(SockScheduler* ss, error_code);
                SockScheduler* s;
                error_code ec;

                bool await_ready();
#ifdef COPROTO_CPP20
                std::coroutine_handle<> await_suspend(std::coroutine_handle<>h);
#endif
                coroutine_handle<> await_suspend(coroutine_handle<>h);
                void await_resume() {}
            };


            optional<SockScheduler::SlotIter> mRequestedRecvSlot;
            struct getRequestedRecvSlot
            {
                getRequestedRecvSlot(SockScheduler* ss) : s(ss) {}

                SockScheduler* s;
                bool await_ready();
#ifdef COPROTO_CPP20
                std::coroutine_handle<> await_suspend(std::coroutine_handle<> h);
#endif
                coroutine_handle<> await_suspend(coroutine_handle<>h);

                optional<SockScheduler::SlotIter> await_resume();

            };



            unique_function<void(void)> mCloseSock;
            AnyNoCopy mSockStorage;
            std::list<Slot> mSlots_;
            u32 mNextLocalSlot = 1;

            // maps SessionID to slot index.
            std::unordered_map<SessionID, SlotIter> mIdSlotMapping_;

            // maps their slot index to SessionID
            std::unordered_map<u32, SlotIter> mRemoteSlotMapping_;

            std::mutex mMutex;

            std::list<SlotIter> mSendBuffers_;

            error_code mEC;
            u64 mNumRecvs = 0;
            std::atomic_bool mClosed;
            Status mRecvStatus = Status::Idle, mSendStatus = Status::Idle;
            Header mSendHeader = {}, mRecvHeader = {};
            ControlBlock mRecvControlBlock, mSendControlBlock;
            SendControlBlock mSendControlBlock2;
            bool mHaveRecvHeader = false;
            u64 mBytesSent = 0, mBytesReceived = 0;

            coroutine_handle<> mRecvTaskHandle, mSendTaskHandle;
            macoro::eager_task<void> mSendTask;
            macoro::eager_task<void> mRecvTask;


            DefaultExecutor mDefaultEx;


            void resetRecvToken()
            {
                assert(mInitializing || mRecvCancelSrc.stop_possible() == false);

                if (!mInitializing)
                    mRecvCancelSrc = macoro::stop_source();
                mRecvToken = mRecvCancelSrc.get_token();
            }
            void resetSendToken()
            {
                assert(mInitializing || mSendCancelSrc.stop_possible() == false);
                if (!mInitializing)
                    mSendCancelSrc = macoro::stop_source();

                mSendToken = mSendCancelSrc.get_token();
            }

            macoro::stop_token mRecvToken, mSendToken;
            macoro::stop_source mRecvCancelSrc, mSendCancelSrc;

            void* mSockPtr = nullptr;

            bool mInitializing = true;

            void* getSocket()
            {
                return mSockPtr;
            }

            template<typename SocketImpl>
            void init(SocketImpl* sock, SessionID sid);

            template<typename SocketImpl>
            SockScheduler(SocketImpl&& s, SessionID sid);

            template<typename SocketImpl>
            SockScheduler(SocketImpl& s, SessionID sid);

            template<typename SocketImpl>
            SockScheduler(std::unique_ptr<SocketImpl>&& s, SessionID sid);


            ~SockScheduler()
            {
                if (mRecvStatus == Status::InUse || mSendStatus == Status::InUse)
                {
                    std::cout << "Socket was destroyed with pending operations. "
                        << "terminate() is being called. Await Socket::flush() "
                        << "before the destructor is called. This will ensure that"
                        << "all pending operations complete. " << MACORO_LOCATION << std::endl;
                    std::terminate();
                }
            }

            template<typename Sock>
            macoro::eager_task<void> makeSendTask(Sock* socket);

#ifdef COPROTO_SOCK_LOGGING
            std::vector<const char*> mRecvLog, mSendLog;
#endif
            template<typename Sock>
            macoro::eager_task<void> receiveDataTask(Sock* socket);

            u32& getSendHeaderSlot();
            u32& getSendHeaderSize();
            span<u8> getSendHeader();
            optional<SlotIter> getRecvHeaderSlot(Lock& _);
            u32 getRecvHeaderSize();
            span<u8> getRecvHeader();
            span<u8> getSendCtrlBlk();
            span<u8> getSendCtrlBlk2();


            SlotIter getLocalSlot(const SessionID& id, Lock& _);

            void initLocalSlot(const SessionID& id, const ExecutorRef& ex, Lock& _);
            error_code initRemoteSlot(u32 slotId, SessionID id, Lock& _);

            SessionID fork(SessionID s);

            MACORO_NODISCARD
                coroutine_handle<void> send(SessionID id, SendBuffer&& op, coroutine_handle<void> callback, macoro::stop_token&& token, bool async = false);
            MACORO_NODISCARD
                coroutine_handle<void> recv(SessionID id, RecvBuffer* data, coroutine_handle<void> ch, macoro::stop_token&& token);


            enum class Caller { Sender, Recver, Extern };
            void close(CBQueue<ExCoHandle>& xcbs, Caller c, bool& closeSock, error_code, Lock&);
            void close();
            void closeFork(SessionID sid);


            coroutine_handle<> flush(coroutine_handle<>h);

            bool mLogging = false;
            void enableLogging()
            {
                mLogging = true;
            }

            void disableLogging()
            {
                mLogging = false;
            }

            template<typename Scheduler>
            void setExecutor(Scheduler& scheduler, SessionID id)
            {
                Lock lock(mMutex);
                auto iter = getLocalSlot(id, lock);
                iter->mExecutor = ExecutorRef(scheduler);
            }

        };

        template<typename SocketImpl>
        void SockScheduler::init(SocketImpl* sock, SessionID sid)
        {
            mSockPtr = sock;
            mRecvTask = receiveDataTask(sock);
            mDefaultEx.mMtx = &mMutex;
            mSendTask = makeSendTask(sock);

            mCloseSock = [this, sock] {sock->close(); };

            resetRecvToken();
            resetSendToken();

            Lock l;
            initLocalSlot(sid, {}, l);

#ifdef COPROTO_SOCK_LOGGING
            mRecvLog.reserve(1000);
            mSendLog.reserve(1000);
#endif
            mInitializing = false;

        }

        template<typename SocketImpl>
        SockScheduler::SockScheduler(SocketImpl&& s, SessionID sid)
            : mClosed(false)
        {
            auto ss = mSockStorage.emplace(std::move(s));
            init(ss, sid);
        }

        template<typename SocketImpl>
        SockScheduler::SockScheduler(SocketImpl& s, SessionID sid)
            : mClosed(false)
        {
            init(&s, sid);
        }

        template<typename SocketImpl>
        SockScheduler::SockScheduler(std::unique_ptr<SocketImpl>&& s, SessionID sid)
            : mClosed(false)
        {
            auto ss = mSockStorage.emplace(std::move(s));
            init(ss->get(), sid);
        }


        template<typename Sock>
        macoro::eager_task<void> SockScheduler::receiveDataTask(Sock* sock)
        {
            MC_BEGIN(macoro::eager_task<void>, this, sock
                , ec = error_code{}
                , bt = u64{}
                , buffer = span<u8>{}
                , iter = optional<SockScheduler::SlotIter>{}
                , slot = SockScheduler::SlotIter{}
                , op = (RecvOperation*)nullptr
                , restoreReadSize = u64{ 0 }
                , restoreBuffer = std::vector<u8>{}
            );


            while (true)
            {
                MC_AWAIT(anyRecvOp(this, ec));
#ifdef COPROTO_SOCK_LOGGING
                mRecvLog.push_back("new-recv");
#endif


                if (restoreReadSize)
                {
#ifdef COPROTO_SOCK_LOGGING
                    mRecvLog.push_back("restore");
#endif
                    restoreBuffer.resize(restoreReadSize);
                    buffer = restoreBuffer;
                    MC_AWAIT_SET(std::tie(ec, bt), sock->recv(buffer));
                    mBytesReceived += bt;
                    assert((static_cast<bool>(ec) ^ (bt == buffer.size())) && (bt <= buffer.size()));

                    if (ec)
                        continue;
                }


                while (!mHaveRecvHeader)
                {
#ifdef COPROTO_SOCK_LOGGING
                    mRecvLog.push_back("header");
#endif
                    buffer = getRecvHeader();
                    MC_AWAIT_SET(std::tie(ec, bt), sock->recv(buffer));
                    mBytesReceived += bt;
                    assert((static_cast<bool>(ec) ^ (bt == buffer.size())) && (bt <= buffer.size()));

                    if (ec)
                        break;

                    if (getRecvHeaderSize() == 0)
                    {
#ifdef COPROTO_SOCK_LOGGING
                        mRecvLog.push_back("header-meta");
#endif
                        buffer = span<u8>((u8*)&mRecvControlBlock, sizeof(mRecvControlBlock));
                        MC_AWAIT_SET(std::tie(ec, bt), sock->recv(buffer));
                        mBytesReceived += bt;
                        assert((static_cast<bool>(ec) ^ (bt == buffer.size())) && (bt <= buffer.size()));

                        if (ec)
                            break;

                        auto slotId = mRecvHeader[1];
                        auto sid = mRecvControlBlock.getSessionID();
                        auto lock = SockScheduler::Lock(mMutex);
                        ec = initRemoteSlot(slotId, sid, lock);
                        if (ec)
                            break;

                    }
                    else
                    {
                        mHaveRecvHeader = true;
                    }
                }
                if (ec)
                    continue;

#ifdef COPROTO_SOCK_LOGGING
                mRecvLog.push_back("getRequestedRecvSlot-enter");
#endif
                MC_AWAIT_SET(iter, getRequestedRecvSlot{ this });
                mHaveRecvHeader = false;

                if (mClosed) {
                    ec = code::closed;
                    continue;
                }

                if (!iter)
                {
                    ec = code::badCoprotoMessageHeader;
                    continue;
                }

                assert(iter);
                slot = *iter;
                op = &slot->mRecvOps2_.front();
                buffer = op->mRecvBuffer->asSpan(getRecvHeaderSize());

                if (buffer.size() != getRecvHeaderSize())
                {
                    ec = code::cancel;
                    continue;
                }

                MC_AWAIT_SET(std::tie(ec, bt), sock->recv(buffer, mRecvToken));
                mBytesReceived += bt;
                assert((static_cast<bool>(ec) ^ (bt == buffer.size())) && (bt <= buffer.size()));

                if (ec == code::operation_aborted)
                {
                    COPROTO_ASSERT(op == &slot->mRecvOps2_.front());
                    restoreReadSize = buffer.size() - bt;
                    op->mRecvBuffer->setError(std::make_exception_ptr(std::system_error(ec)));
                    continue;
                }

                if (ec)
                    continue;
            }

            MC_AWAIT(macoro::suspend_always{});

            MC_END();
        }



        template<typename Sock>
        macoro::eager_task<void> SockScheduler::makeSendTask(Sock* sock)
        {
            MC_BEGIN(macoro::eager_task<void>, this, sock
                , ec = error_code{}
                , bt = u64{}
                , restoreBuffer = std::vector<u8>{}
                , iter = optional<SockScheduler::SlotIter>{}
                , slot = SlotIter{}
                , data = span<u8>{}
                , buffer = span<u8>{}
                , op = (SendOperation*)nullptr
            );
            while (true)
            {

                MC_AWAIT_SET(iter, NextSendOp(this, ec));
#ifdef COPROTO_SOCK_LOGGING
                mSendLog.push_back("new-send");
#endif

                assert(iter);
                slot = *iter;
                op = &slot->mSendOps2_.front();
                data = op->mSendBuff.asSpan();
                COPROTO_ASSERT(data.size() != 0);
                COPROTO_ASSERT(data.size() < std::numeric_limits<u32>::max());
                COPROTO_ASSERT(slot->mLocalId != ~u32(0));


                if (restoreBuffer.size())
                {
#ifdef COPROTO_SOCK_LOGGING
                    mSendLog.push_back("restore");
#endif

                    buffer = restoreBuffer;
                    MC_AWAIT_SET(std::tie(ec, bt), sock->send(buffer, mSendToken));
                    mBytesSent += bt;

                    // we must either 
                    // - finish successfully (!ec) and all bytes sent 
                    // - fail with an error and not all bytes sent.
                    assert((static_cast<bool>(ec) ^ (bt == buffer.size())) && (bt <= buffer.size()));

                    if (ec == code::operation_aborted)
                    {
                        assert(mSendToken.stop_requested());

                        auto rem = buffer.subspan(bt);
                        // save what we have left to send.
                        std::copy(rem.begin(), rem.end(), restoreBuffer.begin());
                        restoreBuffer.resize(rem.size());
                    }
                    if (ec)
                        continue;

                    restoreBuffer.clear();
                }



                if (slot->mInitiated == false)
                {
#ifdef COPROTO_SOCK_LOGGING
                    mSendLog.push_back("meta");
#endif

                    slot->mInitiated = true;
                    mSendControlBlock2.mHeader[0] = 0;
                    mSendControlBlock2.mHeader[1] = slot->mLocalId;
                    mSendControlBlock2.mCtrlBlk.setType(ControlBlock::Type::NewSlot);
                    mSendControlBlock2.mCtrlBlk.setSessionID(slot->mSessionID);

                    //std::cout << "sending open " << this << std::endl;
                    buffer = getSendCtrlBlk2();
                    MC_AWAIT_SET(std::tie(ec, bt), sock->send(buffer, mSendToken));
                    mBytesSent += bt;
                    assert((static_cast<bool>(ec) ^ (bt == buffer.size())) && (bt <= buffer.size()));

                    if (ec == code::operation_aborted)
                    {
                        assert(mSendToken.stop_requested());
                        auto rem = buffer.subspan(bt);

                        if (bt) {
                            // we have already sent some bytes, these are the bytes we need
                            // to send if we want to put the socket back into a good state.
                            restoreBuffer.insert(restoreBuffer.end(), rem.begin(), rem.end());
                        }
                    }
                    if (ec)
                        continue;

                }

                getSendHeaderSlot() = slot->mLocalId;
                getSendHeaderSize() = static_cast<u32>(data.size());
                //std::cout << "sending header " << this << std::endl;
#ifdef COPROTO_SOCK_LOGGING
                mSendLog.push_back("header");
#endif

                buffer = getSendHeader();
                MC_AWAIT_SET(std::tie(ec, bt), sock->send(buffer, mSendToken));
                mBytesSent += bt;
                assert((static_cast<bool>(ec) ^ (bt == buffer.size())) && (bt <= buffer.size()));

                if (ec == code::operation_aborted)
                {
                    assert(mSendToken.stop_requested());
                    auto rem = buffer.subspan(bt);
                    // save what we have left to send.

                    if (bt) {
                        // we have already sent some bytes, these are the bytes we need
                        // to send if we want to put the socket back into a good state.
                        restoreBuffer.insert(restoreBuffer.end(), rem.begin(), rem.end());
                        restoreBuffer.insert(restoreBuffer.end(), data.begin(), data.end());
                    }
                }
                if (ec)
                    continue;
                //std::cout << "sending body " << this << std::endl;

#ifdef COPROTO_SOCK_LOGGING
                mSendLog.push_back("body");
#endif
                MC_AWAIT_SET(std::tie(ec, bt), sock->send(data, mSendToken));
                mBytesSent += bt;
                assert((static_cast<bool>(ec) ^ (bt == data.size())) && (bt <= data.size()));

                if (ec == code::operation_aborted)
                {
                    assert(mSendToken.stop_requested());
                    auto rem = data.subspan(bt);
                    // save what we have left to send.

                    if (bt) {
                        // we have already sent some bytes, these are the bytes we need
                        // to send if we want to put the socket back into a good state.
                        restoreBuffer.insert(restoreBuffer.end(), rem.begin(), rem.end());
                    }
                }
                if (ec)
                    continue;
            }

            MC_AWAIT(macoro::suspend_always{});

            MC_END();
        }


    }
}
#pragma once
#include <vector>
#include <thread>
#include <mutex>
#include <deque>
#include <unordered_set>

#include "macoro/coroutine_handle.h"
#include "macoro/awaiter.h"
#include "stop.h"
#include <algorithm>
#include <sstream>
#include <utility>
#include <condition_variable>

namespace macoro
{
    namespace detail
    {
        using thread_pool_clock = std::chrono::steady_clock;
        using thread_pool_time_point = std::chrono::time_point<thread_pool_clock>;
        struct thread_pool_delay_op
        {
            thread_pool_delay_op() = default;
            thread_pool_delay_op(const thread_pool_delay_op&) = default;
            thread_pool_delay_op(thread_pool_delay_op&&) = default;
            thread_pool_delay_op& operator=(const thread_pool_delay_op&) = default;
            thread_pool_delay_op& operator=(thread_pool_delay_op&&) = default;


            thread_pool_delay_op(std::size_t i, coroutine_handle<> h, thread_pool_time_point p)
                : idx(i)
                , handle(h)
                , deadline(p)
            {}

            std::size_t idx;
            coroutine_handle<> handle;
            thread_pool_time_point deadline;

            bool operator<(const thread_pool_delay_op& o) const { return deadline > o.deadline; }
        };



        struct thread_pool_state
        {
            std::mutex              mMutex;
            std::condition_variable mCondition;

            std::size_t mWork = 0;
            static thread_local thread_pool_state* mCurrentExecutor;

            std::deque<coroutine_handle<void>> mDeque;
            //struct LE
            //{
            //	LE(const char* s, thread_pool_time_point t)
            //		: str(s)
            //		, time(t)
            //	{}
            //	const char* str;
            //	thread_pool_time_point time;
            //};
            //std::vector<LE> mLog;

            //void log(const char* l)
            //{
            //	mLog.emplace_back(l, thread_pool_clock::now());
            //}

            std::vector<thread_pool_delay_op> mDelayHeap;
            std::size_t mDelayOpIdx = 0;
            std::vector<std::thread> mThreads;

            void post(coroutine_handle<void> fn)
            {
                //log("post");
                assert(fn);
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    mDeque.push_back(std::move(fn));
                }
                mCondition.notify_one();
            }

            MACORO_NODISCARD
                bool try_dispatch(coroutine_handle<void> fn)
            {
                //log("try_dispatch");
                if (mCurrentExecutor == this)
                    return true;
                else
                {
                    {
                        std::lock_guard<std::mutex> lock(mMutex);
                        mDeque.push_back(std::move(fn));
                    }

                    mCondition.notify_one();
                    return false;
                }
            }

            void post_after(
                coroutine_handle<> h,
                thread_pool_time_point deadline,
                stop_token&& token,
                optional_stop_callback& reg
            )
            {
                if (token.stop_requested() == false)
                {
                    std::size_t idx;
                    {
                        std::unique_lock<std::mutex> lock(mMutex);
                        idx = mDelayOpIdx++;
                        mDelayHeap.emplace_back(idx, h, deadline);
                        std::push_heap(mDelayHeap.begin(), mDelayHeap.end());
                    }

                    if (token.stop_possible())
                    {
                        reg.emplace(std::move(token), [idx, this] {
                            cancel_delay_op(idx);
                            });
                    }
                    mCondition.notify_one();
                }
                else
                {
                    post(h);
                }
            }


            void cancel_delay_op(std::size_t idx)
            {
                //log("cancel_delay_op");
                bool notify = false;
                {
                    std::unique_lock<std::mutex> lock(mMutex);
                    for (std::size_t i = 0; i < mDelayHeap.size(); ++i)
                    {
                        if (mDelayHeap[i].idx == idx)
                        {
                            mDelayHeap[i].deadline = thread_pool_clock::now();
                            std::make_heap(mDelayHeap.begin(), mDelayHeap.end());
                            notify = true;
                            break;
                        }
                    }
                }
                if (notify)
                    mCondition.notify_one();
            }

        };


        struct thread_pool_post
        {
            thread_pool_state* mPool;

            bool await_ready() const noexcept { return false; }

            template<typename H>
            void await_suspend(H h) const {
                mPool->post(coroutine_handle<void>(h));
            }

            void await_resume() const noexcept {}
        };


        struct thread_pool_dispatch
        {
            detail::thread_pool_state* pool;

            bool await_ready() const noexcept { return false; }

            template<typename H>
            auto await_suspend(const H& h)const {
                using traits_C = coroutine_handle_traits<H>;
                using return_type = typename traits_C::template coroutine_handle<void>;
                if (pool->try_dispatch(coroutine_handle<void>(h)))
                    return return_type(h);
                else
                    return static_cast<return_type>(noop_coroutine());
            }

            void await_resume() const noexcept {}
        };

        struct thread_pool_post_after
        {
            thread_pool_state* mPool;
            thread_pool_time_point mDeadline;
            stop_token mToken;
            optional_stop_callback mReg;

            bool await_ready() const noexcept { return false; }

            template<typename H>
            void await_suspend(const H& h) {
                mPool->post_after(coroutine_handle<void>(h), mDeadline, std::move(mToken), mReg);
            }

            void await_resume() const noexcept {}
        };
    }

    class thread_pool
    {
    public:
        using clock = detail::thread_pool_clock;

        std::unique_ptr<detail::thread_pool_state> mState;

        struct work
        {
            detail::thread_pool_state* mEx = nullptr;

            work(detail::thread_pool_state* e)
                : mEx(e)
            {
                std::lock_guard<std::mutex> lock(mEx->mMutex);
                ++mEx->mWork;
            }

            work() = default;
            work(const work&) = delete;
            work(work&& w) : mEx(std::exchange(w.mEx, nullptr)) {}
            work& operator=(const work&) = delete;
            work& operator=(work&& w) {
                reset();
                mEx = std::exchange(w.mEx, nullptr);
                return *this;
            }

            void reset()
            {

                if (mEx)
                {

                    std::size_t v = 0;
                    {
                        std::lock_guard<std::mutex> lock(mEx->mMutex);
                        v = --mEx->mWork;
                    }
                    if (v == 0)
                    {
                        mEx->mCondition.notify_all();
                    }
                    mEx = nullptr;
                }
            }
            ~work()
            {
                reset();
            }
        };


        thread_pool()
            :mState(new detail::thread_pool_state)
        {}
        thread_pool(thread_pool&&) = default;
        thread_pool& operator=(thread_pool&&) = default;

        thread_pool(std::size_t number_of_threads, work& w)
            : mState(new detail::thread_pool_state)
        {
            w = make_work();
            create_threads(number_of_threads);
        }

        ~thread_pool()
        {
            join();
        }


        detail::thread_pool_post schedule()
        {
            return { mState.get() };
        }


        detail::thread_pool_post post()
        {
            return { mState.get() };
        }


        detail::thread_pool_dispatch dispatch()
        {
            return { mState.get() };
        }

        void schedule(coroutine_handle<void> fn)
        {
            mState->post(fn);
        };

        void post(coroutine_handle<void> fn)
        {
            mState->post(fn);
        };

        coroutine_handle<void> dispatch(coroutine_handle<void> fn)
        {
            if (mState->try_dispatch(fn))
                return fn;
            else
                return noop_coroutine();
        };

        template<typename Rep, typename Per>
        detail::thread_pool_post_after schedule_after(
            std::chrono::duration<Rep, Per> delay,
            stop_token token = {})
        {
            return { mState.get(), delay + clock::now(), std::move(token) };
        }

        work make_work() {
            return { mState.get() };
        }

        void create_threads(std::size_t n)
        {
            std::unique_lock<std::mutex> lock(mState->mMutex);
            if (mState->mWork || mState->mDeque.size() || mState->mDelayHeap.size())
            {
                mState->mThreads.reserve(mState->mThreads.size() + n);
                for (std::size_t i = 0; i < n; ++i)
                {
                    mState->mThreads.emplace_back([this] {run(); });
                }
            }
        }

        void create_thread()
        {
            create_threads(1);
        }

        void join()
        {
            if (mState)
            {

                std::vector<std::thread> thrds;
                {
                    std::unique_lock<std::mutex> lock(mState->mMutex);
                    thrds = std::move(mState->mThreads);
                }

                for (auto& t : thrds)
                    t.join();
            }
        }

        //std::string print_log(detail::thread_pool_time_point begin)
        //{
        //	std::stringstream ss;
        //	for (int i = 0; i < mState->mLog.size(); ++i)
        //	{
        //		ss << std::chrono::duration_cast<std::chrono::milliseconds>(mState->mLog[i].time - begin).count()
        //			<< " ms ~ " << mState->mLog[i].str << std::endl;
        //			
        //	}

        //	return ss.str();
        //}

        void run()
        {
            auto state = mState.get();
            //state->log("run");

            if (detail::thread_pool_state::mCurrentExecutor != nullptr)
                throw std::runtime_error("calling run() on a thread that is already controlled by a thread_pool is not supported. ");
            detail::thread_pool_state::mCurrentExecutor = state;

            coroutine_handle<void> fn;

            {
                std::unique_lock<std::mutex> lock(state->mMutex);
                while (
                    state->mWork ||
                    state->mDeque.size() ||
                    state->mDelayHeap.size())
                {

                    if ((state->mDelayHeap.empty() || state->mDelayHeap.front().deadline > clock::now()) &&
                        state->mDeque.empty())
                    {
                        //state->log("run::no-work");

                        if (state->mDelayHeap.size())
                        {
                            state->mCondition.wait_until(lock, state->mDelayHeap.front().deadline);
                        }
                        else
                        {
                            state->mCondition.wait(lock, [&] {
                                // wake up when theres something in the
                                //  queue, or state->state->mWork == 0
                                return
                                    state->mDeque.size() ||
                                    state->mDelayHeap.size() ||
                                    state->mWork == 0;
                                });
                        }
                    }

                    if (state->mDelayHeap.size() && state->mDelayHeap.front().deadline <= clock::now())
                    {
                        //state->log("run::pop-delay");

                        fn = state->mDelayHeap.front().handle;
                        std::pop_heap(state->mDelayHeap.begin(), state->mDelayHeap.end());
                        state->mDelayHeap.pop_back();
                    }
                    else if (state->mDeque.size())
                    {
                        //state->log("run::pop");
                        fn = state->mDeque.front();
                        state->mDeque.pop_front();
                    }

                    if (fn)
                    {
                        lock.unlock();

                        fn.resume();
                        fn = {};

                        lock.lock();
                    }
                    //state->log("run::next");
                }
            }

            detail::thread_pool_state::mCurrentExecutor = nullptr;
        }


    };

}
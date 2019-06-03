#pragma once
#include "aby3/Common/Defines.h"
#include "aby3/Engines/sh3/Sh3Types.h"
#include <cryptoTools/Network/Session.h>
#include <boost/variant.hpp>
#include <function2/function2.hpp>

namespace aby3
{

    class Sh3Runtime;

    class Sh3Task
    {
    public:

        using RoundFunc = fu2::unique_function<void(CommPkg& comm, Sh3Task& self)>;
        using ContinuationFunc = fu2::unique_function<void(Sh3Task& self)>;

        // returns the associated runtime.
        Sh3Runtime& getRuntime() { return *mRuntime; }

        // schedules a task that can be executed in the following round.
        Sh3Task then(RoundFunc task);

        // schedule a task that can be executed in the same round as this task.
        Sh3Task then(ContinuationFunc task);

        // mark this task as not completed and perform the provided function in the following round.
        void nextRound(RoundFunc task);

        // blocks until this task is completed. 
        void get();

        Sh3Runtime* mRuntime = nullptr;
        i64 mTaskIdx = -1;
    };

    class Sh3TaskBase
    {
    public:
        struct EmptyState {};

        i64 mIdx = -1;
        u64 mDepCount;

        void pop()
        {
            mIdx = -1;
            mChildren.clear();
            mFunc = EmptyState{};
        }

        void addChild(Sh3TaskBase* child)
        {
            mChildren.push_back(child->mIdx);
        }


        std::vector<i64> mChildren;


        boost::variant<
            EmptyState,
            Sh3Task::RoundFunc,
            Sh3Task::ContinuationFunc>
            mFunc = EmptyState{};


        // returns true if this task is initialized.
        bool isValid() const
        {
            return mIdx != -1;
        }

        // returns true if this task is ready to be performed
        bool isReady() const
        {
            return isValid() && mDepCount == 0;
        }

        // returns true if this task has completed all of its work
        bool isCompleted()
        {
            return boost::get<EmptyState>(&mFunc);
        }

        // returns true if this task can be executed in the same round as its parent/dependent tasks.
        bool isContinuationTask()
        {
            return boost::get<Sh3Task::ContinuationFunc>(&mFunc);
        }
    };

    class TaskDeque
    {
    public:

        u64 size() const { return mPushIdx - mPopIdx; }

        u64 capacity() const
        {
            return mTasks.size();
        }

        void reserve(u64 size)
        {
            if (capacity() < size)
            {
                auto nextPowOf2 = 1ull << oc::log2ceil(size);

                auto mod = nextPowOf2 - 1;
                std::vector<Sh3TaskBase> newTasks(nextPowOf2);

                auto s = mPopIdx;
                auto e = mPushIdx;
                while (s != e)
                {
                    newTasks[s & mod] = std::move(*modGet(s));
                    ++s;
                }

                mTasks = std::move(newTasks);
            }
        }


        Sh3TaskBase* push()
        {
            if (size() == capacity())
                reserve(capacity() * 2);

            auto ptr = modGet(mPushIdx);

            if (ptr->isValid())
                throw std::runtime_error(LOCATION);
            ptr->mIdx = mPushIdx;

            ++mPushIdx;

            return ptr;
        }


        Sh3TaskBase* get(i64 taskIdx)
        {
            if (size())
            {
                auto& oldest = *modGet(mPopIdx);
                if (oldest.mIdx <= taskIdx)
                {
                    auto idx = mPopIdx + taskIdx - oldest.mIdx;
                    auto ptr = modGet(idx);

                    if (ptr->mIdx != taskIdx)
                        ptr = nullptr;

                    return ptr;
                }
            }

            return nullptr;
        }

        Sh3TaskBase* getNext(i64 taskIdx)
        {
            while (taskIdx < mPushIdx)
            {
                auto ptr = modGet(taskIdx);
                if (ptr->isReady())
                {
                    return ptr;
                }

                ++taskIdx;
            }

            return nullptr;
        }

        void pop(u64 taskIdx)
        {
            auto ptr = get(taskIdx);
            if (ptr == nullptr)
                throw std::runtime_error(LOCATION);
            if (ptr->isValid() == false)
                throw std::runtime_error(LOCATION);

            ptr->pop();

            while (size() && modGet(mPopIdx)->mIdx == -1)
                ++mPopIdx;
        }


        i64 mPopIdx = 0, mPushIdx = 0;
        std::vector<Sh3TaskBase> mTasks;

        Sh3TaskBase* modGet(u64 idx)
        {
            return &mTasks[idx & (mTasks.size() - 1)];
        }
    };

    class Sh3Runtime
    {
    public:

        Sh3Runtime() = default;
        Sh3Runtime(const Sh3Runtime&) = default;
        Sh3Runtime(Sh3Runtime&&) = default;
        Sh3Runtime(u64 partyIdx, CommPkg& comm)
        {
            init(partyIdx, comm);
        }

        u64 mPartyIdx = -1;
        CommPkg mComm;

        void init(u64 partyIdx, CommPkg& comm)
        {
            mPartyIdx = partyIdx;
            mComm = comm;

            mTasks.reserve(64);
            mNullTask.mRuntime = this;
            mNullTask.mTaskIdx = -1;
            //mActiveTasks.resize(1024, nullptr);
        }
        
        const Sh3Task& noDependencies() const { return mNullTask; }
        operator Sh3Task() const { return noDependencies(); }

        void addTask(span<Sh3Task> deps, Sh3Task& handle, Sh3Task::RoundFunc&& func);

        void addTask(span<Sh3Task> deps, Sh3Task& handle, Sh3Task::ContinuationFunc&& func);
        void configureTask(span<Sh3Task> deps, Sh3Task& handle, Sh3TaskBase* base);

        void runUntilTaskCompletes(i64 taskIdx);
        void runOne();
        void runAll();

        void runTask(Sh3TaskBase* task);

        TaskDeque mTasks;
        Sh3Task mNullTask;
        //std::vector<Sh3TaskBase*> mActiveTasks;
    };

}
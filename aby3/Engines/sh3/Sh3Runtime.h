#pragma once
#include "aby3/Common/Defines.h"
#include "aby3/Engines/sh3/Sh3Defines.h"
#include <cryptoTools/Network/Session.h>

namespace aby3
{

    class Sh3Runtime;
    //class Sh3TaskBase;

    class Sh3Task
    {
    public:
        Sh3Runtime& getRuntime() { return *mRuntime; }

        Sh3Task then(std::function<void(Sh3::CommPkg& comm, Sh3Task& self)> task);

        void nextRound(std::function<void(Sh3::CommPkg& comm, Sh3Task& self)> task);

        void get();

        Sh3Runtime* mRuntime = nullptr;
        i64 mTaskIdx = -1;
    };

    class Sh3TaskBase
    {
    public:
        i64 mIdx = -1;
        u64 mDepCount;

        void pop()
        {
            mIdx = -1;
            mChildren.clear();
            mFunc = nullptr;
        }

        void addChild(Sh3TaskBase* child)
        {
            mChildren.push_back(child->mIdx);
        }


        std::vector<i64> mChildren;
        std::function<void(Sh3::CommPkg& comm, Sh3Task& self)> mFunc;

        bool isValid() const
        {
            return mIdx != -1;
        }

        bool isReady() const
        {
            return isValid() && mDepCount == 0;
        }

        bool isCompleted()
        {
            return !mFunc;
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


        Sh3TaskBase* get(u64 taskIdx)
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


        u64 mPopIdx = 0, mPushIdx = 0;
        std::vector<Sh3TaskBase> mTasks;

        Sh3TaskBase* modGet(u64 idx)
        {
            return &mTasks[idx & (mTasks.size() - 1)];
        }
    };

    class Sh3Runtime
    {
    public:
        u64 mPartyIdx = -1;
        Sh3::CommPkg mComm;

        void init(u64 partyIdx, Sh3::CommPkg& comm)
        {
            mPartyIdx = partyIdx;
            mComm = comm;

            mTasks.reserve(1024);
            mNullTask.mRuntime = this;
            mNullTask.mTaskIdx = -1;
            //mActiveTasks.resize(1024, nullptr);
        }
        
        const Sh3Task& nullTask() const { return mNullTask; }

        void addTask(span<Sh3Task> deps, Sh3Task& handle, std::function<void(Sh3::CommPkg& comm, Sh3Task& self)> func);


        void runUntilTaskCompletes(i64 taskIdx);
        void runOne();
        void runAll();

        TaskDeque mTasks;
        Sh3Task mNullTask;
        //std::vector<Sh3TaskBase*> mActiveTasks;
    };

}
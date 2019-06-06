#pragma once
#include "aby3/Common/Defines.h"
#include "aby3/Engines/sh3/Sh3Types.h"
#include <cryptoTools/Network/Session.h>
#include <boost/variant.hpp>
#include <function2/function2.hpp>
#include <deque>
#include <unordered_map>

namespace aby3
{

    class Sh3Runtime;
	class Sh3TaskBase;

    class Sh3Task
    {
    public:

        using RoundFunc = fu2::unique_function<void(CommPkg& comm, Sh3Task& self)>;
        using ContinuationFunc = fu2::unique_function<void(Sh3Task& self)>;

        // returns the associated runtime.
        Sh3Runtime& getRuntime() const { return *mRuntime; }

        // schedules a task that can be executed in the following round.
        Sh3Task then(RoundFunc task);

        // schedule a task that can be executed in the same round as this task.
        Sh3Task then(ContinuationFunc task);

		// Get a task that is fulfilled when all of this tasks dependencies
		// are fulfilled. 
		Sh3Task getClosure();


		Sh3Task operator&&(const Sh3Task& o)const;

        // blocks until this task is completed. 
        void get();


		Sh3TaskBase* basePtr();
        Sh3Runtime* mRuntime = nullptr;
        i64 mTaskIdx = -1;
    };

    class Sh3TaskBase
    {
    public:
		Sh3TaskBase() = default;
		Sh3TaskBase(const Sh3TaskBase&) = delete;
		Sh3TaskBase(Sh3TaskBase&&) = default;
		~Sh3TaskBase() = default;

        struct EmptyState {};
		struct Closure {};
		struct And {};

        i64 mIdx = -1;
        u64 mDepCount_ = 0;

        void clear()
        {
            mIdx = -1;
            mChildren.clear();
            mFunc = EmptyState{};
        }

        void addChild(Sh3TaskBase& child)
        {
            mChildren.push_back(child.mIdx);
        }


		// The list of downstream tasks that depend on this task. 
		// When this task is fulfillied, we will check if these
		// tasks are ready to be run. 
		std::vector<i64> mChildren;

		// The list of upstream (closure) tasks that should be partially 
		// fulfilled when this task is fulfilled. 
		//std::vector<i64> mClosures;

        boost::variant<
            EmptyState,
			Closure,
			And,
            Sh3Task::RoundFunc,
            Sh3Task::ContinuationFunc>
            mFunc = EmptyState{};

		void depFulfilled(Sh3TaskBase& parent, Sh3Runtime& rt);

        // returns true if this task is initialized.
        bool isValid() const
        {
            return mIdx != -1;
        }

        // returns true if this task is ready to be performed
        bool isReady() const
        {
            return isValid() && mDepCount_ == 0;
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

		bool isAggregateTask()
		{
			return isClosure() || isAnd();
		}

		bool isClosure()
		{
			return boost::get<Closure>(&mFunc);
		}
		bool isAnd()
		{
			return boost::get<And>(&mFunc);
		}
    };


	class TaskDag
	{
	public:
		std::deque<i64> mReadyDeque;
		std::unordered_map<i64, Sh3TaskBase> mTaskMap;

		i64 mPushIdx = 0;
		Sh3TaskBase& emplace()
		{
			auto p = mTaskMap.emplace(mPushIdx, Sh3TaskBase{});
			p.first->second.mIdx = mPushIdx++;
			return p.first->second;
		}
		void enqueueBack(i64 idx)
		{
			if (mTaskMap.find(idx) == mTaskMap.end())
				throw RTE_LOC;
			mReadyDeque.push_back(idx);
		}

		//void enqueueFront(i64 idx)
		//{
		//	if (mTaskMap.find(idx) == mTaskMap.end())
		//		throw RTE_LOC;
		//	mReadyDeque.push_front(idx);
		//}

		Sh3TaskBase& front()
		{
			if (mReadyDeque.size() == 0)
				throw RTE_LOC;
			return mTaskMap.find(mReadyDeque.front())->second;
		}
		void popFront()
		{
			if (mReadyDeque.size() == 0)
				throw RTE_LOC;

			auto iter = mTaskMap.find(mReadyDeque.front());
			if (iter == mTaskMap.end())
				throw RTE_LOC;

			mTaskMap.erase(iter);
			mReadyDeque.pop_front();
		}

		void remove(i64 idx)
		{
			auto iter = mTaskMap.find(idx);
			if (iter == mTaskMap.end())
				throw RTE_LOC;

			mTaskMap.erase(iter);
		}

		Sh3TaskBase* tryGet(i64 idx)
		{
			if (idx >= mPushIdx)
				throw std::runtime_error("requested task that has not been created. " LOCATION);

			auto iter = mTaskMap.find(idx);
			if (iter == mTaskMap.end())
				return nullptr;
			return &iter->second;
		}

		void reserve(u64 n)
		{
			//mReadyQueue.reserve(n);
			mTaskMap.reserve(n);
		}

		u64 size()
		{
			return mTaskMap.size();
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

		~Sh3Runtime()
		{
			if (mTasks.size())
			{
				std::cout << "~~~~~~~~~~~~~~~~ Runtime not empty!!! ~~~~~~~~~~~~~~~~" << std::endl;
			}
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

		Sh3Task addTask(span<Sh3Task> deps, Sh3Task::RoundFunc&& func);

		Sh3Task addTask(span<Sh3Task> deps, Sh3Task::ContinuationFunc&& func);

		Sh3Task addClosure(span<Sh3Task> deps);
		
		Sh3Task addAnd(span<Sh3Task> deps);

		void configureAnd(span<i64> deps, Sh3TaskBase& handle);
		void configureClosure(span<i64> deps, Sh3TaskBase& handle);
		Sh3Task configureTask(span<Sh3Task> deps, Sh3TaskBase& base);

        void runUntilTaskCompletes(i64 taskIdx);
        void runNext();
        void runAll();
		void runOneRound();

        //void runTask(Sh3TaskBase* task);

        TaskDag mTasks;
        Sh3Task mNullTask;
        //std::vector<Sh3TaskBase*> mActiveTasks;
    };



	// class TaskDeque
	// {
	// public:

	//     u64 size() const { return mPushIdx - mPopIdx; }

	//     u64 capacity() const
	//     {
	//         return mTasks.size();
	//     }

	//     void reserve(u64 size)
	//     {
	//         if (capacity() < size)
	//         {
	//             auto nextPowOf2 = 1ull << oc::log2ceil(size);

	//             auto mod = nextPowOf2 - 1;
	//             std::vector<Sh3TaskBase> newTasks(nextPowOf2);

	//             auto s = mPopIdx;
	//             auto e = mPushIdx;
	//             while (s != e)
	//             {
	//                 newTasks[s & mod] = std::move(*modGet(s));
	//                 ++s;
	//             }

	//             mTasks = std::move(newTasks);
	//         }
	//     }


	//     Sh3TaskBase* push()
	//     {
	//         if (size() == capacity())
	//             reserve(capacity() * 2);

	//         auto ptr = modGet(mPushIdx);

	//         if (ptr->isValid())
	//             throw std::runtime_error(LOCATION);
	//         ptr->mIdx = mPushIdx;

	//         ++mPushIdx;

	//         return ptr;
	//     }


	//     Sh3TaskBase* get(i64 taskIdx)
	//     {
			 //if (taskIdx >= mPushIdx)
			 //	throw std::runtime_error("requested task that has not been created. " LOCATION);

	//         if (size())
	//         {
	//             auto& oldest = *modGet(mPopIdx);
	//             if (oldest.mIdx <= taskIdx)
	//             {
	//                 auto idx = mPopIdx + taskIdx - oldest.mIdx;
	//                 auto ptr = modGet(idx);

	//                 if (ptr->mIdx != taskIdx)
	//                     ptr = nullptr;

	//                 return ptr;
	//             }
	//         }

	//         return nullptr;
	//     }

	//     Sh3TaskBase* getNext(i64 taskIdx)
	//     {
	//         while (taskIdx < mPushIdx)
	//         {
	//             auto ptr = modGet(taskIdx);
	//             if (ptr->isReady())
	//             {
	//                 return ptr;
	//             }

	//             ++taskIdx;
	//         }

	//         return nullptr;
	//     }

	//     void pop(u64 taskIdx)
	//     {
	//         auto ptr = get(taskIdx);
	//         if (ptr == nullptr)
	//             throw std::runtime_error(LOCATION);
	//         if (ptr->isValid() == false)
	//             throw std::runtime_error(LOCATION);

	//         ptr->pop();

	//         while (size() && modGet(mPopIdx)->mIdx == -1)
	//             ++mPopIdx;
	//     }


	//     i64 mPopIdx = 0, mPushIdx = 0;
	//     std::vector<Sh3TaskBase> mTasks;

	//     Sh3TaskBase* modGet(u64 idx)
	//     {
	//         return &mTasks[idx & (mTasks.size() - 1)];
	//     }
	// };

}
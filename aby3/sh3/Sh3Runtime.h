#pragma once
#include "aby3/Common/Defines.h"
#include "aby3/sh3/Sh3Types.h"
#include <cryptoTools/Network/Session.h>
#include <boost/variant.hpp>
#include <function2/function2.hpp>
#include <deque>
#include <unordered_map>
#include <string>
#include <unordered_set>

namespace aby3
{

    class Sh3Runtime;
	class Sh3TaskBase;


    class Sh3Task
    {
    public:
#ifdef __INTELLISENSE__ 
		using RoundFunc = std::function<void(CommPkg & comm, Sh3Task & self)>;
		using ContinuationFunc = std::function<void(Sh3Task & self)>;
#else
        using RoundFunc = fu2::unique_function<void(CommPkg& comm, Sh3Task& self)>;
        using ContinuationFunc = fu2::unique_function<void(Sh3Task& self)>;
#endif

		enum Type { Evaluation, Closure };

        // returns the associated runtime.
        Sh3Runtime& getRuntime() const { return *mRuntime; }

        // schedules a task that can be executed in the following round.
        Sh3Task then(RoundFunc task);

        // schedule a task that can be executed in the same round as this task.
        Sh3Task then(ContinuationFunc task);

		// schedules a task that can be executed in the following round.
		Sh3Task then(RoundFunc task, std::string name);

		// schedule a task that can be executed in the same round as this task.
		Sh3Task then(ContinuationFunc task, std::string name);

		// Get a task that is fulfilled when all of this tasks dependencies
		// are fulfilled. 
		Sh3Task getClosure();

		//Sh3Task getClosure(std::string anme);

		std::string& name();

		Sh3Task operator&&(const Sh3Task& o)const;

		Sh3Task operator&=(const Sh3Task& o);

        // blocks until this task is completed. 
        void get();

		bool isCompleted();

		bool operator==(const Sh3Task& t) const { return mRuntime == t.mRuntime && mIdx == t.mIdx && mType == t.mType; }
		bool operator!=(const Sh3Task& t) const { return !(*this == t); }


		Sh3TaskBase* basePtr();
        Sh3Runtime* mRuntime = nullptr;
        i64 mIdx = -1;
		Type mType = Evaluation;
    };

    class Sh3TaskBase
    {
    public:
		Sh3TaskBase() = default;
		Sh3TaskBase(const Sh3TaskBase&) = delete;
		Sh3TaskBase(Sh3TaskBase&&) = default;
		~Sh3TaskBase() = default;

        struct EmptyState {};
		struct And {};

        i64 mIdx = -1;

		std::vector<Sh3Task> mDepList;
		std::string mName;

		// The list of downstream tasks that depend on this task. 
		// When this task is fulfillied, we will check if these
		// tasks are ready to be run. 
		std::vector<i64> mChildren, mUpstreamClosures, mClosureChildren;

		//bool mExtendLifetime = false;
		i64 mClosureCount = -1, mDepCount = 0;
		//bool mIsClosure = false;


        void addChild(Sh3TaskBase& child, Sh3Task::Type type)
        {
			if (type == Sh3Task::Evaluation)
				mChildren.push_back(child.mIdx);
			else if (type == Sh3Task::Closure)
				mClosureChildren.push_back(child.mIdx);
			else
				throw RTE_LOC;
        }

		void addDep(Sh3Task d)
		{
			++mDepCount;
			mDepList.emplace_back(d);
		}



		enum Status { Pending, Evaluated, Closed };
		Status mStatus = Pending;

        boost::variant<
            EmptyState,
			//Closure,
			And,
            Sh3Task::RoundFunc,
            Sh3Task::ContinuationFunc>
            mFunc = EmptyState{};

		void depFulfilled(Sh3Task parent, Sh3Runtime& rt);

        // returns true if this task is initialized.
        bool isValid() const
        {
            return mIdx != -1;
        }

        // returns true if this task is ready to be performed
        bool isReady() const
        {
            return mDepCount == 0;
        }

        // returns true if this task has completed all of its work
        bool isEvaluated()
        {
            return mStatus == Evaluated || mStatus == Closed;
        }

        // returns true if this task can be executed in the same round as its parent/dependent tasks.
        bool isContinuationTask()
        {
            return boost::get<Sh3Task::ContinuationFunc>(&mFunc);
        }


		bool isRemovable()
		{
			return isEvaluated() && (mUpstreamClosures.size() == 0);
		}

		bool isClosure()
		{
			return mClosureCount != -1;
		}
		bool isAnd()
		{
			return boost::get<And>(&mFunc);
		}
    };


	inline std::ostream& operator<<(std::ostream& o, const Sh3Task& d)
	{
		o << d.mIdx;
		if (d.mType == Sh3Task::Evaluation)
			o << ".E";
		else
			o << ".C";
		return o;
	}

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

			mReadyDeque.pop_front();

			if (iter->second.isRemovable())
			{
				mTaskMap.erase(iter);
			}
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

		Sh3TaskBase& get(i64 idx)
		{
			auto ptr = tryGet(idx);
			if (ptr) return *ptr;
			throw std::runtime_error("Task has been evaluated and removed. " LOCATION);
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
        Sh3Runtime(const Sh3Runtime&) = delete;
        Sh3Runtime(Sh3Runtime&&) = delete;
        Sh3Runtime(u64 partyIdx, CommPkg& comm)
        {
            init(partyIdx, comm);
        }

		~Sh3Runtime()
		{
			if (mTasks.size() && std::uncaught_exception() == false)
			{
				std::cout << "~~~~~~~~~~~~~~~~ Runtime not empty!!! ~~~~~~~~~~~~~~~~" << std::endl;
			}
		}

		bool mPrint = false;
        u64 mPartyIdx = -1;
        CommPkg mComm;
		bool mIsActive = false;

        void init(u64 partyIdx, CommPkg& comm)
        {
            mPartyIdx = partyIdx;
            mComm = comm;

            mTasks.reserve(64);
            mNullTask.mRuntime = this;
            mNullTask.mIdx = -1;
            //mActiveTasks.resize(1024, nullptr);
        }
        
        const Sh3Task& noDependencies() const { return mNullTask; }
        operator Sh3Task() const { return noDependencies(); }

		Sh3Task addTask(span<Sh3Task> deps, Sh3Task::RoundFunc&& func, std::string&& name);

		Sh3Task addTask(span<Sh3Task> deps, Sh3Task::ContinuationFunc&& func, std::string&& name);

		Sh3Task addClosure(Sh3Task dep);
		
		Sh3Task addAnd(span<Sh3Task> deps, std::string&& name);

		void configureAnd(span<Sh3Task> deps, Sh3TaskBase& handle);
		//void configureClosure(span<i64> deps, Sh3TaskBase& handle);
		Sh3Task configureTask(span<Sh3Task> deps, Sh3TaskBase& base);


		void removeClosure(Sh3TaskBase& closure);

        void runUntilTaskCompletes(Sh3Task task);
        void runNext();
        void runAll();
		void runOneRound();

		void cancelTasks();

        TaskDag mTasks;
        Sh3Task mNullTask;
    };

}
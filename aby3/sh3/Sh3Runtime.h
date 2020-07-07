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
#include "aby3/Common/Task.h"

namespace aby3
{

    class Sh3Runtime;
	class Sh3TaskBase;


    class Sh3Task
    {
    public:

        using RoundFunc = fu2::unique_function<void(CommPkg& comm, Sh3Task& self)>;
        using ContinuationFunc = fu2::unique_function<void(Sh3Task& self)>;
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
		struct Closure {};

		std::string mName;

		boost::variant<
			EmptyState,
			Closure,
			And,
			Sh3Task::RoundFunc,
			Sh3Task::ContinuationFunc>
			mFunc = EmptyState{};


  //      // returns true if this task is ready to be performed
        //bool isReady() const
        //{
        //    return mBase->mUpstream.size() == 0;
        //}

  //      // returns true if this task has completed all of its work
  //      bool isEvaluated()
  //      {
  //          return mStatus == Evaluated || mStatus == Closed;
  //      }

        // returns true if this task can be executed in the same round as its parent/dependent tasks.
        //bool isContinuationTask()
        //{
        //    return boost::get<Sh3Task::ContinuationFunc>(&mFunc);
        //}


		//bool isRemovable()
		//{
		//	return isEvaluated() && (mUpstreamClosures.size() == 0);
		//}

		//bool isClosure()
		//{
		//	return boost::get<Closure>(&mFunc);
		//}
		//bool isAnd()
		//{
		//	return boost::get<And>(&mFunc);
		//}
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
			if (mSched.mTasks.size())
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

            //mTasks.reserve(64);
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

		//void configureAnd(span<Sh3Task> deps, Sh3TaskBase& handle);
		//void configureClosure(span<i64> deps, Sh3TaskBase& handle);
		//Sh3Task configureTask(span<Sh3Task> deps, Sh3TaskBase& base);


		//void removeClosure(Sh3TaskBase& closure);

        void runUntilTaskCompletes(Sh3Task task);
        void runNext();
        void runAll();
		void runOneRound();

		std::unordered_map<u64, Sh3TaskBase> mTasks;
        Scheduler mSched;
        Sh3Task mNullTask;
    };

}
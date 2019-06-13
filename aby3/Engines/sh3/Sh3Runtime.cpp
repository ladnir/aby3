#include "Sh3Runtime.h"
#include <cryptoTools/Common/Log.h>
#include <sstream>

namespace aby3
{






	Sh3Task Sh3Task::then(RoundFunc task)
	{
		return getRuntime().addTask({ this, 1 }, std::move(task), {});
	}

	Sh3Task Sh3Task::then(ContinuationFunc task)
	{
		return getRuntime().addTask({ this, 1 }, std::move(task), {});
	}



	Sh3Task Sh3Task::then(RoundFunc task, std::string name)
	{
		return getRuntime().addTask({ this, 1 }, std::move(task), std::move(name));
	}

	Sh3Task Sh3Task::then(ContinuationFunc task, std::string name)
	{
		return getRuntime().addTask({ this, 1 }, std::move(task), std::move(name));
	}


	Sh3Task Sh3Task::getClosure()
	{
		return getRuntime().addClosure(*this);

	}

	std::string& Sh3Task::name() { return basePtr()->mName; }

	Sh3Task Sh3Task::operator&&(const Sh3Task& o) const
	{
		std::array<Sh3Task, 2> deps{ *this, o };
		return getRuntime().addAnd(deps, {});
	}

	void Sh3Task::get()
	{
		getRuntime().runUntilTaskCompletes(*this);
	}

	bool Sh3Task::isCompleted() {
		auto taskPtr = basePtr();

		return
			(taskPtr == nullptr) ||
			(mType == Sh3Task::Evaluation && taskPtr->isEvaluated()) ||
			(mType == Sh3Task::Closure && taskPtr->mClosureCount == 0);

	}

	Sh3TaskBase* Sh3Task::basePtr()
	{
		return getRuntime().mTasks.tryGet(mIdx);
	}

	Sh3Task Sh3Runtime::addTask(span<Sh3Task> deps, Sh3Task::RoundFunc&& func, std::string&& name)
	{
		if (func)
		{
			auto& newTask = mTasks.emplace();
			newTask.mFunc = std::forward<Sh3Task::RoundFunc>(func);
			newTask.mName = std::forward<std::string>(name);
			return configureTask(deps, newTask);
		}
		else
		{
			std::cout << "empty task (round function)" << std::endl;
			throw RTE_LOC;
		}
	}


	Sh3Task Sh3Runtime::addTask(span<Sh3Task> deps, Sh3Task::ContinuationFunc&& func, std::string&& name)
	{
		if (func)
		{
			auto& newTask = mTasks.emplace();
			newTask.mFunc = std::forward<Sh3Task::ContinuationFunc>(func);
			newTask.mName = std::forward<std::string>(name);
			return configureTask(deps, newTask);
		}
		else
		{
			std::cout << "empty task (round function)" << std::endl;
			throw RTE_LOC;
		}
	}
	auto getDownstream(TaskDag& tasks, Sh3TaskBase& task)
	{
		std::unordered_set<Sh3TaskBase*> downstreamTasks;
		downstreamTasks.emplace(&task);
		std::function<void(i64)> rec = [&](i64 dsTaskIdx)
		{
			auto& dsTask = tasks.get(dsTaskIdx);
			downstreamTasks.emplace(&dsTask);

			for (auto c : dsTask.mChildren)
				rec(c);
		};

		for (auto c : task.mChildren)
			rec(c);

		return downstreamTasks;
	}

	Sh3Task Sh3Runtime::addClosure(Sh3Task dep)
	{
		auto taskPtr = dep.basePtr();

		if (taskPtr->isClosure())
			return Sh3Task{ this, dep.mIdx, Sh3Task::Closure };

		if (taskPtr && !taskPtr->isEvaluated())
		{
			auto& task = *taskPtr;


			if (mPrint)
				oc::lout << "adding closure " << task.mIdx << " " << task.mName << " {";

			auto downstreamTasks = getDownstream(mTasks, task);

			task.mClosureCount = downstreamTasks.size();
			for (auto dsPtr : downstreamTasks)
			{
				dsPtr->mUpstreamClosures.push_back(task.mIdx);
				//dsPtr->mExtendLifetime = true;
				if (mPrint)
					oc::lout << "\n  " << dsPtr->mIdx << " " << dsPtr->mName;
			}

			if (mPrint)
				oc::lout << " }" << std::endl;

			return { this, dep.mIdx, Sh3Task::Closure };
		}
		else
		{

			throw std::runtime_error("Can not get a closure on a completed task. " LOCATION);
		}
	}

	Sh3Task Sh3Runtime::addAnd(span<Sh3Task> deps, std::string && name)
	{
		auto& newAnd = mTasks.emplace();
		newAnd.mFunc = Sh3TaskBase::And{};
		newAnd.mName = std::forward<std::string>(name);

		if (mPrint)
			oc::lout << "adding And " << newAnd.mIdx << " " << newAnd.mName << " { ... }\n";

		configureAnd(deps, newAnd);

		Sh3Task handle;
		handle.mIdx = newAnd.mIdx;
		handle.mRuntime = this;
		return handle;
	}

	void Sh3Runtime::configureAnd(span<Sh3Task> deps, Sh3TaskBase & newAnd)
	{
		for (auto d : deps)
		{
			if (newAnd.mIdx == d.mIdx)
				throw RTE_LOC;

			auto depPtr = mTasks.tryGet(d.mIdx);
			if (depPtr)
			{
				if (!depPtr->isEvaluated())
				{
					depPtr->addChild(newAnd, d.mType);
					newAnd.addDep(d);
				}

				for (auto c : depPtr->mUpstreamClosures)
				{
					auto& closure = mTasks.get(c);
					closure.mClosureCount++;
					newAnd.mUpstreamClosures.push_back(c);
				}
			}
		}
	}

	Sh3Task Sh3Runtime::configureTask(span<Sh3Task> deps, Sh3TaskBase & newTask)
	{
		std::stringstream ss;

		if (mPrint)
			oc::lout << "adding Task " << newTask.mIdx << " " << newTask.mName << " { ";

		for (auto& dep : deps)
		{

			if (newTask.mIdx == dep.mIdx)
				throw RTE_LOC;

			auto depPtr = mTasks.tryGet(dep.mIdx);
			if (depPtr)
			{
				if (!depPtr->isEvaluated())
				{
					newTask.addDep(dep);
					depPtr->addChild(newTask, dep.mType);
				}

				for (auto c : depPtr->mUpstreamClosures)
				{
					if (c == depPtr->mIdx && dep.mType == Sh3Task::Closure)
						continue;

					auto& closure = mTasks.get(c);
					closure.mClosureCount++;
					newTask.mUpstreamClosures.push_back(c);

					ss << "   extending " << closure.mIdx << " " << closure.mName << " to " << closure.mClosureCount << "\n";
				}

				if (mPrint)
					oc::lout << "\n  " << depPtr->mIdx << " " << depPtr->mName;
			}
		}

		if (mPrint)
			oc::lout << "}\n" << ss.str() << std::flush;

		if (newTask.mDepCount == 0)
		{

			if (mPrint)
				oc::lout << "Queuing  " << newTask.mIdx << " " << newTask.mName << std::endl;

			mTasks.enqueueBack(newTask.mIdx);
		}

		return { this, newTask.mIdx, Sh3Task::Evaluation };
	}

	void Sh3Runtime::runUntilTaskCompletes(Sh3Task task)
	{
		while (task.isCompleted() == false)
			runNext();
	}

	void Sh3Runtime::runAll()
	{
		while (mTasks.size())
			runNext();
	}

	void Sh3Runtime::runOneRound()
	{
		if (mTasks.mReadyDeque.size())
		{
			auto end = mTasks.mReadyDeque.back();
			while (mTasks.mReadyDeque.front() != end)
				runNext();

			runNext();
		}
	}

	void print(TaskDag & tasks)
	{
		oc::lout << "=================================\nReady: \n";

		for (u64 i = 0; i < tasks.mReadyDeque.size(); ++i)
		{
			oc::lout << "r[" << i << "] = " << tasks.mReadyDeque[i] << "\n";
		}
		oc::lout << "\nAll: \n";

		u64 i = 0;
		for (auto& t : tasks.mTaskMap)
		{
			std::string type;
			if (t.second.isAnd())
				type = "And";
			else if (t.second.isClosure())
				type = "Closure";
			else if (t.second.isContinuationTask())
				type = "Cont.";
			else
				type = "Task";

			oc::lout << "t[" << (i++) << "] = " << t.second.mIdx << " " << t.second.mName << " " << type << "\n";

			for (u64 j = 0; j < t.second.mDepList.size(); ++j)
				oc::lout << "   dep[" << j << "] = " << t.second.mDepList[j] << "\n";

			for (u64 j = 0; j < t.second.mChildren.size(); ++j)
				oc::lout << "   chl[" << j << "] = " << t.second.mChildren[j] << "\n";

			for (u64 j = 0; j < t.second.mUpstreamClosures.size(); ++j)
				oc::lout << "   clo[" << j << "] = " << t.second.mUpstreamClosures[j] << "\n";

		}
		oc::lout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n" << std::flush;
	}

	void Sh3Runtime::runNext()
	{
		if (mIsActive)
			throw std::runtime_error("The runtime is currently running a different task. Do not call Sh3Task.get() recursively. " LOCATION);
		auto& task = mTasks.front();

		if (mPrint)
		{
			print(mTasks);
			oc::lout << "evaluating " << task.mIdx << " " << task.mName << std::endl;
		}

		if (task.mDepCount)
			throw RTE_LOC;

		Sh3Task t{ this, task.mIdx, Sh3Task::Evaluation };
		auto roundFuncPtr = boost::get<Sh3Task::RoundFunc>(&task.mFunc);
		auto continueFuncPtr = boost::get<Sh3Task::ContinuationFunc>(&task.mFunc);

		mIsActive = true;
		if (roundFuncPtr)
			(*roundFuncPtr)(mComm, t);
		else if (continueFuncPtr)
			(*continueFuncPtr)(t);
		else throw RTE_LOC;
		mIsActive = false;


		if (mPrint)
			oc::lout << "evaluated " << task.mIdx << " " << task.mName << std::endl;


		for (auto cIdx : task.mUpstreamClosures)
		{
			auto& closure = mTasks.get(cIdx);
			--closure.mClosureCount;

			if (closure.mClosureCount == 0)
				removeClosure(closure);
		}

		for (auto cIdx : task.mChildren)
			mTasks.get(cIdx).depFulfilled(t, *this);

		task.mStatus = Sh3TaskBase::Evaluated;
		mTasks.popFront();

	}

	void Sh3Runtime::removeClosure(Sh3TaskBase& closure)
	{
		if (closure.mClosureCount != 0)
			throw RTE_LOC;


		auto dsTasks = getDownstream(mTasks, closure);
		std::vector<i64> removes; removes.reserve(dsTasks.size());
		for (auto dsPtr : dsTasks)
		{
			if (dsPtr->isEvaluated() == false && dsPtr->mIdx != mTasks.mReadyDeque.front())
				throw RTE_LOC;

			auto iter = std::find(dsPtr->mUpstreamClosures.begin(), dsPtr->mUpstreamClosures.end(), closure.mIdx);
			if (iter == dsPtr->mUpstreamClosures.end())
				throw RTE_LOC;

			std::swap(*iter, dsPtr->mUpstreamClosures.back());
			dsPtr->mUpstreamClosures.pop_back();

			if (dsPtr->isRemovable())
			{
				removes.push_back(dsPtr->mIdx);
			}
		}

		Sh3Task dep{this, closure.mIdx, Sh3Task::Closure };
		for (auto cIdx : closure.mClosureChildren)
			mTasks.get(cIdx).depFulfilled(dep, *this);

		closure.mStatus = Sh3TaskBase::Closed;

		for(auto r : removes)
			mTasks.remove(r);
	}


	void Sh3TaskBase::depFulfilled(Sh3Task parent, Sh3Runtime & rt)
	{
		if (isValid() == false || isReady())
			throw RTE_LOC;

		auto iter = std::find(mDepList.begin(), mDepList.end(), parent);
		if (iter == mDepList.end())
			throw RTE_LOC;

		--mDepCount;

		if (rt.mPrint)
		{
			oc::lout << "dep fulfill " << mIdx << " " << mName << " to " << mDepCount << "   by   " 
				<< parent << " " << rt.mTasks.get(parent.mIdx).mName  << std::endl;
		}

		if (isReady())
		{
			auto c = isContinuationTask();
			auto a = isAnd();
			if (c || a)
			{
				Sh3Task dep{ &rt, mIdx, Sh3Task::Evaluation };

				if (c) {
					if (rt.mPrint)
						oc::lout << "Evaluating cont. " << mIdx << " " << mName << std::endl;

					rt.mIsActive = true;
					boost::get<Sh3Task::ContinuationFunc>(mFunc)(dep);
					rt.mIsActive = false;

					if (rt.mPrint)
						oc::lout << "Evaluated cont. " << mIdx << " " << mName << std::endl;
				}


				for (auto cIdx : mUpstreamClosures)
				{
					auto& closure = rt.mTasks.get(cIdx);
					--closure.mClosureCount;

					if (closure.mClosureCount == 0)
						rt.removeClosure(closure);
				}

				for (auto childIdx : mChildren) 
					rt.mTasks.get(childIdx).depFulfilled(dep, rt);


				mStatus = Sh3TaskBase::Evaluated;
				if(isRemovable())
					rt.mTasks.remove(mIdx);
			}
			else
			{

				if (rt.mPrint)
					oc::lout << "Queuing  " << mIdx << " " << mName << std::endl;

				rt.mTasks.enqueueBack(mIdx);
			}
		}
	}

}


#include "Sh3Runtime.h"


namespace aby3
{






	Sh3Task Sh3Task::then(RoundFunc task)
	{
		return getRuntime().addTask({ this, 1 }, std::move(task));
	}

	Sh3Task Sh3Task::then(ContinuationFunc task)
	{
		return getRuntime().addTask({ this, 1 }, std::move(task));
	}

	Sh3Task Sh3Task::getClosure()
	{
		return getRuntime().addClosure({ this, 1 });

	}

	Sh3Task Sh3Task::operator&&(const Sh3Task& o) const
	{
		std::array<Sh3Task, 2> deps{ *this, o };
		return getRuntime().addAnd(deps);
	}

	void Sh3Task::get()
	{
		getRuntime().runUntilTaskCompletes(mTaskIdx);
	}

	Sh3TaskBase* Sh3Task::basePtr()
	{
		return getRuntime().mTasks.tryGet(mTaskIdx);
	}

	Sh3Task Sh3Runtime::addTask(span<Sh3Task> deps, Sh3Task::RoundFunc&& func)
	{
		if (func)
		{
			auto& newTask = mTasks.emplace();
			newTask.mFunc = std::forward<Sh3Task::RoundFunc>(func);
			return configureTask(deps, newTask);
		}
		else
		{
			std::cout << "empty task (round function)" << std::endl;
			throw RTE_LOC;
		}
	}


	Sh3Task Sh3Runtime::addTask(span<Sh3Task> deps, Sh3Task::ContinuationFunc&& func)
	{
		if (func)
		{
			auto& newTask = mTasks.emplace();
			newTask.mFunc = std::forward<Sh3Task::ContinuationFunc>(func);
			return configureTask(deps, newTask);
		}
		else
		{
			std::cout << "empty task (round function)" << std::endl;
			throw RTE_LOC;
		}
	}

	Sh3Task Sh3Runtime::addClosure(span<Sh3Task> deps)
	{

		auto& newTask = mTasks.emplace();
		newTask.mFunc = Sh3TaskBase::Closure{};

		for (auto idx : deps)
		{
			configureClosure({ &idx.mTaskIdx, 1 }, newTask);
		}

		Sh3Task handle;
		handle.mTaskIdx = newTask.mIdx;
		handle.mRuntime = this;
		return handle;
	}

	Sh3Task Sh3Runtime::addAnd(span<Sh3Task> deps)
	{
		auto& newAnd = mTasks.emplace();
		newAnd.mFunc = Sh3TaskBase::And{};

		for (auto idx : deps)
		{
			configureAnd({ &idx.mTaskIdx, 1 }, newAnd);
		}

		Sh3Task handle;
		handle.mTaskIdx = newAnd.mIdx;
		handle.mRuntime = this;
		return handle;
	}

	void Sh3Runtime::configureAnd(span<i64> deps, Sh3TaskBase& newAnd)
	{
		for (auto idx : deps)
		{
			auto depPtr = mTasks.tryGet(idx);
			if (depPtr)
			{
				depPtr->mChildren.push_back(newAnd.mIdx);
				newAnd.mDepCount_++;
			}
		}
	}

	void Sh3Runtime::configureClosure(span<i64> deps, Sh3TaskBase& closure)
	{
		for (auto idx : deps)
		{
			auto taskPtr = mTasks.tryGet(idx);
			if (taskPtr)
			{
				configureClosure(taskPtr->mChildren, closure);

				taskPtr->mChildren.push_back(closure.mIdx);
				closure.mDepCount_++;
			}
		}
	}

	Sh3Task Sh3Runtime::configureTask(span<Sh3Task> deps, Sh3TaskBase& newTask)
	{

		for (auto& dep : deps)
		{
			auto depPtr = mTasks.tryGet(dep.mTaskIdx);
			if (depPtr)
			{
				for (auto c : depPtr->mChildren)
				{
					auto closurePtr = mTasks.tryGet(c);
					if (closurePtr && closurePtr->isClosure())
					{
						newTask.mChildren.push_back(closurePtr->mIdx);
						closurePtr->mDepCount_++;
					}
				}

				++newTask.mDepCount_;
				depPtr->addChild(newTask);
			}
		}

		if (newTask.mDepCount_ == 0)
		{
			mTasks.enqueueBack(newTask.mIdx);
		}

		Sh3Task handle;
		handle.mTaskIdx = newTask.mIdx;
		handle.mRuntime = this;
		return handle;
	}

	void Sh3Runtime::runUntilTaskCompletes(i64 taskIdx)
	{
		while (mTasks.tryGet(taskIdx))
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


	void Sh3Runtime::runNext()
	{
		auto& task = mTasks.front();
		if (task.mDepCount_)
			throw RTE_LOC;

		Sh3Task t{ this, task.mIdx };
		auto roundFuncPtr = boost::get<Sh3Task::RoundFunc>(&task.mFunc);
		auto continueFuncPtr = boost::get<Sh3Task::ContinuationFunc>(&task.mFunc);

		if (roundFuncPtr)
			(*roundFuncPtr)(mComm, t);
		else if (continueFuncPtr)
			(*continueFuncPtr)(t);
		else throw RTE_LOC;


		for (auto cIdx : task.mChildren)
		{
			auto childPtr = mTasks.tryGet(cIdx);
			if (childPtr == nullptr)
				throw std::runtime_error(LOCATION);
			childPtr->depFulfilled(task, *this);
		}

		mTasks.popFront();

	}

	void Sh3TaskBase::depFulfilled(Sh3TaskBase & parent, Sh3Runtime & rt)
	{
		if (isValid() == false || isReady())
			throw RTE_LOC;

		--mDepCount_;

		if (isReady())
		{
			auto c = isContinuationTask();
			auto a = isAggregateTask();
			if (c || a)
			{
				if (c) {
					boost::get<Sh3Task::ContinuationFunc>(mFunc)(Sh3Task{ &rt, mIdx });
				}

				for (auto childIdx : mChildren) {
					auto childPtr = rt.mTasks.tryGet(childIdx);
					if (childPtr == nullptr)
						throw RTE_LOC;

					childPtr->depFulfilled(*this, rt);
				}

				rt.mTasks.remove(mIdx);
			}
			else
			{
				rt.mTasks.enqueueBack(mIdx);
			}
		}
	}

}


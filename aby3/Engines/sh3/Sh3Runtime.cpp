#include "Sh3Runtime.h"


namespace aby3
{






    Sh3Task Sh3Task::then(RoundFunc task)
    {
        Sh3Task ret;
        getRuntime().addTask({ this, 1 }, ret, std::move(task));
        return ret;
    }

    Sh3Task Sh3Task::then(ContinuationFunc task)
    {
        Sh3Task ret;
        getRuntime().addTask({ this, 1 }, ret, std::move(task));
        return ret;
    }

    void Sh3Task::nextRound(RoundFunc task)
    {
        auto base = getRuntime().mTasks.get(mTaskIdx);
        base->mFunc = std::move(task);
    }

    void Sh3Task::get()
    {
        getRuntime().runUntilTaskCompletes(mTaskIdx);
    }

    void Sh3Runtime::addTask(span<Sh3Task> deps, Sh3Task & handle, Sh3Task::RoundFunc&& func)
    {
        auto base = mTasks.push();
        base->mFunc = std::forward<Sh3Task::RoundFunc>(func);
        configureTask(deps, handle, base);
    }


    void Sh3Runtime::addTask(span<Sh3Task> deps, Sh3Task & handle, Sh3Task::ContinuationFunc&& func)
    {
        auto base = mTasks.push();
        base->mFunc = std::forward<Sh3Task::ContinuationFunc>(func);
        configureTask(deps, handle, base);
    }

    void Sh3Runtime::configureTask(span<Sh3Task> deps, Sh3Task & handle, Sh3TaskBase * base)
    {

        if (handle.mTaskIdx != -1)
            throw std::runtime_error("tasks can not be reused. " LOCATION);

        handle.mTaskIdx = base->mIdx;
        handle.mRuntime = this;

        base->mDepCount = deps.size();

        for (i64 i = 0; i < deps.size(); ++i)
        {
            auto dIdx = deps[i].mTaskIdx;
            if (dIdx >= mTasks.mPopIdx)
            {
                auto depBase = mTasks.get(dIdx);
                if (depBase == nullptr)
                    throw std::runtime_error(LOCATION);
                depBase->addChild(base);
            }
        }
    }

    void Sh3Runtime::runUntilTaskCompletes(i64 taskIdx)
    {
        if (taskIdx < mTasks.mPopIdx)
            return;

        auto taskBase = mTasks.get(taskIdx);
        if (taskBase == nullptr)
            throw std::runtime_error(LOCATION);

        while (taskBase->isCompleted() == false)
        {
            runOne();
        }
    }

    void Sh3Runtime::runOne()
    {
        Sh3Task t;
        t.mRuntime = this;
        if (mTasks.size())
        {

            auto task = mTasks.get(mTasks.mPopIdx);
            while (task)
            {
                runTask(task);
                task = mTasks.getNext(task->mIdx + 1);
            }

        }
    }

    void Sh3Runtime::runAll()
    {
        while (mTasks.size())
            runOne();
    }

    void Sh3Runtime::runTask(Sh3TaskBase * task)
    {

        Sh3Task::RoundFunc* roundFuncPtr = boost::get<Sh3Task::RoundFunc>(&task->mFunc);

        Sh3Task t;
        t.mRuntime = this;
        t.mTaskIdx = task->mIdx;

        if (roundFuncPtr)
        {
            auto func = std::move(*roundFuncPtr);
            task->mFunc = Sh3TaskBase::EmptyState{};
            func(mComm, t);
        }
        else
        {
            Sh3Task::ContinuationFunc* contFuncPtr = boost::get<Sh3Task::ContinuationFunc>(&task->mFunc);
            if (contFuncPtr == nullptr)
                throw std::runtime_error(LOCATION);

            auto func = std::move(*contFuncPtr);
            task->mFunc = Sh3TaskBase::EmptyState{};
            func(t);

            // user is not allowed to call nextRound(...) from a continuation task.
            if (task->isCompleted() == false)
                throw std::runtime_error("nextRound(...) not impl for continuation tasks");
        }


        if (task->isCompleted())
        {
            for (u64 i = 0; i < task->mChildren.size(); ++i)
            {
                auto child = mTasks.get(task->mChildren[i]);
                if (child == nullptr)
                    throw std::runtime_error(LOCATION);

                if (child->isReady())
                    throw std::runtime_error(LOCATION);

                --child->mDepCount;

                // check if the child is ready and can be performed this round.
                // these are called continuation tasks.
                if (child->isReady() && child->isContinuationTask())
                {
                    runTask(child);
                }
            }

            mTasks.pop(task->mIdx);
        }

    }

}


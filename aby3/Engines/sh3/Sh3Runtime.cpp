#include "Sh3Runtime.h"


namespace aby3
{






    Sh3Task Sh3Task::then(std::function<void(Sh3::CommPkg&comm, Sh3Task&self)> task)
    {
        Sh3Task ret;
        getRuntime().addTask({ this, 1 }, ret, std::move(task));
        return ret;
    }

    void Sh3Task::nextRound(std::function<void(Sh3::CommPkg&comm, Sh3Task&self)> task)
    {
        auto base = getRuntime().mTasks.get(mTaskIdx);
        base->mFunc = std::move(task);
    }

    void Sh3Task::get()
    {
        getRuntime().runUntilTaskCompletes(mTaskIdx);
    }

    void Sh3Runtime::addTask(span<Sh3Task> deps, Sh3Task & handle, std::function<void(Sh3::CommPkg&comm, Sh3Task&self)> func)
    {
        if (handle.mTaskIdx != -1)
            throw std::runtime_error("tasks can not be reused. " LOCATION);


        auto base = mTasks.push();

        handle.mTaskIdx = base->mIdx;
        handle.mRuntime = this;

        base->mDepCount = deps.size();
        base->mFunc = std::move(func);

        for (u64 i = 0; i < deps.size(); ++i)
        {
            if (deps[i].mTaskIdx != -1)
            {
                auto depBase = mTasks.get(deps[i].mTaskIdx);
                if (depBase == nullptr)
                    throw std::runtime_error(LOCATION);
                depBase->addChild(base);
            }
        }

        //if (base->mDepCount == 0)
        //{
        //    makeActive(base->mIdx);
        //}
    }

    void Sh3Runtime::runUntilTaskCompletes(i64 taskIdx)
    {
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
                auto func = std::move(task->mFunc);
                t.mTaskIdx = task->mIdx;
                func(mComm, t);

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
                    }

                    mTasks.pop(task->mIdx);
                }

                task = mTasks.getNext(task->mIdx + 1);
            }

        }
    }

    void Sh3Runtime::runAll()
    {
        while (mTasks.size())
            runOne();
    }

}


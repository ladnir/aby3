#pragma once
#include <vector>
#include <unordered_map>
#include "aby3/Common/Defines.h"

namespace aby3
{
    enum class Type
    {
        Round,
        Continuation
    };
    class Scheduler;


    struct Task
    {
    public:
        u64 mTaskIdx;
        Scheduler* mSched;

    };


    class TaskBase
    {
    public:
        Type mType;
        u64 mIdx;

        TaskBase(Type t, u64 idx) : mType(t), mIdx(idx) {};

        std::vector<u64> mUpstream, mDownstream, mClosures;

        void addDownstream(u64 idx)
        {
            auto& ds = mDownstream;
            if (std::find(ds.begin(), ds.end(), idx) == ds.end())
                ds.push_back(idx);
        }

        void addUpstream(u64 idx)
        {
            auto& ds = mUpstream;
            if (std::find(ds.begin(), ds.end(), idx) == ds.end())
                ds.push_back(idx);
        }

        void removeUpstream(u64 idx)
        {
            auto& ds = mUpstream;
            auto iter = std::find(ds.begin(), ds.end(), idx);
            if (iter == ds.end())
                throw RTE_LOC;

            std::swap(*iter, *(ds.end() - 1));
            ds.resize(ds.size() - 1);
        }

        void addClosure(u64 idx)
        {
            auto& ds = mClosures;
            if (std::find(ds.begin(), ds.end(), idx) == ds.end())
                ds.push_back(idx);
        }
    };


    class Scheduler
    {
    public:
        u64 mTaskIdx = 1;
        std::unordered_map<u64, TaskBase> mTasks;

        std::list<u64> mReady, mNextRound;

        void addReady(u64 idx)
        {

            if (idx > mTaskIdx)
                throw RTE_LOC;

            auto iter = mTasks.find(idx);
            if (iter == mTasks.end())
                throw RTE_LOC;

            if (iter->second.mUpstream.size())
                throw RTE_LOC;

            mReady.push_back(idx);
        }

        void addNextRound(u64 idx)
        {
            if (idx > mTaskIdx)
                throw RTE_LOC;

            auto iter = mTasks.find(idx);
            if (iter == mTasks.end())
                throw RTE_LOC;

            if (iter->second.mUpstream.size())
                throw RTE_LOC;

            mNextRound.push_back(idx);
        }


        Task addTask(Type t, Task dep)
        {
            return addTask(t, span<Task>(&dep, 1));
        }

        Task addTask(Type t, const std::vector<Task>& deps)
        {
            return addTask(t, span<Task>((Task*)deps.data(), (int)deps.size()));
        }

        Task addTask(Type t, span<Task> deps)
        {
            auto idx = mTaskIdx++;
            auto x = mTasks.emplace(idx, TaskBase(t, idx));


            for (auto& d : deps)
            {
                if (d.mSched != this)
                    throw RTE_LOC;

                auto db = mTasks.find(d.mTaskIdx);

                if (db != mTasks.end())
                {
                    db->second.addDownstream(idx);
                    x.first->second.addUpstream(d.mTaskIdx);
                }
                else
                {
                    if (d.mTaskIdx >= idx)
                        throw std::runtime_error("This tasks idx in invalid");
                }
            }

            if (x.first->second.mUpstream.size() == 0)
            {
                addReady(idx);
            }

            return { idx, this };
        }


        Task addClosure(Task dep)
        {
            return addClosure(span<Task>((Task*)&dep, 1));
        }
        Task addClosure(const std::vector<Task>& deps)
        {
            return addClosure(span<Task>((Task*)deps.data(), deps.size()));
        }

        Task addClosure(span<Task> deps)
        {
            auto idx = mTaskIdx++;
            auto x = TaskBase(Type::Continuation, idx);


            for (auto& d : deps)
            {
                if (d.mSched != this)
                    throw RTE_LOC;

                auto db = mTasks.find(d.mTaskIdx);

                if (db != mTasks.end())
                {
                    db->second.addClosure(idx);
                    x.addUpstream(d.mTaskIdx);
                }
                else
                {
                    if (d.mTaskIdx >= idx)
                        throw std::runtime_error("This tasks idx in invalid");
                }
            }

            if (x.mUpstream.size())
            {
                mTasks.emplace(idx, std::move(x));
            }

            return { idx, this };
        }


        Task currentTask()
        {
            if (mReady.size() == 0)
                std::swap(mReady, mNextRound);

            if (mReady.size() == 0)
                throw RTE_LOC;

            auto idx = mReady.front();
            return { idx, this };
        }

        void popTask()
        {
            if (mReady.size() == 0)
                throw RTE_LOC;
            auto idx = mReady.front();
            removeTask(idx);
            mReady.pop_front();
        }


        void removeTask(u64 idx)
        {

            auto task = mTasks.find(idx);
            if (task == mTasks.end())
                throw RTE_LOC;

            if (task->second.mUpstream.size())
                throw RTE_LOC;

            for (auto& d : task->second.mDownstream)
            {
                auto ds = mTasks.find(d);
                if (ds == mTasks.end())
                    throw RTE_LOC;

                ds->second.removeUpstream(idx);
                if (ds->second.mUpstream.size() == 0)
                {
                    if (ds->second.mType == Type::Round)
                        addNextRound(d);
                    else
                        addReady(d);
                }

                for (auto& c : task->second.mClosures)
                {

                    auto cc = mTasks.find(c);
                    if (cc == mTasks.end())
                        throw RTE_LOC;

                    ds->second.addClosure(c);
                    cc->second.addUpstream(d);

                }
            }

            for (auto& c : task->second.mClosures)
            {
                auto cc = mTasks.find(c);
                cc->second.removeUpstream(idx);
                if (cc->second.mUpstream.size() == 0)
                {
                    removeTask(c);
                }
            }

            mTasks.erase(task);
        }

        Task nullTask() {
            return { 0, this }; 
        }

        std::string print()
        {
            std::stringstream ss;
            ss << "=================================\n";
            ss << "ready:";

            auto& rr = mReady.size() ? mReady : mNextRound;
            for (auto r : rr)
            {
                ss << " " << r;
            }
            ss << "\n---------------------------------\n";
            for (auto& x : mTasks)
            {
                ss << x.second.mIdx << "\n"
                    << "\tup:";

                for (auto u : x.second.mUpstream)
                    ss << " " << u;

                ss << "\n\tdw:";
                for (auto u : x.second.mDownstream)
                    ss << " " << u;
                ss << "\n\tcl:";
                for (auto u : x.second.mClosures)
                    ss << " " << u;
                ss << "\n";
            }

            return ss.str();
        }

    };


}
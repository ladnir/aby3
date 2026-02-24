#pragma once
// This file and the associated implementation has been placed in the public domain, waiving all copyright. No restrictions are placed on its use.
#include <list>
#include <chrono>
#include <string>
#include <mutex>
#include <exception>
#include <stdexcept>

namespace osuCrypto
{

    class Timer
    {
    public:

        typedef std::chrono::system_clock::time_point timeUnit;

        std::list<std::pair<timeUnit, std::string>> mTimes;
        bool mLocking;
        std::mutex mMtx;

        Timer(bool locking = false)
            :mLocking(locking)
        {
            reset();
        }

        const timeUnit& setTimePoint(const std::string& msg);


        friend std::ostream& operator<<(std::ostream& out, const Timer& timer);

        void reset();
    };

    extern Timer gTimer;
    class TimerAdapter
    {
    public:
        virtual void setTimer(Timer& timer)
        {
            mTimer = &timer;
        }

        Timer& getTimer()
        {
            if (mTimer)
                return *mTimer;

            throw std::runtime_error("Timer net set. ");
        }

        Timer::timeUnit setTimePoint(const std::string& msg)
        {
            if(mTimer) return getTimer().setTimePoint(msg);
            else return {};
        }

        Timer* mTimer = nullptr;
    };


}

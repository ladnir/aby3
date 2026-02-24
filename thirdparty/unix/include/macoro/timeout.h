#pragma once

#include "stop.h"
#include "task.h"
#include <chrono>
#include "type_traits.h"
#include "result.h"
#include "macoro/macros.h"
#include "macoro/wrap.h"

namespace macoro
{

	struct timeout
	{
		optional<stop_source> mSrc;
		std::unique_ptr<stop_callback> mReg;
		eager_task<> mTask;

		timeout() = default;
		timeout(const timeout&) = delete;
		timeout(timeout&& o)
			: mSrc(o.mSrc) // required to be copy so that the token can still be used.
			, mReg(std::move(o.mReg))
			, mTask(std::move(o.mTask))
		{}

		timeout& operator=(const timeout&) = delete;
		timeout& operator=(timeout&& o) {
			mSrc = o.mSrc; // required to be copy so that the token can still be used.
			mReg = std::move(o.mReg);
			mTask = std::move(o.mTask);
			return *this;
		};


		template<typename Scheduler, typename Rep, typename Per>
		timeout(Scheduler& s,
			std::chrono::duration<Rep, Per> d,
			stop_token token = {})
		{
			reset(s, d, std::move(token));
		}


		template<typename Scheduler, typename Rep, typename Per>
		void reset(Scheduler& s,
			std::chrono::duration<Rep, Per> d,
			stop_token token = {})
		{
			mSrc.emplace();
			if (token.stop_possible())
			{
				mReg.reset(new stop_callback(token, [src = mSrc.value()]() mutable {
					src.request_stop();
				}));
			}

			mTask = make_task(s, mSrc.value(), d);
		}

		void request_stop() {
			mSrc.value().request_stop();
		}

		stop_token get_token() { return mSrc.value().get_token(); }

		stop_source get_source() { return mSrc.value(); }

		auto MACORO_OPERATOR_COAWAIT()
		{
			return mTask.MACORO_OPERATOR_COAWAIT();
		}

	private:

		template<typename Scheduler, typename Rep, typename Per>
		static eager_task<> make_task(Scheduler& s,
			stop_source mSrc,
			std::chrono::duration<Rep, Per> d)
		{
			try {

				co_await(s.schedule_after(d, mSrc.get_token()));
			}
			catch (...) { }
			mSrc.request_stop();

		}
	};


}

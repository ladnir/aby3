#pragma once


namespace macoro
{
	template<typename scheduler>
	auto transfer_to(scheduler& s)
	{
		return s.schedule();
	}
}



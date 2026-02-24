
#include "macoro/coroutine_handle.h"

namespace macoro
{
	struct inline_scheduler
	{
		suspend_never schedule()
		{
			return { };
		}
	};
}
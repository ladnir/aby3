#include <vector>
#include "bench.h"
#include "cryptoTools/Common/TestCollection.h"

void bench(const oc::CLP& cmd)
{
	auto c = cmd;
	c.setDefault("u", "");

	oc::TestCollection bn;

	bn.runIf(c);
} 
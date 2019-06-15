#include "Sh3RuntimeTests.h"
#include <aby3/sh3/Sh3Runtime.h>

using namespace aby3;
using namespace oc;

void Sh3_Runtime_schedule_test(const CLP& cmd)
{
	Sh3Runtime rt;
	rt.mPrint = cmd.isSet("print");

	CommPkg comm;
	rt.init(0, comm);

	int counter = 0;
	auto base = rt.noDependencies();

	auto task0 = base.then([&](CommPkg & _, Sh3Task self)
		{
			if (counter++ != 0)
				throw RTE_LOC;
		}
	,"task0");

	auto task1 = base.then([&](CommPkg & _, Sh3Task self)
		{
			if (counter++ != 1)
				throw RTE_LOC;
		}
	, "task1");

	auto task2 = (task0 && task1).then([&](CommPkg & _, Sh3Task self)
		{
			if (counter++ != 2)
				throw RTE_LOC;

			self.then([&](CommPkg & _, Sh3Task self) {

				if (counter++ != 5)
					throw RTE_LOC;
				}
			, "task2-sub1").then([&](CommPkg& _, Sh3Task self) {

					if (counter++ != 6)
						throw RTE_LOC;
				}
			, "task2-sub2");
		}
	, "task2");

	task2.then([&](Sh3Task self) 
		{
			if (counter++ != 3)
				throw RTE_LOC;
		}
	, "task2-cont.");

	auto task3 = task2.getClosure().then([&](CommPkg & _, Sh3Task self) {

		if (counter++ != 7)
			throw RTE_LOC;
		}
	, "task3");

	task2.get();	

	if (counter++ != 4)
		throw RTE_LOC;

	task3.get();


	if (counter++ != 8)
		throw RTE_LOC;


	base.then([&](CommPkg & _, Sh3Task self) {
		if (counter++ != 9)
			throw RTE_LOC;
		self.then([&](CommPkg & _, Sh3Task self) {

			if (counter++ != 12)
				throw RTE_LOC;
			}
		);
		}
	);

	base.then([&](CommPkg& _, Sh3Task self) {
		if (counter++ != 10)
			throw RTE_LOC;
		self.then([&](CommPkg & _, Sh3Task self) {

			if (counter++ != 13)
				throw RTE_LOC;
			}
		);
		}
	);

	rt.runOneRound();

	if (counter++ != 11)
		throw RTE_LOC;

	rt.runOneRound();


	if (counter++ != 14)
		throw RTE_LOC;

	rt.runAll();

	if (counter++ != 15)
		throw RTE_LOC;

}



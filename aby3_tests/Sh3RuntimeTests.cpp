#include "Sh3RuntimeTests.h"
#include <aby3/sh3/Sh3Runtime.h>

// testing and command line parsing
#include <tests_cryptoTools/UnitTests.h>
// convenience function
#include <cryptoTools/Network/IOService.h>
#include "aby3/sh3/Sh3Runtime.h"
#include "aby3/sh3/Sh3Encryptor.h"
#include "aby3/sh3/Sh3Evaluator.h"
#include "aby3/Common/Task.h"
using namespace aby3;
using namespace oc;

void Task_schedule_test(const oc::CLP& cmd)
{

	Scheduler rt;
	int counter = 0;
	auto base = rt.nullTask();

	auto task0 = rt.addTask(Type::Round, base);
	auto task1 = rt.addTask(Type::Round, base);


	auto task2 = rt.addTask(Type::Round, { task0, task1 });
	/*.then([&](CommPkg& _, Sh3Task self)
		{
			if (counter++ != 2)
				throw RTE_LOC;

			self.then([&](CommPkg& _, Sh3Task self) {

				if (counter++ != 5)
					throw RTE_LOC;
				}
				, "task2-sub1").then([&](CommPkg& _, Sh3Task self) {

					if (counter++ != 6)
						throw RTE_LOC;
					}
				, "task2-sub2");
		}
	, "task2");*/

	auto task1b = rt.addTask(Type::Round, base);;

	auto task2b = rt.addTask(Type::Round, task2);

	//task2.then([&](Sh3Task self)
	//	{
	//		if (counter++ != 3)
	//			throw RTE_LOC;
	//	}
	//, "task2-cont.");

	auto close1 = rt.addClosure(task2);

	auto task3 = rt.addTask(Type::Round, close1);


	if (rt.currentTask().mTaskIdx != task0.mTaskIdx)
		throw RTE_LOC;
	rt.popTask();
	if (rt.currentTask().mTaskIdx != task1.mTaskIdx)
		throw RTE_LOC;
	rt.popTask();
	if (rt.currentTask().mTaskIdx != task1b.mTaskIdx)
		throw RTE_LOC;
	rt.popTask();

	if (rt.currentTask().mTaskIdx != task2.mTaskIdx)
		throw RTE_LOC;

	auto task2c = rt.addTask(Type::Round, task2);

	//std::cout << "1) " << rt.print() << std::endl;
	rt.popTask();
	//std::cout << "2) " << rt.print() << std::endl;


	if (rt.currentTask().mTaskIdx != task2b.mTaskIdx)
		throw RTE_LOC;
	rt.popTask();

	//std::cout << "3) " << rt.print() << std::endl;
	if (rt.currentTask().mTaskIdx != task2c.mTaskIdx)
		throw RTE_LOC;
	rt.popTask();
	if (rt.currentTask().mTaskIdx != task3.mTaskIdx)
		throw RTE_LOC;
	rt.popTask();
	return;

	////auto task3 = task2.getClosure().then([&](CommPkg& _, Sh3Task self) {

	////	if (counter++ != 7)
	////		throw RTE_LOC;
	////	}
	////, "task3");

	////task2.get();

	//if (counter++ != 4)
	//	throw RTE_LOC;

	//task3.get();


	//if (counter++ != 8)
	//	throw RTE_LOC;


	//base.then([&](CommPkg& _, Sh3Task self) {
	//	if (counter++ != 9)
	//		throw RTE_LOC;
	//	self.then([&](CommPkg& _, Sh3Task self) {

	//		if (counter++ != 12)
	//			throw RTE_LOC;
	//		}
	//	);
	//	}
	//);

	//base.then([&](CommPkg& _, Sh3Task self) {
	//	if (counter++ != 10)
	//		throw RTE_LOC;
	//	self.then([&](CommPkg& _, Sh3Task self) {

	//		if (counter++ != 13)
	//			throw RTE_LOC;
	//		}
	//	);
	//	}
	//);

	//rt.runOneRound();

	//if (counter++ != 11)
	//	throw RTE_LOC;

	//rt.runOneRound();


	//if (counter++ != 14)
	//	throw RTE_LOC;

	//rt.runAll();

	//if (counter++ != 15)
	//	throw RTE_LOC;

}

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
			if (counter++ != 4)
				throw RTE_LOC;
		}
	, "task2-cont.");

	auto task3 = task2.getClosure().then([&](CommPkg & _, Sh3Task self) {

		if (counter++ != 7)
			throw RTE_LOC;
		}
	, "task3");

	task2.get();	

	if (counter++ != 3)
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


void setup_samples(
	u64 partyIdx,
	IOService& ios,
	Sh3Encryptor& enc,
	Sh3Evaluator& eval,
	Sh3Runtime& runtime);

// declare testing functions
void and_task_test(u64 player);


//int main(int argc, char** argv)
//{
//	try {
//		CLP cmd(argc, argv);
//
//		u64 player = 0;
//		and_task_test(player);
//		return 0;
//
//	}
//	catch (std::exception & e)
//	{
//		std::cout << e.what() << std::endl;
//	}
//
//	return 0;
//}


Sh3Task localInt(Sh3Task dep, i64 val, si64& dest)
{
	return dep.then([val, &dest](CommPkg& comm, Sh3Task& self) {

		//si64 ret;
		//dest[0] = mShareGen.getShare() + val;

		//comm.mNext.asyncSendCopy(dest[0]);
		//auto fu = comm.mPrev.asyncRecv(dest[1]);

		self.then([](CommPkg& comm, Sh3Task& self) mutable {
			//fu.get();
		});
		}).getClosure();
}
Sh3Task revealAll(Sh3Task dep, si64 dest, i64 val)
{
	dep.then([](CommPkg&, Sh3Task self) { });
	return dep.then([](CommPkg&, Sh3Task self) {});
}

void and_task_test(u64 partyIdx) {
	//std::cout << "testing &'ing input tasks" << std::endl;
	//IOService ios;
	//Sh3Encryptor enc;
	//Sh3Evaluator eval;
	Sh3Runtime runtime;
	aby3::CommPkg comm;
	runtime.init(partyIdx, comm);
	//setup_samples(partyIdx, ios, enc, eval, runtime);

	u64 rows = 10;
	std::vector<u64> values(rows);
	for (u64 i = 0; i < rows; i++) {
		values[i] = 10 * i;
	}
	std::vector<si64> A(rows);


	// &-ing these tasks together fails with runtime not empty!!!!
	Sh3Task task = runtime.noDependencies();
	for (u64 i = 0; i < rows; i++) {
		if (partyIdx == 0) {
			task &= localInt(runtime, values[i], A[i]);
		}
		else {
			task &= localInt(runtime, 0, A[i]);
		}
	}
	task.get();

	// &-ing these tasks together is fine!
	i64 result[10];
	task = runtime.noDependencies();
	for (u64 i = 0; i < rows; ++i)
		task &= revealAll(runtime, A[i], result[i]);
	task.get();

	//for (u64 i = 0; i < rows; ++i)
	//	std::cout << result[i] << ", ";
	//std::cout << std::endl;
}


void setup_samples(
	aby3::u64 partyIdx,
	oc::IOService& ios,
	aby3::Sh3Encryptor& enc,
	aby3::Sh3Evaluator& eval,
	aby3::Sh3Runtime& runtime)
{
	aby3::CommPkg comm;
	switch (partyIdx)
	{
	case 0:
		comm.mNext = oc::Session(ios, "127.0.0.1:1313", oc::SessionMode::Server, "01").addChannel();
		comm.mPrev = oc::Session(ios, "127.0.0.1:1314", oc::SessionMode::Server, "02").addChannel();
		break;
	case 1:
		comm.mNext = oc::Session(ios, "127.0.0.1:1315", oc::SessionMode::Server, "12").addChannel();
		comm.mPrev = oc::Session(ios, "127.0.0.1:1313", oc::SessionMode::Client, "01").addChannel();
		break;
	default:
		comm.mNext = oc::Session(ios, "127.0.0.1:1314", oc::SessionMode::Client, "02").addChannel();
		comm.mPrev = oc::Session(ios, "127.0.0.1:1315", oc::SessionMode::Client, "12").addChannel();
		break;
	}


	enc.init(partyIdx, comm, sysRandomSeed());
	eval.init(partyIdx, comm, sysRandomSeed());
	runtime.init(partyIdx, comm);
}

void Sh3_Runtime_repeatInput_test(const oc::CLP& cmd)
{
	and_task_test(0);

}



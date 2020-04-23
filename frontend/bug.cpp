// testing and command line parsing
#include <tests_cryptoTools/UnitTests.h>
#include <cryptoTools/Common/CLP.h>

std::vector<std::string> playerTag2{ "p", "player" };

// convenience function
#include <cryptoTools/Network/IOService.h>
#include "aby3/sh3/Sh3Runtime.h"
#include "aby3/sh3/Sh3Encryptor.h"
#include "aby3/sh3/Sh3Evaluator.h"
using namespace aby3;
using namespace oc;
using namespace std;

void setup_samples(
	u64 partyIdx,
	IOService& ios,
	Sh3Encryptor& enc,
	Sh3Evaluator& eval,
	Sh3Runtime& runtime);

// declare testing functions
void and_task_test(u64 player);


int bug(int argc, char** argv)
{
	try {
		CLP cmd(argc, argv);

		u64 player;
		if (cmd.isSet(playerTag2)) {
			player = cmd.getOr(playerTag2, -1);
		}
		else {
			std::cout << "You need to specify which player you are" << std::endl;
			return 0;
		}

		and_task_test(player);
		return 0;

	}
	catch (std::exception & e)
	{
		std::cout << e.what() << std::endl;
	}

	return 0;
}


void and_task_test(u64 partyIdx) {
	cout << "testing &'ing input tasks" << endl;
	IOService ios;
	Sh3Encryptor enc;
	Sh3Evaluator eval;
	Sh3Runtime runtime;
	runtime.mPrint = true;
	setup_samples(partyIdx, ios, enc, eval, runtime);

	u64 rows = 2;
	vector<u64> values(rows);
	for (u32 i = 0; i < rows; i++) {
		values[i] = 10 * i;
	}
	std::vector<si64> A(rows);


	// &-ing these tasks together fails with runtime not empty!!!!
	Sh3Task task = runtime.noDependencies();
	for (u32 i = 0; i < rows; i++) {
		if (partyIdx == 0) {
			task &= enc.localInt(runtime, values[i], A[i]);
		}
		else {
			task &= enc.remoteInt(runtime, A[i]);
		}
	}
	task.get();

	// &-ing these tasks together is fine!
	i64 result[10];
	task = runtime.noDependencies();
	for (u64 i = 0; i < rows; ++i)
		task &= enc.revealAll(runtime, A[i], result[i]);
	task.get();

	for (u64 i = 0; i < rows; ++i)
		cout << result[i] << ", ";
	cout << endl;
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
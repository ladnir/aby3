#include "SharedOTTests.h"
#include "aby3/OT/SharedOT.h"
#include <cryptoTools/Network/Session.h>
#include <cryptoTools/Network/IOService.h>

using namespace oc;
//using namespace aby3;

void SharedOT_eval_test(const oc::CLP& cmd)
{
	IOService ios;
	auto chl01 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "01").addChannel();
	auto chl10 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "01").addChannel();
	auto chl02 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "02").addChannel();
	auto chl20 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "02").addChannel();
	auto chl12 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "12").addChannel();
	auto chl21 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "12").addChannel();


	u64 n = cmd.getOr("n", 100);
	u64 t = cmd.getOr("t", 10);

	SharedOT sender, recver, helper;

	sender.setSeed(OneBlock);
	helper.setSeed(OneBlock);


	BitVector choices(n);
	std::vector<i64> recvMsgs(n);
	std::vector<std::array<i64,2>> sendMsgs(n);
	PRNG prng(ZeroBlock);

	for (u64 tt = 0ull; tt < t; ++tt)
	{
		choices.randomize(prng);
		prng.get(sendMsgs.data(), sendMsgs.size());

		sender.send(chl02, sendMsgs);
		helper.help(chl12, choices);
		auto cc = choices;
		//recver.recv(chl20, chl21, std::move(cc), recvMsgs);
		recver.asyncRecv(chl20, chl21, std::move(cc), recvMsgs).get();

		for (u64 i = 0ull; i < n; ++i)
		{
			if (recvMsgs[i] != sendMsgs[i][choices[i]])
				throw RTE_LOC;
		}
	}

}

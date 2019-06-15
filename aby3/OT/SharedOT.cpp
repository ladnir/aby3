#include "SharedOT.h"
using namespace oc;
#include  <cryptoTools/Crypto/PRNG.h>
#include  <cryptoTools/Common/Log.h>
#include <tuple>
void SharedOT::send(
	Channel & chl,
	span<std::array<i64, 2>> m)
{
	if (mIdx == -1)
		throw RTE_LOC;

	std::vector<std::array<i64, 2>> msgs(m.size());

	mAes.ecbEncCounterMode(mIdx, msgs.size(), (block*)msgs[0].data());
	mIdx += msgs.size();

	for (u64 i = 0; i < msgs.size(); ++i)
	{
		msgs[i][0] = msgs[i][0] ^ m[i][0];
		msgs[i][1] = msgs[i][1] ^ m[i][1];
	}
	//
	//auto b = mAes.ecbEncBlock(ZeroBlock);
	//chl.asyncSendCopy(b);

	chl.asyncSend(std::move(msgs));
}

void SharedOT::help(
	oc::Channel & chl,
	const oc::BitVector& choices)
{
	if (mIdx == -1)
		throw RTE_LOC;

	std::vector<i64> mc(choices.size());
	std::vector<std::array<i64, 2>> msgs(choices.size());

	mAes.ecbEncCounterMode(mIdx, msgs.size(), (block*)msgs[0].data());
	mIdx += msgs.size();

	for (u64 i = 0; i < msgs.size(); ++i)
	{
		mc[i] = msgs[i][choices[i]];
	}

	//auto b = mAes.ecbEncBlock(ZeroBlock);
	//chl.asyncSendCopy(b);


	chl.asyncSend(std::move(mc));
}

void SharedOT::setSeed(const oc::block & seed, oc::u64 seedIdx)
{
	mAes.setKey(seed);
	mIdx = seedIdx;
}

void SharedOT::recv(
	oc::Channel & sender,
	oc::Channel & helper,
	const oc::BitVector & choices,
	oc::span<oc::i64> recvMsgs)
{
	std::vector<std::array<i64, 2>> msgs(choices.size());
	std::vector<i64> mc(choices.size());

	//block b0, b1;
	//sender.recv(b0);
	//helper.recv(b1);

	//if (neq(b0, b1))
	//	throw RTE_LOC;

	sender.recv(msgs);
	helper.recv(mc);


	for (u64 i = 0; i < msgs.size(); ++i)
	{
		recvMsgs[i] = msgs[i][choices[i]] ^ mc[i];
	}
}

std::future<void> SharedOT::asyncRecv(oc::Channel & sender, oc::Channel & helper, oc::BitVector && choices, oc::span<oc::i64> recvMsgs)
{
	auto m = std::make_shared<
		std::tuple<
		std::vector<std::array<i64, 2>>,
		std::vector<i64>,
		std::promise<void>,
		oc::BitVector,
		std::atomic<int>
		//,std::vector<block>
		>>();

	std::get<0>(*m).resize(choices.size());
	std::get<1>(*m).resize(choices.size());
	std::get<3>(*m) = std::forward<oc::BitVector>(choices);
	std::get<4>(*m) = 2;

	auto d0 = std::get<0>(*m).data();
	auto d1 = std::get<1>(*m).data();
	auto ret = std::get<2>(*m).get_future();
	//auto& b = std::get<5>(*m);
	//b.resize(2);

	//sender.asyncRecv(b[0]);
	//helper.asyncRecv(b[1]);

	auto cb = [m, recvMsgs]() mutable {
		auto& a = std::get<4>(*m);
		if (--a == 0)
		{
			auto& sendMsg = std::get<0>(*m);
			auto& recvMsg = std::get<1>(*m);
			auto& choices = std::get<3>(*m);
			//auto& b = std::get<5>(*m);

			//if (neq(b[0], b[1]))
			//	throw RTE_LOC;

			//oc::lout << "cb " << sendMsg[0][0] << " " << sendMsg[0][1] << " " << recvMsg[0] << std::endl;
			for (u64 i = 0; i < sendMsg.size(); ++i)
			{
				recvMsgs[i] = sendMsg[i][choices[i]] ^ recvMsg[i];
			}

			std::get<2>(*m).set_value();
		}
	};


	//block b0, b1;

	sender.asyncRecv(d0, choices.size(), cb);
	helper.asyncRecv(d1, choices.size(), cb);

	return ret;
}


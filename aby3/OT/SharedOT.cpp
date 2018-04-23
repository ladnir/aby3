#include "SharedOT.h"
using namespace oc;
#include  <cryptoTools/Crypto/PRNG.h>
#include <tuple>
void SharedOT::send(
	Channel & chl,
	span<std::array<i64, 2>> m)
{
	std::vector<std::array<i64, 2>> msgs(m.size());


	TODO("Make non-zero. INSECURE ");
	mAes.ecbEncCounterMode(0, msgs.size(), (block*)msgs[0].data());
	mIdx += msgs.size();

	for (u64 i = 0; i < msgs.size(); ++i)
	{
		msgs[i][0] = msgs[i][0] ^ m[i][0];
		msgs[i][1] = msgs[i][1] ^ m[i][1];
	}

	chl.asyncSend(std::move(msgs));
}

void SharedOT::help(
	oc::Channel & chl,
	const oc::BitVector& choices)
{
	std::vector<i64> mc(choices.size());
	std::vector<std::array<i64, 2>> msgs(choices.size());

	TODO("Make non-zero. INSECURE ");
	mAes.ecbEncCounterMode(0, msgs.size(), (block*)msgs[0].data());
	mIdx += msgs.size();

	for (u64 i = 0; i < msgs.size(); ++i)
	{
		mc[i] = msgs[i][choices[i]];
	}

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

	sender.recv(msgs);
	helper.recv(mc);


	for (u64 i = 0; i < msgs.size(); ++i)
	{
		recvMsgs[i] = msgs[i][choices[i]] ^ mc[i];
	}
}

std::future<void> SharedOT::asyncRecv(oc::Channel & sender, oc::Channel & helper, const oc::BitVector & choices, oc::span<oc::i64> recvMsgs)
{
	auto m = std::make_shared<
		std::tuple<
		std::vector<std::array<i64, 2>>,
		std::vector<i64>,
		std::promise<void>
		>>();

	std::get<0>(*m).resize(choices.size());
	std::get<1>(*m).resize(choices.size());

	auto d0 = std::get<0>(*m).data();
	auto d1 = std::get<1>(*m).data();
	auto ret = std::get<2>(*m).get_future();

	auto cb = [m, recvMsgs, choices, a = new std::atomic<int>(2)]() mutable {
		if (--*a == 0)
		{
			for (u64 i = 0; i < std::get<0>(*m).size(); ++i)
			{
				recvMsgs[i] = std::get<0>(*m)[i][choices[i]] ^ std::get<1>(*m)[i];
			}

			std::get<2>(*m).set_value();
			delete a;
		}
	};

	sender.asyncRecv(d0, choices.size(), cb);
	helper.asyncRecv(d1, choices.size(), cb);

	return ret;
}


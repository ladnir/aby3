#include "SharedOT.h"
using namespace oc;
#include  <cryptoTools/Crypto/PRNG.h>
#include  <cryptoTools/Common/Log.h>
#include <tuple>
void SharedOT::send(
	Channel & chl,
	span<std::array<i64, 2>> m)
{
	if (mIdx == ~0ull)
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
	if (mIdx == ~0ull)
		throw RTE_LOC;


	const u64 stepSize = 128;
	auto n = choices.size();
	auto n8 = choices.size() / stepSize * stepSize;

	struct Buff
	{
		Buff(u64 nn) 
			: n(nn)
			, ptr(new i64[nn])
		{}
		using pointer = i64*;
		using value_type = u64;
		using size_type = u64;
		u64 n;
		std::unique_ptr<i64[]> ptr;
		i64* data() const { return ptr.get(); }
		u64 size() const { return n; }
	};
	static_assert(oc::is_container<Buff>::value, "");

	Buff mc(n);

	std::array<std::array<i64, 2>, stepSize> msgs;
	u64 i = 0;
	for (; i < n8;)
	{
		mAes.ecbEncCounterMode(mIdx, stepSize, (block*)msgs[0].data());
		mIdx += stepSize;

		for(u64 j =0; j < stepSize; ++j, ++i)
			mc.data()[i] = msgs[j][choices[i]];
	}

	for (; i < n; ++i)
	{
		mAes.ecbEncCounterMode(mIdx, 1, (block*)msgs[0].data());
		mIdx += 1;
		mc.data()[i] = msgs[0][choices[i]];
	}


	//std::vector<std::array<i64, 2>> msgs(choices.size());

	//mAes.ecbEncCounterMode(mIdx, msgs.size(), (block*)msgs[0].data());
	//mIdx += msgs.size();

	//for (u64 i = 0; i < msgs.size(); ++i)
	//{
	//	mc.data()[i] = msgs[i][choices[i]];
	//}

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



SharedOT::AsyncRecv SharedOT::asyncRecv(oc::Channel & sender, oc::Channel & helper, oc::BitVector && choices, oc::span<oc::i64> recvMsgs)
{
	AsyncRecv m;
	m.mState = std::make_shared<AsyncRecv::State>();

	auto size = choices.size();
	m.mState->mV.resize (size);
	m.mState->mV2.resize(size);
	m.mState->mBv = std::move(choices);
	m.mState->mAtomic = 2;

	auto d0 = m.mState->mV.data();
	auto d1 = m.mState->mV2.data();
	//auto ret = std::get<2>(*m).get_future();
	//auto& b = std::get<5>(*m);
	//b.resize(2);

	//sender.asyncRecv(b[0]);
	//helper.asyncRecv(b[1]);

	auto cb = [m, recvMsgs]() mutable {
		auto& a = m.mState->mAtomic;
		if (--a == 0)
		{
			auto& sendMsg = m.mState->mV;
			auto& recvMsg = m.mState->mV2;
			auto& choices = m.mState->mBv;
			//auto& b = std::get<5>(*m);

			//if (neq(b[0], b[1]))
			//	throw RTE_LOC;

			//oc::lout << "cb " << sendMsg[0][0] << " " << sendMsg[0][1] << " " << recvMsg[0] << std::endl;
			for (u64 i = 0; i < sendMsg.size(); ++i)
			{
				recvMsgs[i] = sendMsg[i][choices[i]] ^ recvMsg[i];
			}

			m.mState->mProm.set_value();
			//std::get<2>(*m).set_value();
		}
	};


	//block b0, b1;

	sender.asyncRecv(d0, size, cb);
	helper.asyncRecv(d1, size, cb);

	return m;
}


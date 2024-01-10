#pragma once
#include <cryptoTools/Network/Channel.h>
#include <cryptoTools/Common/Defines.h>
#include <cryptoTools/Common/BitVector.h>

class SharedOT
{
public:

	oc::AES mAes;
	oc::u64 mIdx = -1;

	void setSeed(const oc::block& seed, oc::u64 seedIdx = 0);

	void send(
		oc::Channel& recver,
		oc::span<std::array<oc::i64,2>> msgs);

	void help(
		oc::Channel& recver,
		const oc::BitVector& choices);

	static void recv(
		oc::Channel& sender,
		oc::Channel & helper,
		const oc::BitVector& choices,
		oc::span<oc::i64> recvMsgs);

	struct AsyncRecv
	{
		struct State
		{
			std::vector<std::array<oc::i64, 2>> mV;
			std::vector<oc::i64> mV2;
			std::promise<void> mProm;
			oc::BitVector mBv;
			std::atomic<int> mAtomic;
		};

		std::shared_ptr<State> mState;

		void get() const { mState->mProm.get_future().get(); }
	};
	static AsyncRecv asyncRecv(
		oc::Channel& sender,
		oc::Channel & helper,
		oc::BitVector&& choices,
		oc::span<oc::i64> recvMsgs);
};

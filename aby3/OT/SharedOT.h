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


	static std::future<void> asyncRecv(
		oc::Channel& sender,
		oc::Channel & helper,
		const oc::BitVector& choices,
		oc::span<oc::i64> recvMsgs);
};

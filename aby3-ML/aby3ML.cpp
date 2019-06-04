#include "aby3ML.h"
#include <cryptoTools/Crypto/PRNG.h>

void aby3::aby3ML::init(u64 partyIdx, oc::Session& prev, oc::Session& next, oc::block seed)
{
	mPreproPrev = prev.addChannel();
	mPreproNext = next.addChannel();
	mPrev = prev.addChannel();
	mNext = next.addChannel();

	CommPkg c{ mPrev, mNext };
	mRt.init(partyIdx, c);

	oc::PRNG prng(seed);
	mEnc.init(partyIdx, c, prng.get<block>());
	mEval.init(partyIdx, c, prng.get<block>());
}

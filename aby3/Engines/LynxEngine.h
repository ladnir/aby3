#pragma once
#include "aby3/Engines/Lynx/LynxDefines.h"
#include "aby3/OT/SharedOT.h"

namespace Lynx
{
	class Piecewise;


	class Engine 
	{
	public:
		Engine();
		~Engine();

		typedef Lynx::Share Share;
		typedef Lynx::ShareRef ShareRef;
		typedef Lynx::Word Word;
		typedef Lynx::value_type value_type;
		typedef Lynx::value_type_matrix value_type_matrix;
		typedef Lynx::word_matrix word_matrix;
		typedef Lynx::Matrix Matrix;
		typedef Lynx::ShareType ShareType;


		u64 mPartyIdx, mDecimal, mNextIdx, mPrevIdx;
		Channel mNext, mPrev;
		Channel mPreproNext, mPreproPrev;
		PRNG mPrng, mP2P3Prng;

		void init(u64 partyIdx, Session& prev, Session& next, u64 decimal, block seed);

		void sync();

		Share localInput(value_type value);
		Share remoteInput(u64 partyIdx);


        void localInput(const value_type_matrix& value, Matrix& dest);
        void localInput(const word_matrix& value, Matrix& dest);
        void localBinaryInput(const word_matrix& value, Matrix& dest);
		void remoteInput(u64 partyIdx, Matrix& dest);
        void remoteBinaryInput(u64 partyIdx, Matrix& dest);

		value_type reveal(const Share& share, ShareType type = ShareType::Arithmetic);
		value_type_matrix reveal(
			const Matrix& A, ShareType type = ShareType::Arithmetic);

        
		word_matrix reconstructShare(
            const Matrix& A, ShareType type = ShareType::Arithmetic);


		word_matrix reconstructShare(
			const word_matrix& A, ShareType type = ShareType::Arithmetic);

        word_matrix reconstructShare(
            const PackedBinMatrix& A, u64 partyIdx);

		Matrix mul(
			const Matrix& A,
			const Matrix& B);


		Matrix mulTruncate(
			const Matrix& A,
			const Matrix& B,
			u64 truncation);

		Matrix arithmBinMul(
			const Matrix& ArithmaticShares,
			const Matrix& BinaryShares);

		Lynx::CompletionHandle asyncArithBinMul(
			const Matrix& Arithmatic,
			const Matrix& Bin,
			Matrix& C);

		Lynx::CompletionHandle asyncArithBinMul(
			const Word& Arithmatic,
			const Matrix& Bin,
			Matrix& C);

        Lynx::CompletionHandle asyncConvertToPackedBinary(
            const Matrix& arithmetic,
            PackedBinMatrix& binaryShares, 
            CommPackage commPkg);


		Word getShare();
		Word getBinaryShare();


		Word valueToWord(const value_type& v);
		inline Word shareToWord(const Share& s, const Word& s3, ShareType type = ShareType::Arithmetic)
		{
			if (type == ShareType::Arithmetic)
			{
				return Word(s[0] + s[1] + s3);
			}
			else
			{
				return Word(s[0] ^ s[1] ^ s3);
			}
		}
		value_type wordToValue(const Word&);

		value_type shareToValue(const Share& s, const Word& s3, ShareType type = ShareType::Arithmetic);


		Matrix argMax(const Matrix& Y);
		Matrix extractSign(const Matrix& Y);

		BetaLibrary mCirLibrary;

		

		Lynx::CompletionHandle asyncMulTruncate(
			const Matrix& A,
			const Matrix& B,
			Matrix& C,
			u64 truncation);


		void preprocess(u64 num);
		void asyncPreprocess(u64 num, std::function<void()> callback);

		PRNG __TruncationPrng;
		struct TruncationPair{
			TruncationPair() = default;
			TruncationPair(TruncationPair&&) = default;
			Lynx::word_matrix mLongShare;
			Lynx::Matrix mShortShare;
		};
		TruncationPair getTruncationPair(u64 rows, u64 cols, u64 dec, Lynx::word_matrix* r = nullptr);


		//std::unordered_map<int, std::unordered_map<int, int>>  preprocMap;
		static BetaCircuit  makePreprocCircuit(u64 dec);
		static BetaCircuit  makeArgMaxCircuit(u64 dec, u64 numArgs);
		BetaCircuit mPreprocCircuit, mArgMax;
		Matrix mPreprocessMtx0;
		Matrix mPreprocessMtx1;

	private:
		std::array<std::vector<block>, 2> mShareBuff;
		u64 mShareIdx;
		std::array<AES, 2> mShareGen;
		u64 mShareGenIdx;

		void refillBuffer();
		std::vector<block> sharedMem;

		SharedOT mOtP01, mOtP02, mOtP10, mOtP12;

	};

}



inline Lynx::Engine::Word Lynx::Engine::getShare()
{
	if (mShareIdx + sizeof(i64) > mShareBuff[0].size() * sizeof(block))
	{
		refillBuffer();
	}

	i64 ret
		= *(u64*)((u8*)mShareBuff[0].data() + mShareIdx)
		- *(u64*)((u8*)mShareBuff[1].data() + mShareIdx);

	mShareIdx += sizeof(i64);

	//ret = 0;
	return ret;
}

inline Lynx::Engine::Word Lynx::Engine::getBinaryShare()
{
	if (mShareIdx + sizeof(i64) > mShareBuff[0].size() * sizeof(block))
	{
		refillBuffer();
	}

	i64 ret
		= *(u64*)((u8*)mShareBuff[0].data() + mShareIdx)
		^ *(u64*)((u8*)mShareBuff[1].data() + mShareIdx);

	mShareIdx += sizeof(i64);

	//ret = 0;
	return ret;
}

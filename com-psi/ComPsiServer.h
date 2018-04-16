#pragma once

#include <cryptoTools/Network/Channel.h>
#include <cryptoTools/Crypto/PRNG.h>
#include <cryptoTools/Common/MatrixView.h>
#include "aby3/Engines/sh3/Sh3BinaryEvaluator.h"
#include "aby3/Engines/sh3/Sh3Encryptor.h"
#include "aby3/Engines/sh3/Sh3Evaluator.h"
#include "LowMC.h"

namespace osuCrypto
{

    class Table
    {
    public:
        
        struct Column
        {
            u64 mBitLenth;
            aby3::Sh3::i64Matrix mData;
        };

        aby3::Sh3::i64Matrix mKeys;
    };

    class SharedTable
    {
    public:

        // shared keys are stored in packed binary format. i.e. XOR shared and trasposed.
        //aby3::Sh3::sPackedBin mKeys;
        aby3::Sh3::sbMatrix mKeys;

        u64 rows();
    };


    class ComPsiServer
    {
    public:
        u64 mIdx, mKeyBitCount = 80;
        Channel mNext, mPrev;
        PRNG mPrng;

        aby3::Sh3Runtime mRt;
        aby3::Sh3Encryptor mEnc;

        void init(u64 idx, Session& prev, Session& next);


        SharedTable localInput(Table& t);
        SharedTable remoteInput(u64 partyIdx, u64 numRows);


        SharedTable intersect(SharedTable& A, SharedTable& B);




        aby3::Sh3::i64Matrix computeKeys(span<SharedTable*> tables, span<u64> reveals);



        //LowMC2<> mLowMC;
        BetaCircuit mLowMCCir;
    };

}
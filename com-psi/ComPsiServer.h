#pragma once

#include <cryptoTools/Network/Channel.h>
#include <cryptoTools/Crypto/PRNG.h>
#include <cryptoTools/Common/MatrixView.h>
#include "aby3/Engines/LynxEngine.h"
#include "aby3/Engines/Lynx/LynxBinaryEngine.h"
#include "LowMC.h"

namespace osuCrypto
{

    class Table
    {
    public:
        
        struct Column
        {
            u64 mBitLenth;
            std::vector<Lynx::Word> mData;
        };

        //u64 mKeyBitLength;
        Lynx::word_matrix mKeys;
        //std::vector<Column> mCols;
    };

    class SharedTable
    {
    public:
        Lynx::Matrix mKeys;

        u64 rows();
    };


    class ComPsiServer
    {
    public:
        u64 mIdx;
        Channel mNext, mPrev;
        PRNG mPrng;

        Lynx::Engine mEng;

        void init(u64 idx, Session& next, Session& prev);


        SharedTable input(Table& t);
        SharedTable input(u64 idx);


        SharedTable intersect(SharedTable& A, SharedTable& B);




        std::vector<block> computeKeys(span<SharedTable*> tables, span<u64> reveals);


        std::vector<Lynx::Matrix> getRandOprfKey();


        LowMC2<> mLowMC;
        BetaCircuit mLowMCCir;
    };

}
//#pragma once
//#include "cryptoTools/Common/Defines.h"
//#include "cryptoTools/Common/Log.h"
//#include "cryptoTools/Common/BitVector.h"
//
//#include "cryptoTools/Common/Matrix.h"
////#include <mutex>
//#include <atomic>
////#define THREAD_SAFE_CUCKOO
//#include "cryptoTools/Common/CuckooIndex.h"
//
//namespace osuCrypto
//{
//
//    class SimpleCuckoo
//    {
//    public:
//        SimpleCuckoo();
//        ~SimpleCuckoo();
//
//        struct Bin
//        {
//            Bin() :mVal(-1) {}
//            Bin(u64 idx, u64 hashIdx) : mVal(idx | (hashIdx << 56)) {}
//
//            bool isEmpty() const;
//            u64 idx() const;
//            u64 hashIdx() const;
//
//            void swap(u64& idx, u64& hashIdx);
//#ifdef THREAD_SAFE_CUCKOO
//            Bin(const Bin& b) : mVal(b.mVal.load(std::memory_order_relaxed)) {}
//            Bin(Bin&& b) : mVal(b.mVal.load(std::memory_order_relaxed)) {}
//            std::atomic<u64> mVal;
//#else
//            Bin(const Bin& b) : mVal(b.mVal) {}
//            Bin(Bin&& b) : mVal(b.mVal) {}
//            u64 mVal;
//#endif
//        };
//        struct Workspace
//        {
//            Workspace(u64 n, u64 h)
//                : curAddrs(n)
//                , curHashIdxs(n)
//                , oldVals(n)
//                , findVal(n, h)
//            {}
//
//            std::vector<u64>
//                curAddrs,
//                curHashIdxs,
//                oldVals;
//
//            Matrix<u64>   findVal;
//        };
//
//
//
//        u64 mTotalTries;
//
//        bool operator==(const SimpleCuckoo& cmp)const;
//        bool operator!=(const SimpleCuckoo& cmp)const;
//
//        //std::mutex mStashx;
//
//        CuckooParam mParams;
//
//        void print() const;
//        void init();
//
//		void insert(span<u64> itemIdxs, span<block> hashs)
//		{
//			Workspace ws(itemIdxs.size(), mParams.mNumHashes);
//			std::vector<block> bb(mParams.mNumHashes);
//			Matrix<u64> hh(hashs.size(), mParams.mNumHashes);
//
//			for (i64 i = 0; i < hashs.size(); ++i)
//			{
//				//AES aes(hashs[i]);
//				//aes.ecbEncCounterMode(0, bb.size(), bb.data());
//				for (u64 j = 0; j < mParams.mNumHashes; ++j)
//				{
//					//hh(i, j) = *(u64*)&bb[j];
//					hh(i,j) = CuckooIndex<>::getHash(hashs[i], j, mParams.numBins());
//				}
//			}
//
//			insertBatch(itemIdxs, hh, ws);
//		}
//		void insertBatch(span<u64> itemIdxs, MatrixView<u64> hashs, Workspace& workspace);
//
//        u64 findBatch(MatrixView<u64> hashes,
//            span<u64> idxs,
//            Workspace& wordkspace);
//
//
//        u64 stashUtilization();
//
//        std::vector<u64> mLocations;
//        MatrixView<u64> mHashesView;
//
//        std::vector<Bin> mBins;
//        std::vector<Bin> mStash;
//
//        //std::vector<Bin> mBins;
//        //std::vector<Bin> mStash;
//
//
//        //void insertItems(std::array<std::vector<block>,4>& hashs);
//    };
//
//}

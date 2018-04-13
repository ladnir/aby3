#include "LynxEngine.h"

#define SHARE_BUFFER_SIZE 1024
#include "cryptoTools/Common/Log.h"
#include "cryptoTools/Crypto/sha1.h"
#include "cryptoTools/Common/Matrix.h"

#include "aby3/OT/SharedOT.h"
#include "aby3/Engines/Lynx/LynxPiecewise.h"
#include "aby3/Engines/Lynx/LynxBinaryEngine.h"
#include "aby3/Circuit/Garble.h"
#include <libOTe/Tools/Tools.h>

namespace Lynx
{
    using namespace oc;
    Engine::Engine()
    {
    }


    Engine::~Engine()
    {
    }

    void Engine::init(u64 partyIdx, Session & prev, Session & next, u64 dec, block seed)
    {
        mPartyIdx = partyIdx;
        mNextIdx = (partyIdx + 1) % 3;
        mPrevIdx = (partyIdx + 2) % 3;
        mDecimal = dec;
        mPrev = prev.addChannel();
        mNext = next.addChannel();
        mPreproPrev = prev.addChannel();
        mPreproNext = next.addChannel();
        mPrng.SetSeed(seed ^ toBlock(323452345 * partyIdx));

        // todo, make this secret between P2P3
        mP2P3Prng.SetSeed(toBlock(124413423));

        mShareGenIdx = 0;
        mShareBuff[0].resize(SHARE_BUFFER_SIZE);
        mShareBuff[1].resize(SHARE_BUFFER_SIZE);

        mShareGen[0].setKey(mPrng.get<block>());

        //ostreamLock(std::cout) << "party " << mPartyIdx << " gen key = " << mShareGen[0].mRoundKey[0] << std::endl;

        mNext.asyncSendCopy((u8*)&mShareGen[0].mRoundKey[0], sizeof(block));
        block key;
        mPrev.recv((u8*)&key, sizeof(block));
        mShareGen[1].setKey(key);


        refillBuffer();

        TODO("Make non-const. INSECURE ");
        mOtP01.setSeed(oc::toBlock(1));
        mOtP02.setSeed(oc::toBlock(1));
        mOtP10.setSeed(oc::toBlock(1));
        mOtP12.setSeed(oc::toBlock(1));


        mPreprocCircuit = makePreprocCircuit(mDecimal);
        mArgMax = makeArgMaxCircuit(mDecimal, 10);

        __TruncationPrng.SetSeed(toBlock(12523543233));
    }

    void Engine::sync()
    {
        mNext.asyncSend((u8*)&mPartyIdx, sizeof(u64));
        mPrev.asyncSend((u8*)&mPartyIdx, sizeof(u64));


        u64 p[1];
        mPrev.recv((u8*)p, sizeof(u64));

        u64 n[1];
        mNext.recv((u8*)n, sizeof(u64));

        if (*n != (mPartyIdx + 1) % 3)
            throw std::runtime_error(LOCATION);

        auto ep = mPartyIdx ? mPartyIdx - 1 : 2;

        if (*p != ep)
            throw std::runtime_error(LOCATION);
    }

    Engine::Share Engine::localInput(value_type value)
    {
        Share ret;
        ret[0] = getShare() + valueToWord(value);

        mNext.asyncSendCopy((u8*)&ret[0], sizeof(Word));
        mPrev.recv((u8*)&ret[1], sizeof(Word));


        //ostreamLock(std::cout) << "party " << mPartyIdx << " share[0] " << ret[0] << " share[1] " << ret[1] << std::endl;
        return ret;
    }

    Engine::Share Engine::remoteInput(u64 partyIdx)
    {
        Share ret;
        ret[0] = getShare();

        mNext.asyncSendCopy((u8*)&ret[0], sizeof(Word));
        mPrev.recv((u8*)&ret[1], sizeof(Word));

        //ostreamLock(std::cout) << "party " << mPartyIdx << " share[0] " << ret[0] << " share[1] " << ret[1] << std::endl;


        return ret;
    }

    void Engine::localInput(const value_type_matrix & value, Matrix & ret)
    {
        if (ret.cols() != value.cols() || ret.size() != value.size())
            throw std::runtime_error(LOCATION);

        for (u64 i = 0; i < ret.mShares[0].size(); ++i)
            ret.mShares[0](i) = getShare() + valueToWord(value(i));

        mNext.asyncSendCopy((u8*)ret.mShares[0].data(), ret.mShares[0].size() * sizeof(Word));
        mPrev.recv((u8*)ret.mShares[1].data(), ret.mShares[1].size() * sizeof(Word));
    }

    void Engine::localInput(const word_matrix & value, Matrix & ret)
    {
        if (ret.cols() != value.cols() || ret.size() != value.size())
            throw std::runtime_error(LOCATION);

        for (u64 i = 0; i < ret.mShares[0].size(); ++i)
            ret.mShares[0](i) = getShare() + value(i);

        mNext.asyncSendCopy((u8*)ret.mShares[0].data(), ret.mShares[0].size() * sizeof(Word));
        mPrev.recv((u8*)ret.mShares[1].data(), ret.mShares[1].size() * sizeof(Word));
    }


    void Engine::localBinaryInput(const word_matrix & value, Matrix & ret)
    {
        if (ret.cols() != value.cols() || ret.size() != value.size())
            throw std::runtime_error(LOCATION);

        for (u64 i = 0; i < ret.mShares[0].size(); ++i)
            ret.mShares[0](i) = getBinaryShare() ^ value(i);

        mNext.asyncSendCopy((u8*)ret.mShares[0].data(), ret.mShares[0].size() * sizeof(Word));
        mPrev.recv((u8*)ret.mShares[1].data(), ret.mShares[1].size() * sizeof(Word));
    }


    void Engine::remoteInput(u64 partyIdx, Matrix & ret)
    {
        for (u64 i = 0; i < ret.mShares[0].size(); ++i)
            ret.mShares[0](i) = getShare();

        mNext.asyncSendCopy((u8*)ret.mShares[0].data(), ret.mShares[0].size() * sizeof(Word));
        mPrev.recv((u8*)ret.mShares[1].data(), ret.mShares[1].size() * sizeof(Word));
    }

    void Engine::remoteBinaryInput(u64 partyIdx, Matrix & ret)
    {
        for (u64 i = 0; i < ret.mShares[0].size(); ++i)
            ret.mShares[0](i) = getBinaryShare();

        mNext.asyncSendCopy((u8*)ret.mShares[0].data(), ret.mShares[0].size() * sizeof(Word));
        mPrev.recv((u8*)ret.mShares[1].data(), ret.mShares[1].size() * sizeof(Word));
    }



    Engine::value_type Engine::reveal(const Share & share, ShareType type)
    {
        mNext.asyncSendCopy((u8*)&(share[1]), sizeof(Word));
        Word s3;
        mPrev.recv((u8*)&s3, sizeof(Word));

        return shareToValue(share, s3, type);
    }

    Eigen::Matrix<Engine::value_type, Eigen::Dynamic, Eigen::Dynamic> Engine::reveal(
        const Matrix& A, ShareType type)
    {
        Eigen::Matrix<Engine::value_type, Eigen::Dynamic, Eigen::Dynamic> ret(A.rows(), A.cols());
        auto words = reconstructShare(A, type);

        for (u64 i = 0; i < A.size(); ++i)
        {
            ret(i) = wordToValue(words(i));
        }
        return ret;
    }

    Engine::word_matrix Engine::reconstructShare(const Matrix & A, ShareType type)
    {
        std::vector<Word> buff(A.size());

        for (u64 i = 0; i < A.size(); ++i)
            buff[i] = A.mShares[1](i);
        mNext.asyncSend(std::move(buff));

#ifdef LYNX_DEBUG
        buff.resize(A.size());
        for (u64 i = 0; i < A.size(); ++i)
            buff[i] = A(i)[0];
        mPrev.asyncSend(std::move(buff));
#endif

        word_matrix ret(A.rows(), A.cols());
        mPrev.recv(buff);

        for (u64 i = 0; i < A.size(); ++i)
        {
            ret(i) = shareToWord(A(i), buff[i], type);
        }

#ifdef LYNX_DEBUG
        std::vector<Word> buff2(A.size());
        mNext.recv(buff2);
        for (u64 i = 0; i < A.size(); ++i)
        {
            if (buff2[i] != buff2[i])
            {
                throw std::runtime_error(LOCATION);
            }
        }
#endif

        return ret;
    }

    Engine::word_matrix Engine::reconstructShare(const word_matrix & A, ShareType type)
    {
        mNext.asyncSendCopy(A.data(), A.size());
        mPrev.asyncSendCopy(A.data(), A.size());

        word_matrix a1, a2;
        a1.resizeLike(A);
        a2.resizeLike(A);
        mNext.recv(a1.data(), a1.size());
        mPrev.recv(a2.data(), a2.size());

        if (type == ShareType::Arithmetic)
            a1 += a2 + A;
        else
            for (size_t i = 0; i < A.size(); i++)
                a1(i) ^= a2(i) ^ A(i);

        return std::move(a1);
    }


    Engine::word_matrix Engine::reconstructShare(
        const PackedBinMatrix& A, u64 partyIdx)
    {
        auto wordWidth = (A.bitCount() + 8 * sizeof(Word) - 1) / (8 * sizeof(Word));
        if (partyIdx == mPartyIdx)
        {
            Engine::word_matrix buff, r;
            buff.resize(A.bitCount(), A.simdWidth());
            r.resize(A.mShareCount, wordWidth);

            mPrev.recv(buff.data(), buff.size());


            for (u64 i = 0; i < buff.size(); ++i)
            {
                buff(i) = buff(i) ^ A.mShares[0](i) ^ A.mShares[1](i);
            }

            MatrixView<u8> bb((u8*)buff.data(), A.bitCount(), A.simdWidth() * sizeof(Word));
            MatrixView<u8> rr((u8*)r.data(), r.rows(), r.cols() * sizeof(Word));
            sse_transpose(bb, rr);
            return r;
        }
        else if (partyIdx == mNextIdx)
        {
            mNext.asyncSendCopy(A.mShares[1].data(), A.mShares[1].size());
        }

        return {};
    }

    std::string toString(u64 partyIdx, const Matrix& A, u64 rowIdx)
    {
        std::stringstream ss;
        ss << "(";
        switch (partyIdx)
        {
        case 0:
            ss << A.mShares[0](rowIdx) << ",                   _, " << A.mShares[1](rowIdx);
            break;
        case 1:
            ss << A.mShares[1](rowIdx) << ", " << A.mShares[0](rowIdx) << ",                   _";
            break;
        case 2:
            ss << "                   _," << A.mShares[1](rowIdx) << ", " << A.mShares[0](rowIdx);
            break;
        default:
            throw std::runtime_error(LOCATION);
        }

        ss << ")";

        return ss.str();
    }

    CompletionHandle Engine::asyncConvertToPackedBinary(
        const Matrix & arithmetic, 
        PackedBinMatrix & binaryShares,
        CommPackage commPkg)
    {
        if (arithmetic.cols() != 1)
            throw std::runtime_error("not impl " LOCATION);

        u64 bitCount = sizeof(Word) * 8;
        BinaryEngine eng(mPartyIdx, commPkg.mPrev, commPkg.mNext);
        auto cir = mCirLibrary.int_int_add(bitCount, bitCount, bitCount, BetaLibrary::Optimized::Depth);

        eng.setCir(cir, arithmetic.rows());

        if (mPartyIdx == 0)
        {
            word_matrix in = arithmetic.mShares[0] + arithmetic.mShares[1];
            
            
            Matrix input; input.resize(in.rows(), in.cols());
            localBinaryInput(in, input);


            //ostreamLock(std::cout) << "0 " << mPartyIdx << " :  "
            //    << toString(mPartyIdx, input, 0) << std::endl;
            
            eng.setInput(0, input);
            input.mShares[0].setZero();
            input.mShares[1].setZero();


            //ostreamLock(std::cout) << "1 " << mPartyIdx << " :  "
            //    << toString(mPartyIdx, input, 0) << std::endl;

            eng.setInput(1, input);
        }
        else
        {
            Matrix input; input.resize(arithmetic.rows(), arithmetic.cols());
            remoteBinaryInput(0, input);

            eng.setInput(0, input);

            //ostreamLock(std::cout) << "0 " << mPartyIdx << " :  "
            //    << toString(mPartyIdx, input, 0) << std::endl;


            if (mPartyIdx == 1)
            {
                input.mShares[0] = arithmetic.mShares[0];
                input.mShares[1].setZero();
            }
            else
            {
                input.mShares[1] = arithmetic.mShares[1];
                input.mShares[0].setZero();
            }
            //ostreamLock(std::cout) << "1 " << mPartyIdx << " :  "
            //    << toString(mPartyIdx, input, 0) << std::endl;
            eng.setInput(1, input);
        }

        eng.evaluate();
        eng.getOutput(0, binaryShares);

        Matrix output(arithmetic.rows(), arithmetic.cols());
        eng.getOutput(0, output);
        ostreamLock(std::cout) << mPartyIdx << " : " << std::hex << output.mShares[0](0) << " " << std::hex <<output.mShares[1](0) << std::endl;

        return {};
    }

    Engine::Matrix
        Engine::mul(
            const Matrix& A,
            const Matrix& B)
    {
        Matrix C;
        asyncMulTruncate(A, B, C, mDecimal).get();
        return std::move(C);


        //C.mShares[0]
        //	= A.mShares[0] * B.mShares[0]
        //	+ A.mShares[0] * B.mShares[1]
        //	+ A.mShares[1] * B.mShares[0];


        //mNext.send(C.mS);
        //mPrev.recv(buff);

        //C.mShares[1].resize(C.mShares[0].rows(), C.mShares[0].cols());

        //for (u64 i = 0; i < C.mShares[0].size(); ++i)
        //{
        //	// cast from a Word to a Word.
        //	C.mShares[1](i) = buff[i];
        //}

        //if (mDecimal)
        //{
        //	throw std::runtime_error(LOCATION);

        //}
    }

    Engine::Matrix Engine::mulTruncate(const Matrix & A, const Matrix & B, u64 truncation)
    {
        Matrix C;
        asyncMulTruncate(A, B, C, mDecimal + truncation).get();
        return std::move(C);
    }

    Engine::Matrix Engine::arithmBinMul(const Matrix & A, const Matrix & B)
    {
        Matrix C;
        asyncArithBinMul(A, B, C).get();
        return std::move(C);
    }

    CompletionHandle Engine::asyncArithBinMul(const Matrix & a, const Matrix & b, Matrix & c)
    {
        switch (mPartyIdx)
        {
        case 0:
        {
            std::vector<std::array<Word, 2>> s0(a.size());
            BitVector c1(a.size());
            for (u64 i = 0; i < s0.size(); ++i)
            {
                auto bb = b.mShares[0](i) ^ b.mShares[1](i);
                if (bb < 0 || bb > 1)
                    throw std::runtime_error(LOCATION);

                auto zeroShare = getShare();

                s0[i][bb] = zeroShare;
                s0[i][bb ^ 1] = a.mShares[1](i) + zeroShare;
                //std::cout << "b=(" << b.mShares[0](i) << ",  , " << b.mShares[1](i) << ")" << std::endl;

                //ostreamLock(std::cout) << "s0[" << i << "] = " << bbb(i) * a.mShares[1](i) << std::endl;
                c1[i] = b.mShares[1](i);
            }
            // share 0: from p0 to p1,p2
            mOtP02.send(mNext, s0);
            mOtP01.send(mPrev, s0);

            // share 1: from p1 to p0,p2 
            mOtP10.help(mPrev, c1);



            auto fu1 = mPrev.asyncRecv(c.mShares[0].data(), c.size()).share();
            Word* dd = c.mShares[1].data();
            auto fu2 = SharedOT::asyncRecv(mNext, mPrev, c1, { dd, i64(c.size()) }).share();

            return { [fu1 = std::move(fu1), fu2 = std::move(fu2)]() mutable {
                fu1.get();
                fu2.get();
            } };
        }
        case 1:
        {
            std::vector<std::array<Word, 2>> s1(a.size());
            BitVector c0(a.size());
            for (u64 i = 0; i < s1.size(); ++i)
            {
                auto bb = b.mShares[0](i) ^ b.mShares[1](i);
                if (bb < 0 || bb > 1)
                    throw std::runtime_error(LOCATION);
                auto ss = getShare();

                s1[i][bb] = ss;
                s1[i][bb ^ 1] = (a.mShares[0](i) + a.mShares[1](i)) + ss;

                //std::cout << "b=(   ," << b.mShares[0](i) << ",   )" << "  " << (a.mShares[0](i) + a.mShares[1](i)) << std::endl;
                //ostreamLock(std::cout) << "s1[" << i << "] = " << bbb(i) * (a.mShares[0](i) + a.mShares[1](i)) << " = b *  (" <<a.mShares[0](i) <<" +  "<<a.mShares[1](i) <<")" << std::endl;

                c0[i] = b.mShares[0](i);
            }

            // share 0: from p0 to p1,p2
            mOtP01.help(mNext, c0);

            // share 1: from p1 to p0,p2 
            mOtP10.send(mNext, s1);
            mOtP12.send(mPrev, s1);


            // share 0: from p0 to p1,p2
            Word* dd = c.mShares[0].data();
            auto fu1 = SharedOT::asyncRecv(mPrev, mNext, c0, { dd, i64(c.size()) }).share();

            // share 1:
            auto fu2 = mNext.asyncRecv(c.mShares[1].data(), c.size()).share();

            return { [fu1 = std::move(fu1), fu2 = std::move(fu2)]() mutable {
                fu1.get();
                fu2.get();
            } };
        }
        case 2:
        {
            BitVector c0(a.size()), c1(a.size());
            std::vector<Word> s0(a.size()), s1(a.size());
            for (u64 i = 0; i < a.size(); ++i)
            {
                c0[i] = b.mShares[1](i);
                c1[i] = b.mShares[0](i);

                s0[i] = s1[i] = getShare();
            }

            // share 0: from p0 to p1,p2
            mOtP02.help(mPrev, c0);
            mNext.asyncSend(std::move(s0));

            // share 1: from p1 to p0,p2 
            mOtP12.help(mNext, c1);
            mPrev.asyncSend(std::move(s1));

            // share 0: from p0 to p1,p2
            Word* dd0 = c.mShares[1].data();
            auto fu1 = SharedOT::asyncRecv(mNext, mPrev, c0, { dd0, i64(c.size()) }).share();

            // share 1: from p1 to p0,p2
            Word* dd1 = c.mShares[0].data();
            auto fu2 = SharedOT::asyncRecv(mPrev, mNext, c1, { dd1, i64(c.size()) }).share();

            return { [fu1 = std::move(fu1), fu2 = std::move(fu2)]() mutable {
                fu1.get();
                fu2.get();
            } };
            break;
        }
        default:
            throw std::runtime_error(LOCATION);
        }
    }




    CompletionHandle Engine::asyncArithBinMul(const Word & a, const Matrix & b, Matrix & c)
    {
        switch (mPartyIdx)
        {
        case 0:
        {
            std::vector<std::array<Word, 2>> s0(b.size());
            for (u64 i = 0; i < s0.size(); ++i)
            {
                auto bb = b.mShares[0](i) ^ b.mShares[1](i);
                auto zeroShare = getShare();

                s0[i][bb] = zeroShare;
                s0[i][bb ^ 1] = a + zeroShare;
            }

            // share 0: from p0 to p1,p2
            mOtP02.send(mNext, s0);
            mOtP01.send(mPrev, s0);

            auto fu1 = mNext.asyncRecv(c.mShares[0].data(), c.size()).share();
            auto fu2 = mPrev.asyncRecv(c.mShares[1].data(), c.size()).share();
            return { [fu1 = std::move(fu1), fu2 = std::move(fu2)]() mutable {
                fu1.get();
                fu2.get();
            } };
        }
        case 1:
        {
            BitVector c0(b.size());
            for (u64 i = 0; i < b.size(); ++i)
            {
                c.mShares[1](i) = getShare();
                c0[i] = b.mShares[0](i);
            }

            // share 0: from p0 to p1,p2
            mOtP10.help(mNext, c0);
            mPrev.asyncSendCopy(c.mShares[1].data(), c.size());

            Word* dd = c.mShares[0].data();
            auto fu1 = SharedOT::asyncRecv(mPrev, mNext, c0, { dd, i64(c.size()) }).share();
            return { [fu1 = std::move(fu1)]() mutable {
                fu1.get();
            } };

        }
        case 2:
        {
            BitVector c0(b.size());
            for (u64 i = 0; i < b.size(); ++i)
            {
                c.mShares[0](i) = getShare();
                c0[i] = b.mShares[1](i);
            }

            // share 0: from p0 to p1,p2
            mOtP02.help(mPrev, c0);
            mNext.asyncSendCopy(c.mShares[0].data(), c.size());

            Word* dd0 = c.mShares[1].data();
            auto fu1 = SharedOT::asyncRecv(mNext, mPrev, c0, { dd0, i64(c.size()) }).share();

            return { [fu1 = std::move(fu1)]() mutable {
                fu1.get();
            } };
            break;
        }
        default:
            throw std::runtime_error(LOCATION);
        }

    }


    CompletionHandle Engine::asyncMulTruncate(
        const Matrix & A,
        const Matrix & B,
        Matrix & prod,
        u64 truncation)
    {
        //word_matrix trueA = reconstructShare(A);
        //word_matrix trueB = reconstructShare(B);



        prod.mShares[0]
            = A.mShares[0] * B.mShares[0]
            + A.mShares[0] * B.mShares[1]
            + A.mShares[1] * B.mShares[0];

        prod.mShares[1].resizeLike(prod.mShares[0]);

        if (truncation)
        {
#define PRE_PROCESS_TRUNCATION
#ifdef PRE_PROCESS_TRUNCATION
            //word_matrix trueR;


            auto r = getTruncationPair(prod.rows(), prod.cols(), truncation);
            auto XYminusR = (prod.mShares[0] - r.mLongShare).eval();

            //if (trueR != reconstructShare(std::get<0>(r)))
            //	throw std::runtime_error(LOCATION);

            //auto pub = reconstructShare(XYminusR);
            //
            //if (pub != trueA * trueB - trueR)
            //{

            //	throw std::runtime_error(LOCATION);
            //}

            // set prod to be the truncated shares of r. party 0,1 will
            // subtract (r-xy)/2^d from it to get 
            //     xy/2^d = (r/2^d) - ((r-xy)/2^d)
            prod = std::move(r.mShortShare);
            //prod.mShares[0] = -prod.mShares[0];
            //prod.mShares[1] = -prod.mShares[1];

            // have party 0,1 reconstruct  r-xy
            if (mNextIdx < 2) mNext.asyncSendCopy(XYminusR.data(), XYminusR.size());
            if (mPrevIdx < 2) mPrev.asyncSendCopy(XYminusR.data(), XYminusR.size());


            if (mPartyIdx < 2)
            {

                std::shared_ptr<std::array<word_matrix, 3>> shares(new std::array<word_matrix, 3>);

                // these will hold the three shares of r-xy
                (*shares)[0].resizeLike(prod.mShares[0]);
                (*shares)[1].resizeLike(prod.mShares[0]);
                (*shares)[2] = std::move(XYminusR);

                // perform the async receives
                auto fu0 = mNext.asyncRecv((*shares)[0].data(), (*shares)[0].size()).share();
                auto fu1 = mPrev.asyncRecv((*shares)[1].data(), (*shares)[1].size()).share();

                // set the completion handle complete the computation
                return { [fu0, fu1, shares, truncation, &prod, this]() mutable
                {
                    fu0.get();
                    fu1.get();

                    // r-xy 
                    (*shares)[0] += (*shares)[1];
                    (*shares)[0] += (*shares)[2];

                    //if ((*shares)[0] != pub)
                    //{
                    //	throw std::runtime_error(LOCATION);
                    //}				
                    //if ((*shares)[0] != trueA * trueB - trueR)
                    //{

                    //	throw std::runtime_error(LOCATION);
                    //}

                    // (r-xy) / 2^d
                    (*shares)[0] /= (Word(1) << truncation);

                    // xy/2^ = (r/2^d) - ((r-xy) / 2^d)
                    prod.mShares[mPartyIdx] += (*shares)[0];

                } };
                //	return { []() {} };

            }
            else
            {
                // party 2 has no-op completion handle...
                return { []() {} };
            }
#else 
            todo, make this async....

                switch (mPartyIdx)
                {
                case 0:
                {
                    auto& c1 = prod.mShares[0];
                    auto& c3 = prod.mShares[1];

                    mNext.send((u8*)c1.data(), c1.size() * sizeof(Word));

                    auto bc = []()
                    {

                    };

                    mPrev.asyncRecv((u8*)c3.data(), c3.size() * sizeof(Word));
                    mNext.asyncRecv((u8*)c1.data(), c1.size() * sizeof(Word));

                    break;
                }
                case 1:
                {
                    auto& c2 = prod.mShares[0];
                    auto& c1 = prod.mShares[1];



                    mPrev.recv((u8*)c1.data(), c1.size() * sizeof(Word));
                    c1 += c2;
                    c1 /= (Word(1) << truncation);


                    mP2P3Prng.get(c2.data(), c2.size());

                    c1 -= c2;
                    mPrev.asyncSendCopy((u8*)c1.data(), c1.size() * sizeof(Word));

                    break;
                }
                case 2:
                {
                    auto& c3 = prod.mShares[0];
                    auto& c2 = prod.mShares[1];

                    c3 /= (Word(1) << truncation);
                    mNext.asyncSendCopy((u8*)c3.data(), c3.size() * sizeof(Word));

                    mP2P3Prng.get(c2.data(), c2.size());
                    break;
            }
                default:
                    throw std::runtime_error(LOCATION);
                    break;
        }
#endif
    }
        else
        {
            //std::vector<Word> buff(prod.mShares[0].size());
            //for (u64 i = 0; i < buff.size(); ++i) {
            //	// cast from a Word to a Word
            //	buff[i] = prod.mShares[0](i);
            //}

            mNext.asyncSendCopy((u8*)prod.mShares[0].data(), prod.mShares[0].size() * sizeof(Word));
            auto fu = mPrev.asyncRecv((u8*)prod.mShares[1].data(), prod.mShares[1].size() * sizeof(Word)).share();


            return { [fu = std::move(fu)](){ fu.get(); } };

            //prod.mShares[1].resize(prod.mShares[0].rows(), prod.mShares[0].cols());
            //for (u64 i = 0; i < prod.mShares[0].size(); ++i) {
            //	// cast from a Word to a Word.
            //	prod.mShares[1](i) = buff[i];
            //}
        }
}



    void Engine::preprocess(u64 num)
    {
        asyncPreprocess(num, []() {});
    }

    void Engine::asyncPreprocess(u64 n, std::function<void()> callback)
    {
        //u64 n = (dimension + batchSize) * numIterations * 2;
        if (n & 1) ++n;


        BinaryEngine eng(mPartyIdx, mPreproPrev, mPreproNext);

        eng.setCir(&mPreprocCircuit, n);

        TODO("properlly seed this.");
        //PRNG prng(ZeroBlock);
        auto& aes = mAesFixedKey;

        Matrix&in0 = mPreprocessMtx0;
        Matrix&in1 = mPreprocessMtx1;
        Matrix&in2 = mPreprocessMtx1;

        in0.resize(n, 1);
        in1.resize(n, 1);
        in2.resize(n, 1);

        aes.ecbEncCounterMode(0 * n, n / 2, (oc::block*) in0.mShares[0].data());
        aes.ecbEncCounterMode(1 * n, n / 2, (oc::block*) in0.mShares[1].data());
        aes.ecbEncCounterMode(2 * n, n / 2, (oc::block*) in1.mShares[1].data());
        aes.ecbEncCounterMode(3 * n, n / 2, (oc::block*) in1.mShares[0].data());
        aes.ecbEncCounterMode(4 * n, n / 2, (oc::block*) in2.mShares[1].data());
        aes.ecbEncCounterMode(5 * n, n / 2, (oc::block*) in2.mShares[0].data());
        //aes.ecbEncCounterMode(3, n / 2, (oc::block*) in1.mShares[1].data());

        eng.setInput(0, in0);
        eng.setInput(1, in1);
        eng.setInput(2, in2);

        eng.evaluate();

        eng.getOutput(0, in0);
        eng.getOutput(1, in1);

        callback();
        //std::vector<std::pair<word_matrix, Matrix>>
    }

    Engine::TruncationPair Engine::getTruncationPair(u64 rows, u64 cols, u64 dec, word_matrix* rr)
    {
        //return mZeroPair;

        {
            //++preprocMap[rows][cols];

            TruncationPair r;
            r.mLongShare.resize(rows, cols);
            r.mLongShare.setZero();
            r.mShortShare.resize(rows, cols);
            r.mShortShare.mShares[0].setZero();
            r.mShortShare.mShares[1].setZero();
            if (rr) *rr = r.mLongShare;

            return r;
        }

        word_matrix rrt(rows, cols);
        std::array<word_matrix, 3> r;
        std::array<Matrix, 3> rt;

        r[0].resize(rows, cols);
        r[1].resize(rows, cols);
        r[2].resize(rows, cols);
        rt[0].resize(rows, cols);
        rt[1].resize(rows, cols);
        rt[2].resize(rows, cols);

        __TruncationPrng.get(r[0].data(), r[0].size());
        __TruncationPrng.get(r[1].data(), r[1].size());


        __TruncationPrng.get(r[2].data(), r[2].size());
        //r[2] = -r[0] - r[1];

        rrt = r[0] + r[1] + r[2];
        if (rr) *rr = rrt;

        for (u64 i = 0; i < rrt.size(); ++i)
        {
            rrt(i) = rrt(i) >> dec;
        }

        __TruncationPrng.get(rt[0].mShares[0].data(), rt[0].mShares[0].size());
        __TruncationPrng.get(rt[1].mShares[0].data(), rt[1].mShares[0].size());

        rt[2].mShares[0] = rrt - rt[0].mShares[0] - rt[1].mShares[0];

        rt[0].mShares[1] = rt[2].mShares[0];
        rt[1].mShares[1] = rt[0].mShares[0];
        rt[2].mShares[1] = rt[1].mShares[0];

        return { std::move(r[mPartyIdx]), std::move(rt[mPartyIdx]) };
    }

    BetaCircuit Engine::makeArgMaxCircuit(u64 dec, u64 numArgs)
    {
        const auto size = 40;// sizeof(Word) * 8;
        BetaCircuit cd;

        std::vector<BetaBundle> a0(numArgs), a1(numArgs);

        std::vector<BetaBundle> a(numArgs);

        //std::vector<std::vector<BetaBundle>> select(log2ceil(numArgs));

        for (u64 i = 0; i < numArgs; ++i)
        {
            a[i].mWires.resize(size);
            a0[i].mWires.resize(size);
            a1[i].mWires.resize(size);

            cd.addInputBundle(a0[i]);
            cd.addInputBundle(a1[i]);
        }


        BetaBundle argMax(log2ceil(numArgs));
        cd.addOutputBundle(argMax);

        // set argMax to zero initially...
        for (auto& w : argMax.mWires) cd.addConst(w, 0);


        BetaBundle t(3);
        cd.addTempWireBundle(t);
        BetaLibrary lib;
        for (u64 i = 0; i < numArgs; ++i)
        {
            cd.addTempWireBundle(a[i]);
            lib.int_int_add_build_so(cd, a0[i], a1[i], a[i], t);

            cd.addPrint("a[" + ToString(i) + "] = ");
            cd.addPrint(a[i]);
            cd.addPrint("\n");
        }

        // maxPointer will equal 1 if the the rhs if greater.
        BetaBundle maxPointer(1);
        maxPointer[0] = argMax[0];
        auto& max = a[0];

        lib.int_int_lt_build(cd, max, a[1], maxPointer);

        for (u64 i = 2; i < numArgs; ++i)
        {
            // currently, max = max(a[0],...,a[i-2]). Now make 
            //		max = max(a[0],...,a[i-1]) 
            //      max = maxPointer ? a[i - 1] : max;
            lib.int_int_multiplex_build(cd, a[i - 1], max, maxPointer, max, t);

            cd.addPrint("max({0, ...," + std::to_string(i - 1) + " }) = ");
            cd.addPrint(max);
            cd.addPrint(" @ ");
            cd.addPrint(argMax);
            cd.addPrint(" * ");
            cd.addPrint(maxPointer);
            cd.addPrint("\n");

            // now compute which is greater (a[i], max) and store the result in maxPointer
            cd.addTempWireBundle(maxPointer);
            lib.int_int_lt_build(cd, max, a[i], maxPointer);

            // construct a const wire bundle that encodes the index i.
            BetaBundle idx(log2ceil(numArgs));
            cd.addConstBundle(idx, BitVector((u8*)&i, log2ceil(numArgs)));

            // argMax = (max < a[i]) ? i : argMax ;
            // argMax =   maxPointer ? i : argMax ;
            lib.int_int_multiplex_build(cd, idx, argMax, maxPointer, argMax, t);
        }

        cd.addPrint("max({0, ...," + std::to_string(numArgs) + " }) = _____ @ ");
        cd.addPrint(argMax);
        cd.addPrint("\n");

        return std::move(cd);
    }



    BetaCircuit Engine::makePreprocCircuit(u64 dec)
    {
        const auto size = sizeof(Word) * 8;
        BetaCircuit cd;

        BetaBundle a(size);
        BetaBundle b0(size);
        BetaBundle c0(size);
        BetaBundle b1(size - dec);
        BetaBundle c1(size - dec);

        cd.addInputBundle(a);
        //std::cout << "a " << a.mWires[0] << "-" << a.mWires.back() << std::endl;
        cd.addInputBundle(b0);
        cd.addInputBundle(b1);
        cd.addOutputBundle(c0);
        cd.addOutputBundle(c1);

        BetaBundle t(3);
        cd.addTempWireBundle(t);

        BetaLibrary lib;
        lib.int_int_add_build_so(cd, a, b0, c0, t);

        a.mWires.erase(a.mWires.begin(), a.mWires.begin() + dec);

        lib.int_int_add_build_so(cd, a, b1, c1, t);

        if (cd.mNonlinearGateCount != 2 * (size - 1) - dec)
            throw std::runtime_error(LOCATION);

        return std::move(cd);
    }





    Engine::Word Engine::valueToWord(const value_type & v)
    {
        return Word(v * (1ULL << mDecimal));
    }

    Engine::value_type Engine::wordToValue(const Word &w)
    {
        return value_type(w) / (1ULL << mDecimal);
    }

    Engine::value_type Engine::shareToValue(
        const Share & s,
        const Word & s3,
        ShareType type)
    {
        return wordToValue(shareToWord(s, s3, type));
        //ostreamLock(std::cout) << "party " << mPartyIdx << " shares " << s[0] << " " << s[1] << " " << s3 << " = " << (s[0] + s[1] + s3) << std::endl;
    }


    Matrix Engine::argMax(const Matrix & Y)
    {
        //BinaryEngine bEng(mPartyIdx, mPrev, mNext);

        //auto cir = mCirLibrary.int_argMax(sizeof(Word) * 8, Y.cols());
        //bEng.init(cir, Y.size());
        std::array<block, 2> tt{ ZeroBlock, ZeroBlock }, zeroAndDelta{ ZeroBlock, CCBlock };
        auto inputSize = 40 * Y.cols();

        if (mPartyIdx == 0)
        {

            for (u64 jj = 0; jj < Y.size(); ++jj)
            {

                std::vector<block> inputs(inputSize);
                mAesFixedKey.ecbEncCounterMode(0, inputs.size(), inputs.data());
                mNext.asyncSend(std::move(inputs));

                sharedMem.resize(mArgMax.mWireCount);

                mAesFixedKey.ecbEncCounterMode(0, inputSize * 2, sharedMem.data());

                std::vector<GarbledGate<2>>gates(mArgMax.mNonlinearGateCount);
                std::vector<u8> shareAuxBits;

                Garble::garble(mArgMax, sharedMem, tt, gates, zeroAndDelta, shareAuxBits);

                if (gates.size())
                    mNext.asyncSend(std::move(gates));
                for (auto bit : shareAuxBits)
                    mNext.asyncSendCopy(&bit, 1);
                shareAuxBits.clear();

            }

        }

        if (mPartyIdx == 1)
        {
            for (u64 jj = 0; jj < Y.size(); ++jj)
            {

                sharedMem.resize(mArgMax.mWireCount);

                mPrev.recv(sharedMem.data(), inputSize);
                mNext.recv(sharedMem.data() + inputSize, inputSize);

                std::vector<GarbledGate<2>> gates;

                if (mArgMax.mNonlinearGateCount)
                {
                    mPrev.recv(gates);
                    Expects(gates.size() == mArgMax.mNonlinearGateCount);
                }

                auto mRecvBit = ([this]() {bool b; mPrev.recv((u8*)&b, 1); return b; });
                Garble::evaluate(mArgMax, sharedMem, tt, gates, mRecvBit);

            }
        }

        if (mPartyIdx == 2)
        {
            for (u64 jj = 0; jj < Y.size(); ++jj)
            {
                std::vector<block> inputs(inputSize);
                mAesFixedKey.ecbEncCounterMode(99999999, inputs.size(), inputs.data());
                mPrev.asyncSend(std::move(inputs));
            }
        }


        return Matrix(Y.rows(), 1);
    }

    Matrix Engine::extractSign(const Matrix & Y)
    {
        std::array<block, 2> tt{ ZeroBlock, ZeroBlock }, zeroAndDelta{ ZeroBlock, CCBlock };
        auto inputSize = 40;
        auto cir = mCirLibrary.int_int_add(inputSize, inputSize, inputSize);

        if (mPartyIdx == 0)
        {

            for (u64 jj = 0; jj < Y.size(); ++jj)
            {

                std::vector<block> inputs(inputSize);
                mAesFixedKey.ecbEncCounterMode(0, inputs.size(), inputs.data());
                mNext.asyncSend(std::move(inputs));
                sharedMem.resize(cir->mWireCount);

                mAesFixedKey.ecbEncCounterMode(0, inputSize * 2, sharedMem.data());

                std::vector<GarbledGate<2>>gates(cir->mNonlinearGateCount);
                std::vector<u8> shareAuxBits;

                Garble::garble(*cir, sharedMem, tt, gates, zeroAndDelta, shareAuxBits);

                if (gates.size())
                    mNext.asyncSend(std::move(gates));
                for (auto bit : shareAuxBits)
                    mNext.asyncSendCopy(&bit, 1);
                shareAuxBits.clear();

            }

        }

        if (mPartyIdx == 1)
        {
            for (u64 jj = 0; jj < Y.size(); ++jj)
            {

                sharedMem.resize(cir->mWireCount);

                mPrev.recv(sharedMem.data(), inputSize);
                mNext.recv(sharedMem.data() + inputSize, inputSize);

                std::vector<GarbledGate<2>> gates;

                if (cir->mNonlinearGateCount)
                {
                    mPrev.recv(gates);
                    Expects(gates.size() == cir->mNonlinearGateCount);
                }

                auto mRecvBit = ([this]() {bool b; mPrev.recv((u8*)&b, 1); return b; });
                Garble::evaluate(*cir, sharedMem, tt, gates, mRecvBit);

            }
        }

        if (mPartyIdx == 2)
        {
            for (u64 jj = 0; jj < Y.size(); ++jj)
            {
                std::vector<block> inputs(inputSize);
                mAesFixedKey.ecbEncCounterMode(99999999, inputs.size(), inputs.data());
                mPrev.asyncSend(std::move(inputs));
            }
        }

        return Matrix(Y.rows(), 1);


    }

    void Engine::refillBuffer()
    {
        mShareGen[0].ecbEncCounterMode(mShareGenIdx, mShareBuff[0].size(), mShareBuff[0].data());
        mShareGen[1].ecbEncCounterMode(mShareGenIdx, mShareBuff[1].size(), mShareBuff[1].data());
        mShareGenIdx += mShareBuff[0].size();
        mShareIdx = 0;
    }




}
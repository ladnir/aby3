#include "Sh3Encryptor.h"
#include <libOTe/Tools/Tools.h>


namespace aby3
{

    //Sh3Task Sh3Encryptor::init(Sh3Runtime& rt)
    //{
    //    auto task = rt.nullTask();
    //    task.then
    //}

    void Sh3Encryptor::complateSharing(Sh3::CommPkg& comm, span<i64> send, span<i64> recv)
    {
        comm.mNext.asyncSendCopy(send);
        comm.mPrev.recv(recv);
    }


    Sh3::si64 Sh3Encryptor::localInt(Sh3::CommPkg & comm, i64 val)
    {
        Sh3::si64 ret;
        ret[0] = mShareGen.getShare() + val;
        

        comm.mNext.asyncSendCopy(ret[0]);
        comm.mPrev.recv(ret[1]);

        return ret;
    }

    Sh3::si64 Sh3Encryptor::remoteInt(Sh3::CommPkg & comm)
    {
        return localInt(comm, 0);
    }

    Sh3::sb64 Sh3Encryptor::localBinary(Sh3::CommPkg & comm, i64 val)
    {
        Sh3::sb64 ret;
        ret[0] = mShareGen.getBinaryShare() ^ val;

        comm.mNext.asyncSendCopy(ret[0]);
        comm.mPrev.recv(ret[1]);

        return ret;
    }

    Sh3::sb64 Sh3Encryptor::remoteBinary(Sh3::CommPkg & comm)
    {
        return localBinary(comm, 0);
    }

    void Sh3Encryptor::localIntMatrix(Sh3::CommPkg & comm, const Sh3::i64Matrix & m, Sh3::si64Matrix & ret)
    {
        if (ret.cols() != m.cols() || ret.size() != m.size())
            throw std::runtime_error(LOCATION);

        for (u64 i = 0; i < ret.mShares[0].size(); ++i)
            ret.mShares[0](i) = mShareGen.getShare() + m(i);

        comm.mNext.asyncSendCopy(ret.mShares[0].data(), ret.mShares[0].size());
        comm.mPrev.recv(ret.mShares[1].data(), ret.mShares[1].size());
    }

    void Sh3Encryptor::remoteIntMatrix(Sh3::CommPkg & comm, Sh3::si64Matrix & ret)
    {

        for (u64 i = 0; i < ret.mShares[0].size(); ++i)
            ret.mShares[0](i) = mShareGen.getShare();

        comm.mNext.asyncSendCopy(ret.mShares[0].data(), ret.mShares[0].size());
        comm.mPrev.recv(ret.mShares[1].data(), ret.mShares[1].size());
    }


    void Sh3Encryptor::localBinMatrix(Sh3::CommPkg & comm, const Sh3::i64Matrix & m, Sh3::sb64Matrix & ret)
    {
        auto b0 = ret.cols() != m.cols();
        auto b1 = ret.size() != m.size();
        if (b0 || b1)
            throw std::runtime_error(LOCATION);

        for (u64 i = 0; i < ret.mShares[0].size(); ++i)
            ret.mShares[0](i) = mShareGen.getBinaryShare() ^ m(i);

        comm.mNext.asyncSendCopy(ret.mShares[0].data(), ret.mShares[0].size());
        comm.mPrev.recv(ret.mShares[1].data(), ret.mShares[1].size());
    }

    void Sh3Encryptor::remoteBinMatrix(Sh3::CommPkg & comm, Sh3::sb64Matrix & ret)
    {
        for (u64 i = 0; i < ret.mShares[0].size(); ++i)
            ret.mShares[0](i) = mShareGen.getBinaryShare();

        comm.mNext.asyncSendCopy(ret.mShares[0].data(), ret.mShares[0].size());
        comm.mPrev.recv(ret.mShares[1].data(), ret.mShares[1].size());
    }

    void Sh3Encryptor::localPackedBinary(Sh3::CommPkg & comm, const Sh3::i64Matrix& m, Sh3::sPackedBin & dest)
    {
        if (dest.bitCount() != m.cols() * sizeof(i64) * 8)
            throw std::runtime_error(LOCATION);
        if (dest.shareCount() != m.rows())
            throw std::runtime_error(LOCATION);

        auto bits = sizeof(i64) * 8;
        auto outRows = dest.bitCount(); 
        auto outCols = (dest.shareCount() + bits - 1) / bits;
        oc::MatrixView<u8> in((u8*)m.data(), m.rows(), m.cols() * sizeof(i64));
        oc::MatrixView<u8> out((u8*)dest.mShares[0].data(), outRows, outCols * sizeof(i64));
        oc::sse_transpose(in, out);

        for (u64 i = 0; i < dest.mShares[0].size(); ++i)
            dest.mShares[0](i) = dest.mShares[0](i) ^ mShareGen.getBinaryShare();

        comm.mNext.asyncSendCopy(dest.mShares[0].data(), dest.mShares[0].size());
        comm.mPrev.recv(dest.mShares[1].data(), dest.mShares[1].size());
    }

    void Sh3Encryptor::remotePackedBinary(Sh3::CommPkg & comm, Sh3::sPackedBin & dest)
    {
        for (u64 i = 0; i < dest.mShares[0].size(); ++i)
            dest.mShares[0](i) =  mShareGen.getBinaryShare();

        comm.mNext.asyncSendCopy(dest.mShares[0].data(), dest.mShares[0].size());
        comm.mPrev.recv(dest.mShares[1].data(), dest.mShares[1].size());
    }

    i64 Sh3Encryptor::reveal(Sh3::CommPkg & comm, const Sh3::si64 & x)
    {
        i64 s;
        comm.mNext.recv(s);
        return s + x[0] + x[1];
    }

    i64 Sh3Encryptor::revealAll(Sh3::CommPkg & comm, const Sh3::si64 & x)
    {
        reveal(comm, x, (mPartyIdx + 2) % 3);
        return reveal(comm, x);
    }

    void Sh3Encryptor::reveal(Sh3::CommPkg & comm, const Sh3::si64 & x, u64 partyIdx)
    {
        auto p = ((mPartyIdx + 2)) % 3;
        if(p == partyIdx)
            comm.mPrev.asyncSendCopy(x[0]);
    }

    i64 Sh3Encryptor::reveal(Sh3::CommPkg & comm, const Sh3::sb64 & x)
    {
        i64 s;
        comm.mNext.recv(s);
        return s ^ x[0] ^ x[1];
    }

    i64 Sh3Encryptor::revealAll(Sh3::CommPkg & comm, const Sh3::sb64 & x)
    {
        reveal(comm, x, (mPartyIdx + 2) % 3);
        return reveal(comm, x);
    }

    void Sh3Encryptor::reveal(Sh3::CommPkg & comm, const Sh3::sb64 & x, u64 partyIdx)
    {
        if ((mPartyIdx + 2) % 3 == partyIdx)
            comm.mPrev.asyncSendCopy(x[0]);
    }

    void Sh3Encryptor::reveal(Sh3::CommPkg & comm, const Sh3::si64Matrix & x, Sh3::i64Matrix & dest)
    {
        if (dest.rows() != x.rows() || dest.cols() != x.cols())
            throw std::runtime_error(LOCATION);

        comm.mNext.recv(dest.data(), dest.size());
        for (u64 i = 0; i < dest.size(); ++i)
        {
            dest(i) += x.mShares[0](i) + x.mShares[1](i);
        }
    }

    void Sh3Encryptor::revealAll(Sh3::CommPkg & comm, const Sh3::si64Matrix & x, Sh3::i64Matrix & dest)
    {
        reveal(comm, x, (mPartyIdx + 2) % 3);
        reveal(comm, x, dest);
    }

    void Sh3Encryptor::reveal(Sh3::CommPkg & comm, const Sh3::si64Matrix & x, u64 partyIdx)
    {
        if ((mPartyIdx + 2) % 3 == partyIdx)
            comm.mPrev.asyncSendCopy(x.mShares[0].data(), x.mShares[0].size());
    }

    void Sh3Encryptor::reveal(Sh3::CommPkg & comm, const Sh3::sb64Matrix & x, Sh3::i64Matrix & dest)
    {
        if (dest.rows() != x.rows() || dest.cols() != x.cols())
            throw std::runtime_error(LOCATION);

        comm.mNext.recv(dest.data(), dest.size());
        for (u64 i = 0; i < dest.size(); ++i)
        {
            dest(i) ^= x.mShares[0](i) ^ x.mShares[1](i);
        }
    }

    void Sh3Encryptor::revealAll(Sh3::CommPkg & comm, const Sh3::sb64Matrix & x, Sh3::i64Matrix & dest)
    {
        reveal(comm, x, (mPartyIdx + 2) % 3);
        reveal(comm, x, dest);
    }

    void Sh3Encryptor::reveal(Sh3::CommPkg & comm, const Sh3::sb64Matrix & x, u64 partyIdx)
    {
        if ((mPartyIdx + 2) % 3 == partyIdx)
            comm.mPrev.asyncSendCopy(x.mShares[0].data(), x.mShares[0].size());
    }

    void Sh3Encryptor::reveal(Sh3::CommPkg & comm, const Sh3::sPackedBin & A, Sh3::i64Matrix & r)
    {
        auto wordWidth = (A.bitCount() + 8 * sizeof(i64) - 1) / (8 * sizeof(i64));
        Sh3::i64Matrix buff;
        buff.resize(A.bitCount(), A.simdWidth());
        r.resize(A.mShareCount, wordWidth);

        comm.mNext.recv(buff.data(), buff.size());


        for (u64 i = 0; i < buff.size(); ++i)
        {
            buff(i) = buff(i) ^ A.mShares[0](i) ^ A.mShares[1](i);
        }

        oc::MatrixView<u8> bb((u8*)buff.data(), A.bitCount(), A.simdWidth() * sizeof(i64));
        oc::MatrixView<u8> rr((u8*)r.data(), r.rows(), r.cols() * sizeof(i64));
        sse_transpose(bb, rr);
    }

    void Sh3Encryptor::revealAll(Sh3::CommPkg & comm, const Sh3::sPackedBin & A, Sh3::i64Matrix & r)
    {
        reveal(comm, A, (mPartyIdx + 2) % 3);
        reveal(comm, A, r);
    }
    void Sh3Encryptor::reveal(Sh3::CommPkg & comm, const Sh3::sPackedBin & A, u64 partyIdx)
    {
        if ((mPartyIdx + 2) % 3 == partyIdx)
            comm.mPrev.asyncSendCopy(A.mShares[0].data(), A.mShares[0].size());
    }

    void Sh3Encryptor::rand(Sh3::CommPkg & comm, Sh3::si64Matrix & dest)
    {
        for (u64 i = 0; i < dest.size(); ++i)
        {
            auto s = mShareGen.getRandIntShare();
            dest.mShares[0](i) = s[0];
            dest.mShares[1](i) = s[1];
        }
    }

    void Sh3Encryptor::rand(Sh3::CommPkg & comm, Sh3::sb64Matrix & dest)
    {
        for (u64 i = 0; i < dest.size(); ++i)
        {
            auto s = mShareGen.getRandBinaryShare();
            dest.mShares[0](i) = s[0];
            dest.mShares[1](i) = s[1];
        }
    }

    void Sh3Encryptor::rand(Sh3::CommPkg & comm, Sh3::sPackedBin & dest)
    {
        for (u64 i = 0; i < dest.mShares[0].size(); ++i)
        {
            auto s = mShareGen.getRandBinaryShare();
            dest.mShares[0](i) = s[0];
            dest.mShares[1](i) = s[1];
        }
    }
}


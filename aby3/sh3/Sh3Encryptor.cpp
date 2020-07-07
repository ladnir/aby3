#include "Sh3Encryptor.h"
#include <libOTe/Tools/Tools.h>

#include "cryptoTools/Common/Log.h"
namespace aby3
{

    //Sh3Task Sh3Encryptor::init(Sh3Runtime& rt)
    //{
    //    auto task = rt.nullTask();
    //    task.then
    //}

    void Sh3Encryptor::complateSharing(CommPkg& comm, span<i64> send, span<i64> recv)
    {
        comm.mNext.asyncSendCopy(send);
        comm.mPrev.recv(recv);
    }


    si64 Sh3Encryptor::localInt(CommPkg & comm, i64 val)
    {
        si64 ret;
        ret[0] = mShareGen.getShare() + val;


        comm.mNext.asyncSendCopy(ret[0]);
        comm.mPrev.recv(ret[1]);

        return ret;
    }

    si64 Sh3Encryptor::remoteInt(CommPkg & comm)
    {
        return localInt(comm, 0);
    }

    Sh3Task Sh3Encryptor::localInt(Sh3Task dep, i64 val, si64 & dest)
    {
        return dep.then([this, val, &dest](CommPkg& comm, Sh3Task& self) {

            //si64 ret;
            dest[0] = mShareGen.getShare() + val;

            comm.mNext.asyncSendCopy(dest[0]);
            auto fu = comm.mPrev.asyncRecv(dest[1]);

            self.then([fu = std::move(fu)](CommPkg& comm, Sh3Task& self) mutable {
                fu.get();
            });
        }).getClosure();
    }

    Sh3Task Sh3Encryptor::remoteInt(Sh3Task dep, si64 & dest)
    {
        return localInt(dep, 0, dest);
    }

    sb64 Sh3Encryptor::localBinary(CommPkg & comm, i64 val)
    {
        sb64 ret;
        ret[0] = mShareGen.getBinaryShare() ^ val;

        comm.mNext.asyncSendCopy(ret[0]);
        comm.mPrev.recv(ret[1]);

        return ret;
    }

    sb64 Sh3Encryptor::remoteBinary(CommPkg & comm)
    {
        return localBinary(comm, 0);
    }

    Sh3Task Sh3Encryptor::localBinary(Sh3Task dep, i64 val, sb64 & ret)
    {
        return dep.then([this, val, &ret](CommPkg& comm, Sh3Task& self) {
            ret[0] = mShareGen.getBinaryShare() ^ val;

            comm.mNext.asyncSendCopy(ret[0]);
            auto fu = comm.mPrev.asyncRecv(ret[1]);

            self.then([fu = std::move(fu)](CommPkg& comm, Sh3Task& self) mutable {
                fu.get();
            });

        }).getClosure();
    }

    Sh3Task Sh3Encryptor::remoteBinary(Sh3Task dep, sb64 & dest)
    {
        return localBinary(dep, 0, dest);
    }

    void Sh3Encryptor::localIntMatrix(CommPkg & comm, const i64Matrix & m, si64Matrix & ret)
    {
        if (ret.cols() != static_cast<u64>(m.cols()) ||
            ret.size() != static_cast<u64>(m.size()))
            throw std::runtime_error(LOCATION);
        for (i64 i = 0; i < ret.mShares[0].size(); ++i)
            ret.mShares[0](i) = mShareGen.getShare() + m(i);

        comm.mNext.asyncSendCopy(ret.mShares[0].data(), ret.mShares[0].size());
        comm.mPrev.recv(ret.mShares[1].data(), ret.mShares[1].size());
    }

    Sh3Task Sh3Encryptor::localIntMatrix(Sh3Task dep, const i64Matrix & m, si64Matrix & ret)
    {

        return dep.then([this, &m, &ret](CommPkg& comm, Sh3Task& self) {

            oc::lout << self.mRuntime->mPartyIdx << " localIntMatrix" << std::endl;

            if (ret.cols() != static_cast<u64>(m.cols()) ||
                ret.size() != static_cast<u64>(m.size()))
                throw std::runtime_error(LOCATION);
            for (i64 i = 0; i < ret.mShares[0].size(); ++i)
                ret.mShares[0](i) = mShareGen.getShare() + m(i);

            comm.mNext.asyncSendCopy(ret.mShares[0].data(), ret.mShares[0].size());
            auto fu = comm.mPrev.asyncRecv(ret.mShares[1].data(), ret.mShares[1].size());

            self.then([fu = std::move(fu)](CommPkg& comm, Sh3Task& self)mutable{
                oc::lout << self.mRuntime->mPartyIdx << " localIntMatrix 2" << std::endl;
                fu.get();
            });
        }).getClosure();

    }

    void Sh3Encryptor::remoteIntMatrix(CommPkg & comm, si64Matrix & ret)
    {

        for (i64 i = 0; i < ret.mShares[0].size(); ++i)
            ret.mShares[0](i) = mShareGen.getShare();

        comm.mNext.asyncSendCopy(ret.mShares[0].data(), ret.mShares[0].size());
        comm.mPrev.recv(ret.mShares[1].data(), ret.mShares[1].size());
    }

    Sh3Task Sh3Encryptor::remoteIntMatrix(Sh3Task dep, si64Matrix & ret)
    {
        return dep.then([this, &ret](CommPkg& comm, Sh3Task& self) {
            oc::lout << self.mRuntime->mPartyIdx << " remoteIntMatrix 1" << std::endl;

            for (i64 i = 0; i < ret.mShares[0].size(); ++i)
                ret.mShares[0](i) = mShareGen.getShare();

            comm.mNext.asyncSendCopy(ret.mShares[0].data(), ret.mShares[0].size());
            auto fu = comm.mPrev.asyncRecv(ret.mShares[1].data(), ret.mShares[1].size());

            self.then([fu = std::move(fu)](CommPkg& comm, Sh3Task& self) mutable {
                oc::lout << self.mRuntime->mPartyIdx << " remoteIntMatrix 2" << std::endl;
                fu.get();
            });
        }).getClosure();
    }


    void Sh3Encryptor::localBinMatrix(CommPkg & comm, const i64Matrix & m, sbMatrix & ret)
    {
        auto b0 = ret.i64Cols() != static_cast<u64>(m.cols());
        auto b1 = ret.i64Size() != static_cast<u64>(m.size());
        if (b0 || b1)
            throw std::runtime_error(LOCATION);

        for (u64 i = 0; i < ret.mShares[0].size(); ++i)
            ret.mShares[0](i) = mShareGen.getBinaryShare() ^ m(i);

        comm.mNext.asyncSendCopy(ret.mShares[0].data(), ret.mShares[0].size());
        comm.mPrev.recv(ret.mShares[1].data(), ret.mShares[1].size());
    }

    Sh3Task Sh3Encryptor::localBinMatrix(Sh3Task dep, const i64Matrix & m, sbMatrix & ret)
    {
        return dep.then([this, &m, &ret](CommPkg& comm, Sh3Task self) {

            auto b0 = ret.i64Cols() != static_cast<u64>(m.cols());
            auto b1 = ret.i64Size() != static_cast<u64>(m.size());
            if (b0 || b1)
                throw std::runtime_error(LOCATION);

            for (u64 i = 0; i < ret.mShares[0].size(); ++i)
                ret.mShares[0](i) = mShareGen.getBinaryShare() ^ m(i);

            comm.mNext.asyncSendCopy(ret.mShares[0].data(), ret.mShares[0].size());
            auto fu = comm.mPrev.asyncRecv(ret.mShares[1].data(), ret.mShares[1].size());

            self.then([fu = std::move(fu)](CommPkg&, Sh3Task& self) mutable {
                fu.get();
            });
        }).getClosure();
    }

    void Sh3Encryptor::remoteBinMatrix(CommPkg & comm, sbMatrix & ret)
    {
        for (u64 i = 0; i < ret.mShares[0].size(); ++i)
            ret.mShares[0](i) = mShareGen.getBinaryShare();

        comm.mNext.asyncSendCopy(ret.mShares[0].data(), ret.mShares[0].size());
        comm.mPrev.recv(ret.mShares[1].data(), ret.mShares[1].size());
    }

    Sh3Task Sh3Encryptor::remoteBinMatrix(Sh3Task dep, sbMatrix & ret)
    {
        return dep.then([this, &ret](CommPkg& comm, Sh3Task& self) mutable {

            for (u64 i = 0; i < ret.mShares[0].size(); ++i)
                ret.mShares[0](i) = mShareGen.getBinaryShare();

            comm.mNext.asyncSendCopy(ret.mShares[0].data(), ret.mShares[0].size());
            auto fu = comm.mPrev.asyncRecv(ret.mShares[1].data(), ret.mShares[1].size());

            self.then([fu = std::move(fu)](CommPkg& comm, Sh3Task& self) mutable {
                fu.get();
            });
        }).getClosure();
    }

    void Sh3Encryptor::localPackedBinary(CommPkg & comm, const i64Matrix& m, sPackedBin & dest)
    {
        if (dest.bitCount() != m.cols() * sizeof(i64) * 8)
            throw std::runtime_error(LOCATION);
        if (dest.shareCount() != static_cast<u64>(m.rows()))
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

    Sh3Task Sh3Encryptor::localPackedBinary(Sh3Task dep, const i64Matrix & m, sPackedBin & dest)
    {

        oc::MatrixView<u8> mm((u8*)m.data(), m.rows(), m.cols() * sizeof(i64));
        return localPackedBinary(dep, mm, dest, true);
    }

    Sh3Task Sh3Encryptor::localPackedBinary(Sh3Task dep, oc::MatrixView<u8> m, sPackedBin & dest, bool transpose)
    {
        return dep.then([this, m, &dest, transpose](CommPkg& comm, Sh3Task& self) {

            if (dest.bitCount() != m.cols() * 8)
                throw std::runtime_error(LOCATION);
            if (dest.shareCount() != m.rows())
                throw std::runtime_error(LOCATION);

            auto bits = sizeof(i64) * 8;
            auto outRows = dest.bitCount();
            auto outCols = (dest.shareCount() + bits - 1) / bits;
            oc::MatrixView<u8> out((u8*)dest.mShares[0].data(), outRows, outCols * sizeof(i64));

            if (transpose)
                oc::sse_transpose(m, out);
            else
                memcpy(out.data(), m.data(), m.size());

            for (u64 i = 0; i < dest.mShares[0].size(); ++i)
                dest.mShares[0](i) = dest.mShares[0](i) ^ mShareGen.getBinaryShare();

            comm.mNext.asyncSendCopy(dest.mShares[0].data(), dest.mShares[0].size());
            auto fu = comm.mPrev.asyncRecv(dest.mShares[1].data(), dest.mShares[1].size());

            self.then([fu = std::move(fu)](CommPkg& comm, Sh3Task& self) mutable {
                fu.get();
            });
        }).getClosure();
    }

    void Sh3Encryptor::remotePackedBinary(CommPkg & comm, sPackedBin & dest)
    {
        for (u64 i = 0; i < dest.mShares[0].size(); ++i)
            dest.mShares[0](i) = mShareGen.getBinaryShare();

        comm.mNext.asyncSendCopy(dest.mShares[0].data(), dest.mShares[0].size());
        comm.mPrev.recv(dest.mShares[1].data(), dest.mShares[1].size());
    }

    Sh3Task Sh3Encryptor::remotePackedBinary(Sh3Task dep, sPackedBin & dest)
    {
        return dep.then([this, &dest](CommPkg& comm, Sh3Task& self)
        {

            for (u64 i = 0; i < dest.mShares[0].size(); ++i)
                dest.mShares[0](i) = mShareGen.getBinaryShare();

            comm.mNext.asyncSendCopy(dest.mShares[0].data(), dest.mShares[0].size());
            auto fu = comm.mPrev.asyncRecv(dest.mShares[1].data(), dest.mShares[1].size());

            self.then(std::move([fu = std::move(fu)](CommPkg& comm, Sh3Task& self) mutable {
                fu.get();
            }));
        }).getClosure();
    }

    i64 Sh3Encryptor::reveal(CommPkg & comm, const si64 & x)
    {
        i64 s;
        comm.mNext.recv(s);
        return s + x[0] + x[1];
    }

    i64 Sh3Encryptor::revealAll(CommPkg & comm, const si64 & x)
    {
        reveal(comm, (mPartyIdx + 2) % 3, x);
        return reveal(comm, x);
    }

    void Sh3Encryptor::reveal(CommPkg & comm, u64 partyIdx, const si64 & x)
    {
        auto p = ((mPartyIdx + 2)) % 3;
        if (p == partyIdx)
            comm.mPrev.asyncSendCopy(x[0]);
    }

    Sh3Task Sh3Encryptor::reveal(Sh3Task  dep, const si64 & x, i64 & dest)
    {
        return dep.then([&x, &dest](CommPkg& comm, Sh3Task& self) {
            comm.mNext.recv(dest);
            dest += x[0] + x[1];
        });
    }


    Sh3Task Sh3Encryptor::revealAll(Sh3Task dep, const si64& x, i64& dest)
    {
        reveal(dep, (mPartyIdx + 2) % 3, x);
        return reveal(dep, x, dest);
    }

    Sh3Task Sh3Encryptor::reveal(Sh3Task dep, u64 partyIdx, const si64& x)
    {
        //TODO("decide if we can move the if outside the call to then(...)");
        bool send = ((mPartyIdx + 2) % 3) == partyIdx;
        return dep.then([send, &x](CommPkg& comm, Sh3Task&) {
            if (send)
                comm.mPrev.asyncSendCopy(x[0]);
        });
    }


    Sh3Task Sh3Encryptor::reveal(Sh3Task dep, const sb64& x, i64& dest)
    {
        return dep.then([&x, &dest](CommPkg& comm, Sh3Task& self) {
            comm.mNext.recv(dest);
            dest ^= x[0] ^ x[1];
        });
    }

    Sh3Task Sh3Encryptor::revealAll(Sh3Task dep, const sb64& x, i64& dest)
    {
        reveal(dep, (mPartyIdx + 2) % 3, x);
        return reveal(dep, x, dest);
    }

    Sh3Task Sh3Encryptor::reveal(Sh3Task dep, u64 partyIdx, const sb64& x)
    {
        //TODO("decide if we can move the if outside the call to then(...)");
        bool send = ((mPartyIdx + 2) % 3) == partyIdx;
        return dep.then([send, &x](CommPkg& comm, Sh3Task&) {
            if (send)
                comm.mPrev.asyncSendCopy(x[0]);
        });
    }

    Sh3Task Sh3Encryptor::reveal(Sh3Task dep, const si64Matrix& x, i64Matrix& dest)
    {
        return dep.then([&x, &dest](CommPkg& comm, Sh3Task& self) {
            oc::lout << self.mRuntime->mPartyIdx << " reveal recv" << std::endl;
            dest.resize(x.rows(), x.cols());
            comm.mNext.recv(dest.data(), dest.size());
            dest += x.mShares[0];
            dest += x.mShares[1];
        });
    }

    Sh3Task Sh3Encryptor::revealAll(Sh3Task dep, const si64Matrix& x, i64Matrix& dest)
    {
        reveal(dep, (mPartyIdx + 2) % 3, x);
        return reveal(dep, x, dest);
    }

    Sh3Task Sh3Encryptor::reveal(Sh3Task dep, u64 partyIdx, const si64Matrix& x)
    {
        //TODO("decide if we can move the if outside the call to then(...)");
        bool send = ((mPartyIdx + 2) % 3) == partyIdx;
        return dep.then([send, &x](CommPkg& comm, Sh3Task& self) {

            oc::lout << self.mRuntime->mPartyIdx << " reveal Send" << std::endl;

            if (send)
                comm.mPrev.asyncSendCopy(x.mShares[0].data(), x.mShares[0].size());
        });
    }

    Sh3Task Sh3Encryptor::reveal(Sh3Task dep, const sbMatrix& x, i64Matrix& dest)
    {
        return dep.then([&x, &dest](CommPkg& comm, Sh3Task& self) {
            comm.mNext.recv(dest.data(), dest.size());
            for (i32 i = 0; i < dest.size(); ++i)
            {
                dest(i) ^= x.mShares[0](i);
                dest(i) ^= x.mShares[1](i);
            }
        });
    }
    Sh3Task Sh3Encryptor::revealAll(Sh3Task dep, const sbMatrix& x, i64Matrix& dest)
    {
        reveal(dep, (mPartyIdx + 2) % 3, x);
        return reveal(dep, x, dest);
    }
    Sh3Task Sh3Encryptor::reveal(Sh3Task dep, u64 partyIdx, const sbMatrix& x)
    {
        //TODO("decide if we can move the if outside the call to then(...)");
        bool send = ((mPartyIdx + 2) % 3) == partyIdx;
        return dep.then([send, &x](CommPkg& comm, Sh3Task& self) {
            if (send)
                comm.mPrev.asyncSendCopy(x.mShares[0].data(), x.mShares[0].size());
        });
    }








    i64 Sh3Encryptor::reveal(CommPkg & comm, const sb64 & x)
    {
        i64 s;
        comm.mNext.recv(s);
        return s ^ x[0] ^ x[1];
    }

    i64 Sh3Encryptor::revealAll(CommPkg & comm, const sb64 & x)
    {
        reveal(comm, (mPartyIdx + 2) % 3, x);
        return reveal(comm, x);
    }

    void Sh3Encryptor::reveal(CommPkg & comm, u64 partyIdx, const sb64 & x)
    {
        if ((mPartyIdx + 2) % 3 == partyIdx)
            comm.mPrev.asyncSendCopy(x[0]);
    }

    void Sh3Encryptor::reveal(CommPkg & comm, const si64Matrix & x, i64Matrix & dest)
    {
        if (dest.rows() != static_cast<i64>(x.rows()) || dest.cols() != static_cast<i64>(x.cols()))
            throw std::runtime_error(LOCATION);

        comm.mNext.recv(dest.data(), dest.size());
        for (i64 i = 0; i < dest.size(); ++i)
        {
            dest(i) += x.mShares[0](i) + x.mShares[1](i);
        }
    }

    void Sh3Encryptor::revealAll(CommPkg & comm, const si64Matrix & x, i64Matrix & dest)
    {
        reveal(comm, (mPartyIdx + 2) % 3, x);
        reveal(comm, x, dest);
    }

    void Sh3Encryptor::reveal(CommPkg & comm, u64 partyIdx, const si64Matrix & x)
    {
        if ((mPartyIdx + 2) % 3 == partyIdx)
            comm.mPrev.asyncSendCopy(x.mShares[0].data(), x.mShares[0].size());
    }

    void Sh3Encryptor::reveal(CommPkg & comm, const sbMatrix & x, i64Matrix & dest)
    {
        if (dest.rows() != static_cast<i64>(x.rows()) || dest.cols() != static_cast<i64>(x.i64Cols()))
            throw std::runtime_error(LOCATION);

        comm.mNext.recv(dest.data(), dest.size());
        for (i64 i = 0; i < dest.size(); ++i)
        {
            dest(i) ^= x.mShares[0](i) ^ x.mShares[1](i);
        }
    }

    void Sh3Encryptor::revealAll(CommPkg & comm, const sbMatrix & x, i64Matrix & dest)
    {
        reveal(comm, (mPartyIdx + 2) % 3, x);
        reveal(comm, x, dest);
    }

    void Sh3Encryptor::reveal(CommPkg & comm, u64 partyIdx, const sbMatrix & x)
    {
        if ((mPartyIdx + 2) % 3 == partyIdx)
            comm.mPrev.asyncSendCopy(x.mShares[0].data(), x.mShares[0].size());
    }

    Sh3Task Sh3Encryptor::reveal(Sh3Task dep, const sPackedBin& A, i64Matrix& r)
    {
        return dep.then([&A, &r](CommPkg& comm, Sh3Task&  self)
        {
            auto wordWidth = (A.bitCount() + 8 * sizeof(i64) - 1) / (8 * sizeof(i64));
            i64Matrix buff;
            buff.resize(A.bitCount(), A.simdWidth());
            r.resize(A.mShareCount, wordWidth);

            comm.mNext.recv(buff.data(), buff.size());


            for (i64 i = 0; i < buff.size(); ++i)
            {
                buff(i) ^= A.mShares[0](i);
                buff(i) ^= A.mShares[1](i);
            }

            oc::MatrixView<u8> bb((u8*)buff.data(), A.bitCount(), A.simdWidth() * sizeof(i64));
            oc::MatrixView<u8> rr((u8*)r.data(), r.rows(), r.cols() * sizeof(i64));
            memset(r.data(), 0, r.size() * sizeof(i64));
            sse_transpose(bb, rr);
        });
    }
    Sh3Task Sh3Encryptor::revealAll(Sh3Task dep, const sPackedBin& A, i64Matrix& r)
    {

        reveal(dep, (mPartyIdx + 2) % 3, A);
        return reveal(dep, A, r);
    }

    Sh3Task Sh3Encryptor::reveal(Sh3Task dep, u64 partyIdx, const sPackedBin& A)
    {
        //TODO("decide if we can move the if outside the call to then(...)"); 
        bool send = (mPartyIdx + 2) % 3 == partyIdx;
        return dep.then([send, &A](CommPkg& comm, Sh3Task& self) {
            if (send)
                comm.mPrev.asyncSendCopy(A.mShares[0].data(), A.mShares[0].size());
        });
    }



    void Sh3Encryptor::reveal(CommPkg & comm, const sPackedBin & A, i64Matrix & r)
    {
        auto wordWidth = (A.bitCount() + 8 * sizeof(i64) - 1) / (8 * sizeof(i64));
        i64Matrix buff;
        buff.resize(A.bitCount(), A.simdWidth());
        r.resize(A.mShareCount, wordWidth);

        comm.mNext.recv(buff.data(), buff.size());


        for (i64 i = 0; i < buff.size(); ++i)
        {
            buff(i) = buff(i) ^ A.mShares[0](i) ^ A.mShares[1](i);
        }

        oc::MatrixView<u8> bb((u8*)buff.data(), A.bitCount(), A.simdWidth() * sizeof(i64));
        oc::MatrixView<u8> rr((u8*)r.data(), r.rows(), r.cols() * sizeof(i64));
        memset(r.data(), 0, r.size() * sizeof(i64));
        sse_transpose(bb, rr);
    }

    Sh3Task Sh3Encryptor::revealAll(Sh3Task dep, const sPackedBin& A, PackedBin& r)
    {

        reveal(dep, (mPartyIdx + 2) % 3, A);
        return reveal(dep, A, r);
    }


    Sh3Task Sh3Encryptor::reveal(Sh3Task dep, const sPackedBin & A, PackedBin & r)
    {
        return dep.then([&A, &r](CommPkg& comm, Sh3Task&  self)
        {
            r.resize(A.mShareCount, A.bitCount());

            comm.mNext.recv(r.mData.data(), r.mData.size());


            for (u64 i = 0; i < r.size(); ++i)
            {
                r.mData(i) = r.mData(i) ^ A.mShares[0](i) ^ A.mShares[1](i);
            }
        });
    }

    void Sh3Encryptor::revealAll(CommPkg & comm, const sPackedBin & A, i64Matrix & r)
    {
        reveal(comm, (mPartyIdx + 2) % 3, A);
        reveal(comm, A, r);
    }
    void Sh3Encryptor::reveal(CommPkg & comm, u64 partyIdx, const sPackedBin & A)
    {
        if ((mPartyIdx + 2) % 3 == partyIdx)
            comm.mPrev.asyncSendCopy(A.mShares[0].data(), A.mShares[0].size());
    }

    void Sh3Encryptor::rand(si64Matrix & dest)
    {
        for (u64 i = 0; i < dest.size(); ++i)
        {
            auto s = mShareGen.getRandIntShare();
            dest.mShares[0](i) = s[0];
            dest.mShares[1](i) = s[1];
        }
    }

    void Sh3Encryptor::rand(sbMatrix & dest)
    {
        for (u64 i = 0; i < dest.i64Size(); ++i)
        {
            auto s = mShareGen.getRandBinaryShare();
            dest.mShares[0](i) = s[0];
            dest.mShares[1](i) = s[1];
        }
    }

    void Sh3Encryptor::rand(sPackedBin & dest)
    {
        for (u64 i = 0; i < dest.mShares[0].size(); ++i)
        {
            auto s = mShareGen.getRandBinaryShare();
            dest.mShares[0](i) = s[0];
            dest.mShares[1](i) = s[1];
        }
    }


}


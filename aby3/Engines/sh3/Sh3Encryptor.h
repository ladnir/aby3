#pragma once
#include "Sh3Defines.h"
#include "Sh3ShareGen.h"
#include "Sh3Runtime.h"

namespace aby3
{
    class Sh3Encryptor
    {
    public:

        //Sh3Task init(Sh3Task& dep);
        void init(u64 partyIdx, block prevSeed, block nextSeed, u64 buffSize = 256) { mShareGen.init(prevSeed, nextSeed, buffSize); mPartyIdx = partyIdx; }
        void init(u64 partyIdx, Sh3::CommPkg& comm, block seed, u64 buffSize = 256) { mShareGen.init(comm, seed, buffSize); mPartyIdx = partyIdx; }

        Sh3::si64 localInt(Sh3::CommPkg& comm, i64 val);
        Sh3::si64 remoteInt(Sh3::CommPkg& comm);

        Sh3::sb64 localBinary(Sh3::CommPkg& comm, i64 val);
        Sh3::sb64 remoteBinary(Sh3::CommPkg& comm);


        // generates a integer sharing of the matrix m and places the result in dest
        void localIntMatrix(Sh3::CommPkg& comm, const Sh3::i64Matrix& m, Sh3::si64Matrix& dest);

        // generates a integer sharing of the matrix ibput by the remote party and places the result in dest
        void remoteIntMatrix(Sh3::CommPkg& comm, Sh3::si64Matrix& dest);

        // generates a binary sharing of the matrix m and places the result in dest
        void localBinMatrix(Sh3::CommPkg& comm, const Sh3::i64Matrix& m, Sh3::sb64Matrix& dest);

        // generates a binary sharing of the matrix ibput by the remote party and places the result in dest
        void remoteBinMatrix(Sh3::CommPkg& comm, Sh3::sb64Matrix& dest);

        // generates a sPackedBin from the given matrix.
        void localPackedBinary(Sh3::CommPkg& comm, const Sh3::i64Matrix& m, Sh3::sPackedBin& dest);

        // generates a sPackedBin from the given matrix.
        void remotePackedBinary(Sh3::CommPkg& comm, Sh3::sPackedBin& dest);

        i64 reveal(Sh3::CommPkg& comm, const Sh3::si64& x);
        i64 revealAll(Sh3::CommPkg& comm, const Sh3::si64& x);
        void reveal(Sh3::CommPkg& comm, const Sh3::si64& x, u64 partyIdx);

        i64 reveal(Sh3::CommPkg& comm, const Sh3::sb64& x);
        i64 revealAll(Sh3::CommPkg& comm, const Sh3::sb64& x);
        void reveal(Sh3::CommPkg& comm, const Sh3::sb64& x, u64 partyIdx);

        void reveal(Sh3::CommPkg& comm, const Sh3::si64Matrix& x, Sh3::i64Matrix& dest);
        void revealAll(Sh3::CommPkg& comm, const Sh3::si64Matrix& x, Sh3::i64Matrix& dest);
        void reveal(Sh3::CommPkg& comm, const Sh3::si64Matrix& x, u64 partyIdx);

        void reveal(Sh3::CommPkg& comm, const Sh3::sb64Matrix& x, Sh3::i64Matrix& dest);
        void revealAll(Sh3::CommPkg& comm, const Sh3::sb64Matrix& x, Sh3::i64Matrix& dest);
        void reveal(Sh3::CommPkg& comm, const Sh3::sb64Matrix& x, u64 partyIdx);

        void reveal(Sh3::CommPkg& comm, const Sh3::sPackedBin& x, Sh3::i64Matrix& dest);
        void revealAll(Sh3::CommPkg& comm, const Sh3::sPackedBin& x, Sh3::i64Matrix& dest);
        void reveal(Sh3::CommPkg& comm, const Sh3::sPackedBin& x, u64 partyIdx);


        void rand(Sh3::CommPkg& comm, Sh3::si64Matrix& dest);
        void rand(Sh3::CommPkg& comm, Sh3::sb64Matrix& dest);
        void rand(Sh3::CommPkg& comm, Sh3::sPackedBin& dest);


        u64 mPartyIdx = -1;
        Sh3ShareGen mShareGen;

        void complateSharing(Sh3::CommPkg& comm, span<i64> send, span<i64> recv);
    };

}

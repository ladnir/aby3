#pragma once
// This file and the associated implementation has been placed in the public domain, waiving all copyright. No restrictions are placed on its use. 

#include <cryptoTools/Common/Defines.h>
#include <cryptoTools/Crypto/PRNG.h>
#include <cryptoTools/Crypto/RandomOracle.h>
#include <iostream>

namespace osuCrypto {

#define COMMIT_BUFF_u32_SIZE  5
static_assert(RandomOracle::HashSize == sizeof(u32) * COMMIT_BUFF_u32_SIZE, "buffer need to be the same size as hash size");


class Commit 
    {
    public:

		// Default constructor of a Commitment. The state is undefined.
		Commit() = default;

		// Compute a randomized commitment of input. 
		Commit(const block& in, PRNG& prng)
        {
            block rand = prng.get<block>();
            hash((u8*)(&in), sizeof(block), rand);
        }

		// Compute a randomized commitment of input. 
        Commit(const block& in, block& rand)
        {
             hash((u8*)(&in), sizeof(block), rand);
        }

		// Compute a randomized commitment of input. 
		Commit(const span<u8> in, PRNG& prng)
        {
            block rand = prng.get<block>();
             hash(in.data(), in.size(), rand);
        }
		
		// Compute a randomized commitment of input. 
		Commit(const span<u8> in, block& rand)
        {
             hash(in.data(), in.size(), rand);
        }



		// Compute a non-randomized commitment of input. 
		// Note: insecure if input has low entropy. 
		Commit(const block& input) { hash((u8*)(&input), sizeof(block)); }

		// Compute a non-randomized commitment of input. 
		// Note: insecure if input has low entropy. 
		Commit(const std::array<block, 3>& input)
		{
			hash((u8*)(&input[0]), sizeof(block));
			hash((u8*)(&input[1]), sizeof(block));
			hash((u8*)(&input[2]), sizeof(block));
		}

		// Compute a non-randomized commitment of input. 
		// Note: insecure if input has low entropy. 
		Commit(const span<u8> in)
        {
            hash(in.data(), in.size());
        }


		// Compute a non-randomized commitment of input. 
		// Note: insecure if input has low entropy. 
        Commit(u8* d, u64 s)
        {
            hash(d, s);
        }

		// Utility function to test if two commitments are equal.
        bool operator==(const Commit& rhs) const
        {
            for (u64 i = 0; i < COMMIT_BUFF_u32_SIZE; ++i)
            {
                if (buff[i] != rhs.buff[i])
                    return false;
            }
            return true;
        }

		// Utility function to test if two commitments are not equal.
		bool operator!=(const Commit& rhs) const
        {
            return !(*this == rhs);
        }

		// Returns a pointer to the commitment value.
        u8* data() const
        {
            return (u8*)buff;
        }

		// Returns the size of the commitment in bytes.
		static u64 size()
        {
            return RandomOracle::HashSize;
        }

    private:
		u32 buff[COMMIT_BUFF_u32_SIZE];

        void hash(u8* data, u64 size)
        {
            RandomOracle sha;
            sha.Update(data, size);
            sha.Final((u8*)buff);
        }

         void hash(u8* data, u64 size, block& rand)
         {
              RandomOracle sha;
              sha.Update(data, size);
              sha.Update(rand);
              sha.Final((u8*)buff);
         }

    };

    static_assert(sizeof(Commit) == RandomOracle::HashSize, "needs to be Pod type");


	//std::ostream& operator<<(std::ostream& out, const Commit& comm);
}

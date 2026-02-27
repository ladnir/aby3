#pragma once
// This file and the associated implementation has been placed in the public domain, waiving all copyright. No restrictions are placed on its use.
#include <cryptoTools/Common/Defines.h>

namespace osuCrypto {
    namespace details
    {
        static const int rijndael256_rounds = 14;
    }

    struct Block256 : public std::array<block, 2>
    {
    private:
        using Base = std::array<block, 2>;

    public:
        Block256() = default;
        using Base::Base;
        using Base::operator=;

        Block256(block b0, block b1) : Base({ b0, b1 }) {}
        Block256(const std::uint8_t* data) : Base({ toBlock(data), toBlock(data + 16) }) {}
        Block256(std::uint64_t low_u64) : Base({ toBlock(low_u64), toBlock((std::uint64_t)0) }) {}

        const unsigned char* data() const
        {
            // Unsafe, but I don't see a better way.
            return (const unsigned char*)&(*this)[0];
        }

        unsigned char* data()
        {
            return (unsigned char*)&(*this)[0];
        }
    };


#ifdef OC_ENABLE_AESNI


    class Rijndael256Enc
    {
    public:
        using Block = Block256;
        static const int rounds = details::rijndael256_rounds;
        std::array<Block, rounds + 1> mRoundKey;

        // Default constructor leaves the class in an invalid state
        // until setKey(...) is called.
        Rijndael256Enc() = default;
        Rijndael256Enc(const Rijndael256Enc&) = default;

        // Constructor to initialize the class with the given key
        Rijndael256Enc(const Block& userKey)
        {
            setKey(userKey);
        }

        // Set the key to be used for encryption.
        void setKey(const Block& userKey);

        void encBlock(const Block& plaintext, Block& ciphertext) const
        {
            encBlocksFixed<1>(&plaintext, &ciphertext);
        }

        Block encBlock(const Block& plaintext) const
        {
            Block ciphertext;
            encBlock(plaintext, ciphertext);
            return ciphertext;
        }

        // Instantiated only for {1, 2, 3, 4} blocks.
        template<size_t blocks>
        void encBlocksFixed(const Block* plaintext, Block* ciphertext) const;
        template<size_t blocks>
        void encBlocksFixed(const Block (&plaintext)[blocks], Block (&ciphertext)[blocks]) const
        {
            encBlocksFixed(&plaintext[0], &ciphertext[0]);
        }

        void encBlocks(const Block* plaintexts, size_t blocks, Block* ciphertext) const;

        static Block roundEnc(Block state, const Block& roundKey);
        static Block finalEnc(Block state, const Block& roundKey);
    };

    class Rijndael256Dec
    {
    public:
        using Block = Block256;
        static const int rounds = details::rijndael256_rounds;
        std::array<Block, rounds + 1> mRoundKey;

        Rijndael256Dec() = default;
        Rijndael256Dec(const Rijndael256Dec&) = default;

        Rijndael256Dec(const Rijndael256Enc& enc)
        {
            setKey(enc);
        }

        Rijndael256Dec(const Block& userKey)
        {
            setKey(userKey);
        }

        void setKey(const Block& userKey)
        {
            setKey(Rijndael256Enc(userKey));
        }

        void setKey(const Rijndael256Enc& enc);

        void decBlock(const Block& ciphertext, Block& plaintext) const
        {
            decBlocksFixed<1>(&ciphertext, &plaintext);
        }

        Block decBlock(const Block& ciphertext) const
        {
            Block plaintext;
            decBlock(ciphertext, plaintext);
            return plaintext;
        }

        // Instantiated only for {1, 2, 3, 4} blocks.
        template<size_t blocks>
        void decBlocksFixed(const Block* ciphertext, Block* plaintext) const;
        template<size_t blocks>
        void decBlocksFixed(const Block (&ciphertext)[blocks], Block (&plaintext)[blocks]) const
        {
            decBlocksFixed(*ciphertext[0], &plaintext[0]);
        }

        void decBlocks(const Block* ciphertexts, size_t blocks, Block* plaintext) const;

        static Block roundDec(Block state, const Block& roundKey);
        static Block finalDec(Block state, const Block& roundKey);
    };

    // TODO: encryption of N values under N different keys
#endif
}


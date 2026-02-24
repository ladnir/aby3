#pragma once
// This file and the associated implementation has been placed in the public domain, waiving all copyright. No restrictions are placed on its use.
#include <cryptoTools/Common/Defines.h>
#include <type_traits>
#include <cassert>
#include <utility>

namespace osuCrypto {

    namespace details
    {
        enum AESTypes
        {
            NI,
            Portable
        };

        template<AESTypes type>
        class AES
        {
        public:
            static const u64 rounds = 10;

            // Default constructor leave the class in an invalid state
            // until setKey(...) is called.
            AES() = default;
            AES(const AES&) = default;

            // Constructor to initialize the class with the given key
            AES(const block& userKey);

            // Set the key to be used for encryption.
            void setKey(const block& userKey);

            block getKey() const {
                return mRoundKey[0];
            }


            ///////////////////////////////////////////////
            // ECB ENCRYPTION

            // Use this function (or other *Inline functions) if you want to force inlining.
            template<u64 blocks>
            OC_FORCEINLINE typename std::enable_if<(blocks <= 16)>::type
            ecbEncBlocksInline(const block* plaintext, block* ciphertext) const
            {
                assert((u64)plaintext % 16 == 0 && "plaintext must be aligned.");
                assert((u64)ciphertext % 16 == 0 && "ciphertext must be aligned.");

                for (u64 j = 0; j < blocks; ++j)
                    ciphertext[j] = plaintext[j] ^ mRoundKey[0];
                for (u64 i = 1; i < rounds; ++i)
                    roundEncBlocks<blocks>(ciphertext, ciphertext, mRoundKey[i]);
                finalEncBlocks<blocks>(ciphertext, ciphertext, mRoundKey[rounds]);
            }

            template<u64 blocks>
            OC_FORCEINLINE void
            ecbEncBlocksInline(const block (&plaintext)[blocks], block (&ciphertext)[blocks]) const
            {
                ecbEncBlocksInline<blocks>(&plaintext[0], &ciphertext[0]);
            }

            // Fall back to encryption loop rather than doing way too many blocks at once.
            template<u64 blocks>
            OC_FORCEINLINE typename std::enable_if<(blocks > 16)>::type
            ecbEncBlocksInline(const block* plaintext, block* ciphertext) const
            {
                ecbEncBlocks(plaintext, blocks, ciphertext);
            }

            // Not necessarily inlined version. See specialization below class for more information.
            template<u64 blocks>
            inline void ecbEncBlocks(const block* plaintext, block* ciphertext) const
            {
                ecbEncBlocksInline<blocks>(plaintext, ciphertext);
            }

            // Encrypts the plaintext block and stores the result in ciphertext
            inline void ecbEncBlock(const block& plaintext, block& ciphertext) const
            {
                ecbEncBlocks<1>(&plaintext, &ciphertext);
            }

            // Encrypts the plaintext block and returns the result
            inline block ecbEncBlock(const block& plaintext) const
            {
                block ciphertext;
                ecbEncBlock(plaintext, ciphertext);
                return ciphertext;
            }

            // Encrypts blockLength starting at the plaintext pointer and writes the result
            // to the ciphertext pointer
            inline void ecbEncBlocks(const block* plaintext, u64 blockLength, block* ciphertext) const
            {
                const u64 step = 8;
                u64 idx = 0;

                for (; idx + step <= blockLength; idx += step)
                {
                    ecbEncBlocks<step>(plaintext + idx, ciphertext + idx);
                }

                i32 misalignment = blockLength % step;
                switch (misalignment) {
                    #define SWITCH_CASE(n) \
                    case n: \
                        ecbEncBlocks<n>(plaintext + idx, ciphertext + idx); \
                        break;
                    SWITCH_CASE(1)
                    SWITCH_CASE(2)
                    SWITCH_CASE(3)
                    SWITCH_CASE(4)
                    SWITCH_CASE(5)
                    SWITCH_CASE(6)
                    SWITCH_CASE(7)
                    #undef SWITCH_CASE
                }
            }

            inline void ecbEncBlocks(span<const block> plaintext, span<block> ciphertext) const
            {
                if (plaintext.size() != ciphertext.size())
                    throw RTE_LOC;
                ecbEncBlocks(plaintext.data(), plaintext.size(), ciphertext.data());
            }


            ///////////////////////////////////////////////
            // Counter mode stream

            // Encrypts the vector of blocks {baseIdx, baseIdx + 1, ..., baseIdx + blockLength - 1}
            // and writes the result to ciphertext.
            void ecbEncCounterMode(u64 baseIdx, u64 blockLength, block* ciphertext) const
            {
                ecbEncCounterMode(toBlock(baseIdx), blockLength, ciphertext);
            }
            void ecbEncCounterMode(u64 baseIdx, span<block> ciphertext) const
            {
                ecbEncCounterMode(toBlock(baseIdx), ciphertext.size(), ciphertext.data());
            }
            void ecbEncCounterMode(block baseIdx, span<block> ciphertext) const
            {
                ecbEncCounterMode(baseIdx, ciphertext.size(), ciphertext.data());
            }
            void ecbEncCounterMode(block baseIdx, u64 blockLength, block* ciphertext) const
            {
                ecbEncCounterModeImpl(baseIdx, blockLength, ciphertext[0].data());
            }

            // Use this version (which writes to a u8* pointer) for unaligned output.
            void ecbEncCounterMode(block baseIdx, u64 byteLength, u8* ciphertext) const
            {
                if (byteLength % sizeof(block))
                    throw RTE_LOC;
                ecbEncCounterModeImpl(baseIdx, byteLength / sizeof(block), ciphertext);
            }
            void ecbEncCounterMode(u64 baseIdx, u64 blockLength, u8* ciphertext) const
            {
                ecbEncCounterMode(toBlock(baseIdx), blockLength, ciphertext);
            }


            ///////////////////////////////////////////////
            // Tweakable correlation robust hash function.
	        // https://eprint.iacr.org/2019/074.pdf section 7.4


            // Tweakable correlation robust hash function.
	        // y_i = AES(AES(x_i) ^ tweak_i) + AES(x_i).
            template<u64 blocks, typename TweakFn>
            OC_FORCEINLINE typename std::enable_if<(blocks <= 16)>::type
                TmmoHashBlocks(const block* plaintext, block* ciphertext, TweakFn&& tweakFn) const
            {
                block buff[blocks];
                block pix[blocks];

                // pi = AES(x)
                ecbEncBlocks<blocks>(plaintext, pix);

                // { tweaks_0, ..., baseTweak_{blocks - 1} } 
                generateTweaks<blocks>(std::forward<TweakFn>(tweakFn), buff);

                // AES(x) ^ tweaks
                xorBlocks<blocks>(pix, buff);

                // buff = AES( AES(x) ^ tweaks)
                ecbEncBlocks<blocks>(buff, buff);

                // ciphertext = AES(AES(x) ^ tweaks) ^ AES(x)
                xorBlocks<blocks>(buff, pix, ciphertext);
            }


            // Tweakable correlation robust hash function.
            // TMMO(x, i) = AES(AES(x) + i) + AES(x).
            block TmmoHashBlock(block plaintext, block baseTweak) const
            {
                block r;
                TmmoHashBlocks<1>(&plaintext, &r, [baseTweak]() { return baseTweak; });
                return r;
            }


            // Tweakable correlation robust hash function.
            // y_i = AES(AES(x_i) ^ tweak_i) + AES(x_i).
            template<typename TweakFn>
            inline void TmmoHashBlocks(span<const block> plaintext, span<block> ciphertext, TweakFn&& tweak) const
            {
                if (plaintext.size() != ciphertext.size())
                    throw RTE_LOC;

                TmmoHashBlocks(plaintext.data(), plaintext.size(), ciphertext.data(), tweak);
            }


            // Tweakable correlation robust hash function.
            // y_i = AES(AES(x_i) ^ tweak_i) + AES(x_i).
            template<typename TweakFn>
            inline void TmmoHashBlocks(const block* plaintext, u64 blockLength, block* ciphertext, TweakFn&& tweak) const
            {
                const u64 step = 8;
                u64 idx = 0;

                for (; idx + step <= blockLength; idx += step)
                {
                    TmmoHashBlocks<step>(plaintext + idx, ciphertext + idx, tweak);
                }

                i32 misalignment = blockLength % step;
                switch (misalignment) {
#define SWITCH_CASE(n) \
                    case n: \
                        TmmoHashBlocks<n>(plaintext + idx, ciphertext + idx, tweak); \
                        break;
                    SWITCH_CASE(1)
                    SWITCH_CASE(2)
                    SWITCH_CASE(3)
                    SWITCH_CASE(4)
                    SWITCH_CASE(5)
                    SWITCH_CASE(6)
                    SWITCH_CASE(7)
#undef SWITCH_CASE
                }
            }




            ///////////////////////////////////////////////
            // Correlation robust hash function.


            // Correlation robust hash function.
            // H(x) = AES(x) + x.
            template<u64 blocks>
            OC_FORCEINLINE typename std::enable_if<(blocks <= 16)>::type
                hashBlocks(const block* plaintext, block* ciphertext) const
            {
                block buff[blocks];
                ecbEncBlocks<blocks>(plaintext, buff);
                xorBlocks<blocks>(plaintext, buff, ciphertext);
            }

            // Correlation robust hash function.
            // H(x) = AES(x) + x.
            // Fall back to encryption loop rather than unrolling way too many blocks.
            template<u64 blocks>
            OC_FORCEINLINE typename std::enable_if<(blocks > 16)>::type
                hashBlocks(const block* plaintext, block* ciphertext) const
            {
                hashBlocks(plaintext, blocks, ciphertext);
            }


            // Correlation robust hash function.
            // H(x) = AES(x) + x.
            inline void hashBlocks(span<const block> plaintext, span<block> ciphertext) const
            {
                if (plaintext.size() != ciphertext.size())
                    throw RTE_LOC;
                hashBlocks(plaintext.data(), plaintext.size(), ciphertext.data());
            }


            // Correlation robust hash function.
            // H(x) = AES(x) + x.
            inline block hashBlock(const block& plaintext) const
            {
                block ciphertext;
                hashBlocks<1>(&plaintext, &ciphertext);
                return ciphertext;
            }


            // Correlation robust hash function.
            // H(x) = AES(x) + x.
            inline void hashBlocks(const block* plaintext, u64 blockLength, block* ciphertext) const
            {
                const u64 step = 8;
                u64 idx = 0;

                for (; idx + step <= blockLength; idx += step)
                {
                    hashBlocks<step>(plaintext + idx, ciphertext + idx);
                }

                i32 misalignment = blockLength % step;
                switch (misalignment) {
                    #define SWITCH_CASE(n) \
                    case n: \
                        hashBlocks<n>(plaintext + idx, ciphertext + idx); \
                        break;
                    SWITCH_CASE(1)
                    SWITCH_CASE(2)
                    SWITCH_CASE(3)
                    SWITCH_CASE(4)
                    SWITCH_CASE(5)
                    SWITCH_CASE(6)
                    SWITCH_CASE(7)
                    #undef SWITCH_CASE
                }
            }


            // The expanded key.
            std::array<block, rounds + 1> mRoundKey;

            ////////////////////////////////////////
            // Low level

            static block roundEnc(block state, const block& roundKey);
            static block finalEnc(block state, const block& roundKey);

        private:

            template<u64 blocks>
            static OC_FORCEINLINE typename std::enable_if<(blocks > 0)>::type
            roundEncBlocks(const block* stateIn, block* stateOut, const block& roundKey)
            {
                // Force unrolling using template recursion.
                roundEncBlocks<blocks - 1>(stateIn, stateOut, roundKey);
                stateOut[blocks - 1] = roundEnc(stateIn[blocks - 1], roundKey);
            }

            template<u64 blocks>
            static OC_FORCEINLINE typename std::enable_if<(blocks > 0)>::type
            finalEncBlocks(const block* stateIn, block* stateOut, const block& roundKey)
            {
                finalEncBlocks<blocks - 1>(stateIn, stateOut, roundKey);
                stateOut[blocks - 1] = finalEnc(stateIn[blocks - 1], roundKey);
            }

            // Base case
            template<u64 blocks>
            static OC_FORCEINLINE typename std::enable_if<blocks == 0>::type
            roundEncBlocks(const block* stateIn, block* stateOut, const block& roundKey) {}

            template<u64 blocks>
            static OC_FORCEINLINE typename std::enable_if<blocks == 0>::type
            finalEncBlocks(const block* stateIn, block* stateOut, const block& roundKey) {}




            // For simplicity, all CTR modes are defined in terms of the unaligned version.
            void ecbEncCounterModeImpl(block baseIdx, u64 blockLength, u8* ciphertext) const;

            // Use template for loop unrolling.
            template<u64 blocks>
            static OC_FORCEINLINE typename std::enable_if<(blocks > 0)>::type
                xorBlocks(const block* plaintext, block* buff, block* ciphertext)
            {
                buff[blocks - 1] ^= plaintext[blocks - 1];
                xorBlocks<blocks - 1>(plaintext, buff, ciphertext);

                // Only write to ciphertext after computing the entire output, so the compiler won't
                // have to worry about ciphertext aliasing plaintext.
                ciphertext[blocks - 1] = buff[blocks - 1];
            }

            template<u64 blocks>
            static OC_FORCEINLINE typename std::enable_if<(blocks == 0)>::type
                xorBlocks(const block* plaintext, const block* buff, block* ciphertext) {}


            template<u64 blocks, typename TweakFn>
            static OC_FORCEINLINE typename std::enable_if<(blocks > 0)>::type
                generateTweaks(TweakFn&& tweakFn, block* tweaks)
            {
                generateTweaks<blocks - 1>(std::forward<TweakFn>(tweakFn), tweaks);
                tweaks[blocks - 1] = tweakFn();
            }


            template<u64 blocks, typename TweakFn>
            static OC_FORCEINLINE typename std::enable_if<(blocks == 0)>::type
                generateTweaks(TweakFn&& baseTweak, block* tweaks) {}


            template<u64 blocks>
            static OC_FORCEINLINE typename std::enable_if<(blocks > 0)>::type
                xorBlocks(const block* x, block* y)
            {
                auto t = x[blocks - 1] ^ y[blocks - 1];
                xorBlocks<blocks - 1>(x, y);
                y[blocks - 1] = t;
            }


            template<u64 blocks>
            static OC_FORCEINLINE typename std::enable_if<(blocks == 0)>::type
                xorBlocks(const block* baseTweak, block* tweaks) {}
        };

#ifdef OC_ENABLE_AESNI
        template<>
        void AES<NI>::setKey(const block& userKey);

        template<>
        inline block AES<NI>::finalEnc(block state, const block& roundKey)
        {
            return _mm_aesenclast_si128(state, roundKey);
        }

        template<>
        inline block AES<NI>::roundEnc(block state, const block& roundKey)
        {
            return _mm_aesenc_si128(state, roundKey);
        }


#endif

        // A class to perform AES decryption.
        template<AESTypes type>
        class AESDec
        {
        public:
            static const u64 rounds = AES<type>::rounds;

            AESDec() = default;
            AESDec(const AESDec&) = default;

            AESDec(const block& key)
            {
                setKey(key);
            }

            void setKey(const block& userKey);
            void ecbDecBlock(const block& ciphertext, block& plaintext);

            block ecbDecBlock(const block& ciphertext)
            {
                block ret;
                ecbDecBlock(ciphertext, ret);
                return ret;
            }

            std::array<block,rounds + 1> mRoundKey;


            static block roundDec(block state, const block& roundKey);
            static block finalDec(block state, const block& roundKey);

        };
        //void InvCipher(block& state, std::array<block, 11>& RoundKey);


    }

#ifdef OC_ENABLE_AESNI
    using AES = details::AES<details::NI>;
    using AESDec = details::AESDec<details::NI>;
#else
    using AES = details::AES<details::Portable>;
    using AESDec = details::AESDec<details::Portable>;
#endif


    // Specialization of the AES class to support encryption of N values under N different keys
    template<int N>
    class MultiKeyAES
    {
    public:
        std::array<AES, N> mAESs;

        // Default constructor leave the class in an invalid state
        // until setKey(...) is called.
        MultiKeyAES() = default;
        MultiKeyAES(const MultiKeyAES&) = default;

        // Constructor to initialize the class with the given key
        MultiKeyAES(span<block> keys) { setKeys(keys); }

#ifdef OC_ENABLE_AESNI

        // TODO: use technique from "Fast Garbling of Circuits Under Standard Assumptions".
        template<int SS>
        void keyGenHelper8(std::array<block,8>& key)
        {
            std::array<block, 8> keyRcon, t, p;
            keyRcon[0] = _mm_aeskeygenassist_si128(key[0], SS);
            keyRcon[1] = _mm_aeskeygenassist_si128(key[1], SS);
            keyRcon[2] = _mm_aeskeygenassist_si128(key[2], SS);
            keyRcon[3] = _mm_aeskeygenassist_si128(key[3], SS);
            keyRcon[4] = _mm_aeskeygenassist_si128(key[4], SS);
            keyRcon[5] = _mm_aeskeygenassist_si128(key[5], SS);
            keyRcon[6] = _mm_aeskeygenassist_si128(key[6], SS);
            keyRcon[7] = _mm_aeskeygenassist_si128(key[7], SS);

            keyRcon[0] = _mm_shuffle_epi32(keyRcon[0], _MM_SHUFFLE(3, 3, 3, 3));
            keyRcon[1] = _mm_shuffle_epi32(keyRcon[1], _MM_SHUFFLE(3, 3, 3, 3));
            keyRcon[2] = _mm_shuffle_epi32(keyRcon[2], _MM_SHUFFLE(3, 3, 3, 3));
            keyRcon[3] = _mm_shuffle_epi32(keyRcon[3], _MM_SHUFFLE(3, 3, 3, 3));
            keyRcon[4] = _mm_shuffle_epi32(keyRcon[4], _MM_SHUFFLE(3, 3, 3, 3));
            keyRcon[5] = _mm_shuffle_epi32(keyRcon[5], _MM_SHUFFLE(3, 3, 3, 3));
            keyRcon[6] = _mm_shuffle_epi32(keyRcon[6], _MM_SHUFFLE(3, 3, 3, 3));
            keyRcon[7] = _mm_shuffle_epi32(keyRcon[7], _MM_SHUFFLE(3, 3, 3, 3));

            p[0] = key[0];
            p[1] = key[1];
            p[2] = key[2];
            p[3] = key[3];
            p[4] = key[4];
            p[5] = key[5];
            p[6] = key[6];
            p[7] = key[7];

            for (u64 i = 0; i < 3; ++i)
            {
                t[0] = _mm_slli_si128(p[0], 4);
                t[1] = _mm_slli_si128(p[1], 4);
                t[2] = _mm_slli_si128(p[2], 4);
                t[3] = _mm_slli_si128(p[3], 4);
                t[4] = _mm_slli_si128(p[4], 4);
                t[5] = _mm_slli_si128(p[5], 4);
                t[6] = _mm_slli_si128(p[6], 4);
                t[7] = _mm_slli_si128(p[7], 4);

                p[0] = _mm_xor_si128(p[0], t[0]);
                p[1] = _mm_xor_si128(p[1], t[1]);
                p[2] = _mm_xor_si128(p[2], t[2]);
                p[3] = _mm_xor_si128(p[3], t[3]);
                p[4] = _mm_xor_si128(p[4], t[4]);
                p[5] = _mm_xor_si128(p[5], t[5]);
                p[6] = _mm_xor_si128(p[6], t[6]);
                p[7] = _mm_xor_si128(p[7], t[7]);
            }

            key[0] = _mm_xor_si128(p[0], keyRcon[0]);
            key[1] = _mm_xor_si128(p[1], keyRcon[1]);
            key[2] = _mm_xor_si128(p[2], keyRcon[2]);
            key[3] = _mm_xor_si128(p[3], keyRcon[3]);
            key[4] = _mm_xor_si128(p[4], keyRcon[4]);
            key[5] = _mm_xor_si128(p[5], keyRcon[5]);
            key[6] = _mm_xor_si128(p[6], keyRcon[6]);
            key[7] = _mm_xor_si128(p[7], keyRcon[7]);
        };
#endif

        // Set the N keys to be used for encryption.
        void setKeys(span<block> keys)
        {
#ifdef OC_ENABLE_AESNI
            constexpr u64 main = N / 8 * 8;

            auto cp = [&](u8 i, AES* aes, std::array<block, 8>& buff)
            {
                aes[0].mRoundKey[i] = buff[0];
                aes[1].mRoundKey[i] = buff[1];
                aes[2].mRoundKey[i] = buff[2];
                aes[3].mRoundKey[i] = buff[3];
                aes[4].mRoundKey[i] = buff[4];
                aes[5].mRoundKey[i] = buff[5];
                aes[6].mRoundKey[i] = buff[6];
                aes[7].mRoundKey[i] = buff[7];
            };
            std::array<block, 8> buff;

            for (u64 i = 0; i < main; i += 8)
            {
                auto* aes = &mAESs[i];

                buff[0] = keys[i + 0];
                buff[1] = keys[i + 1];
                buff[2] = keys[i + 2];
                buff[3] = keys[i + 3];
                buff[4] = keys[i + 4];
                buff[5] = keys[i + 5];
                buff[6] = keys[i + 6];
                buff[7] = keys[i + 7];
                cp(0, aes, buff);

                keyGenHelper8<0x01>(buff);
                cp(1, aes, buff);
                keyGenHelper8<0x02>(buff);
                cp(2, aes, buff);
                keyGenHelper8<0x04>(buff);
                cp(3, aes, buff);
                keyGenHelper8<0x08>(buff);
                cp(4, aes, buff);
                keyGenHelper8<0x10>(buff);
                cp(5, aes, buff);
                keyGenHelper8<0x20>(buff);
                cp(6, aes, buff);
                keyGenHelper8<0x40>(buff);
                cp(7, aes, buff);
                keyGenHelper8<0x80>(buff);
                cp(8, aes, buff);
                keyGenHelper8<0x1B>(buff);
                cp(9, aes, buff);
                keyGenHelper8<0x36>(buff);
                cp(10, aes, buff);
            }
#else
            u64 main = 0;
#endif

            for (u64 i = main; i < N; ++i)
            {
                mAESs[i].setKey(keys[i]);
            }
        }

        // Computes the encrpytion of N blocks pointed to by plaintext
        // and stores the result at ciphertext.
        void ecbEncNBlocks(const block* plaintext, block* ciphertext) const
        {
            for (int i = 0; i < N; ++i) ciphertext[i] = plaintext[i] ^ mAESs[i].mRoundKey[0];
            for (int i = 0; i < N; ++i) ciphertext[i] = AES::roundEnc(ciphertext[i], mAESs[i].mRoundKey[1]);
            for (int i = 0; i < N; ++i) ciphertext[i] = AES::roundEnc(ciphertext[i], mAESs[i].mRoundKey[2]);
            for (int i = 0; i < N; ++i) ciphertext[i] = AES::roundEnc(ciphertext[i], mAESs[i].mRoundKey[3]);
            for (int i = 0; i < N; ++i) ciphertext[i] = AES::roundEnc(ciphertext[i], mAESs[i].mRoundKey[4]);
            for (int i = 0; i < N; ++i) ciphertext[i] = AES::roundEnc(ciphertext[i], mAESs[i].mRoundKey[5]);
            for (int i = 0; i < N; ++i) ciphertext[i] = AES::roundEnc(ciphertext[i], mAESs[i].mRoundKey[6]);
            for (int i = 0; i < N; ++i) ciphertext[i] = AES::roundEnc(ciphertext[i], mAESs[i].mRoundKey[7]);
            for (int i = 0; i < N; ++i) ciphertext[i] = AES::roundEnc(ciphertext[i], mAESs[i].mRoundKey[8]);
            for (int i = 0; i < N; ++i) ciphertext[i] = AES::roundEnc(ciphertext[i], mAESs[i].mRoundKey[9]);
            for (int i = 0; i < N; ++i) ciphertext[i] = AES::finalEnc(ciphertext[i], mAESs[i].mRoundKey[10]);
        }


        void ecbEncCounterMode(u64 index, block* dst)
        {
            block buff[N];
            for (int i = 0; i < N; ++i)
                buff[i] = block(index);
            ecbEncNBlocks(buff, dst);
        }

        // Computes the hash of N blocks pointed to by plaintext
        // and stores the result at ciphertext.
        void hashNBlocks(const block* plaintext, block* hashes) const
        {
            std::array<block, N> buff;
            for (int i = 0; i < N; ++i)
                buff[i] = plaintext[i];
            //memcpy(buff.data(), plaintext, 16 * N);
            ecbEncNBlocks(buff.data(), buff.data());
            for (int i = 0; i < N; ++i) hashes[i] = buff[i] ^ plaintext[i];
        }


        // Utility to compare the keys.
        const MultiKeyAES<N>& operator=(const MultiKeyAES<N>& rhs)
        {
            for (u64 i = 0; i < N; ++i)
                for (u64 j = 0; j < 11; ++j)
                    mAESs[i].mRoundKey[j] = rhs.mAESs[i].mRoundKey[j];

            return rhs;
        }
    };

	// Pseudorandomly generate a stream of AES round keys.
	struct AESStream
	{
		static constexpr size_t chunkSize = 8;

		AES mPrng;
		MultiKeyAES<chunkSize> mAesRoundKeys;
		u64 mIndex = ~0ull;

		// Uninitialized.
		AESStream() = default;
        AESStream(AESStream && o) noexcept
            : mPrng(std::move(o.mPrng))
            , mAesRoundKeys(std::move(o.mAesRoundKeys))
            , mIndex(std::exchange(o.mIndex, ~0ull))
        {}

        AESStream&operator=(AESStream&& o) noexcept
        {
            mPrng = (std::move(o.mPrng));
            mAesRoundKeys = (std::move(o.mAesRoundKeys));
            mIndex = (std::exchange(o.mIndex, ~0ull));
            return *this;
        }


		AESStream(block seed)
		{
			setSeed(seed);
		}

        void setSeed(block seed);

		const AES& get() const
		{
            assert(mIndex != ~0ull);
			return mAesRoundKeys.mAESs[mIndex % chunkSize];
		}

		void next()
		{
			if (++mIndex % chunkSize == 0)
				refillBuffer();
		}

		void refillBuffer()
		{
			std::array<block, chunkSize> keys;
			mPrng.ecbEncCounterMode(mIndex, keys);
			mAesRoundKeys.setKeys(keys);
		}

        // returns a new AES stream that is derived from this one.
        // Both can be used independently.
        AESStream split()
        {
            return mPrng.ecbEncBlock(block(23142341234234ull, mIndex++));
        }
	};


    // An AES instance with a fixed and public key.
    extern const AES mAesFixedKey;


}

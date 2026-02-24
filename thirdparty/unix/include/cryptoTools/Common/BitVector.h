#pragma once
// This file and the associated implementation has been placed in the public domain, waiving all copyright. No restrictions are placed on its use.
#include <cryptoTools/Common/Defines.h>
#include <cryptoTools/Common/BitIterator.h>
#include <cryptoTools/Crypto/PRNG.h>
#include <cryptoTools/Network/IoBuffer.h>

namespace osuCrypto {

    // A class to access a vector of packed bits. Similar to std::vector<bool>.
    class BitVector
    {
    public:
        typedef u8 value_type;
        typedef value_type* pointer;
        typedef u64 size_type;

        // Default constructor.
        BitVector() = default;

        // Inititialize the BitVector with length bits pointed to by data.
        BitVector(u8* data, u64 length);

        // Inititialize the BitVector from a string of '0' and '1' characters.
        BitVector(std::string data);

        // Construct a zero initialized BitVector of size n.
        explicit BitVector(u64 n) { reset(n); }

        // Copy an existing BitVector.
        BitVector(const BitVector& K) { assign(K); }

        // Move an existing BitVector. Moved from is set to size zero.
        BitVector(BitVector&& rref) { *this = std::move(rref); }

        // Reset the BitVector to have value b.
        void assign(const block& b);

        // Copy an existing BitVector
        void assign(const BitVector& K);

        // Append length bits pointed to by data starting a the bit index by offset.
        void append(u8* data, u64 length, u64 offset = 0);

        // Append an existing BitVector to this BitVector.
        void append(const BitVector& k) { append(k.data(), k.size()); }

        // Append length bits pointed to by data starting a the bit index by offset.
        void append(const BitVector& k, u64 length, u64 offset = 0);

        // erases original contents and set the new size, default 0.
        void reset(size_t new_nbits = 0);

        // Resize the BitVector to have the desired number of bits.
        void resize(u64 newSize);

        // Resize the BitVector to have the desired number of bits.
        // Fill each new bit with val.
        void resize(u64 newSize, u8 val);

        // Reserve enough space for the specified number of bits.
        void reserve(u64 bits);

        // Copy length bits from src starting at offset idx.
        void copy(const BitVector& src, u64 idx, u64 length);

        // Returns the number of bits this BitVector can contain using the current allocation.
        u64 capacity() const { return mAllocBlocks * 8 * sizeof(block); }

        // Returns the number of bits this BitVector current has.
        u64 size() const { return mNumBits; }

        // Return the number of bytes the BitVector currently utilize.
        u64 sizeBytes() const { return divCeil(mNumBits, 8); }

        // Return the number of blocks the BitVector currently utilizes.
        u64 sizeBlocks() const { return divCeil(mNumBits, 8 * sizeof(block)); }

        // Returns a byte pointer to the underlying storage.
        u8* data() const { return (u8*)mData.get(); }

        // Underlying storage, as blocks.
        block* blocks() const { return mData.get(); }

        // Copy and existing BitVector.
        BitVector& operator=(const BitVector& K);

        BitVector& operator=(BitVector&& rref);

        // Get a reference to a specific bit.
        BitReference operator[](const u64 idx) const;

        // Xor two BitVectors together and return the result. Must have the same size.
        BitVector operator^(const BitVector& B)const;

        // AND two BitVectors together and return the result. Must have the same size.
        BitVector operator&(const BitVector& B)const;

        // OR two BitVectors together and return the result. Must have the same size.
        BitVector operator|(const BitVector& B)const;

        // Invert the bits of the BitVector and return the result.
        BitVector operator~()const;

        // Xor the rhs into this BitVector
        void operator^=(const BitVector& A);

        // And the rhs into this BitVector
        void operator&=(const BitVector& A);

        // Or the rhs into this BitVector
        void operator|=(const BitVector& A);

        // Check for equality between two BitVectors
        bool operator==(const BitVector& k)const { return equals(k); }

        // Check for inequality between two BitVectors
        bool operator!=(const BitVector& k)const { return !equals(k); }

        // Check for equality between two BitVectors
        bool equals(const BitVector& K) const;

        // Initialize this BitVector from a string of '0' and '1' characters.
        void fromString(std::string data);

        // Returns an Iterator for the first bit.
        BitIterator begin() const;

        // Returns an Iterator for the position past the last bit.
        BitIterator end() const;

        // Initialize this bit vector to size n with a random set of k bits set to 1.
        void nChoosek(u64 n, u64 k, PRNG& prng);

        // Return the hamming weight of the BitVector.
        u64 hammingWeight() const;

        // Append the bit to the end of the BitVector.
        void pushBack(u8 bit);

        // Returns a refernce to the last bit.
        inline BitReference back() { return (*this)[size() - 1]; }

        // Set all the bits to random values.
        void randomize(PRNG& G);

        // Return the parity of the vector.
        u8 parity();

        // Return the hex representation of the vector.
        std::string hex()const;

        // Reinterpret the vector of bits as a vector of type T.
        template<class T>
        span<T> getArrayView() const;

        // Reinterpret the vector of bits as a vector of type T.
        template<class T>
        span<T> getSpan() const;

    private:
        std::unique_ptr<block[]> mData;
        u64 mNumBits = 0, mAllocBlocks = 0;
    };

    inline BitVector& BitVector::operator=(const BitVector& K)
    {
        if (this != &K) { assign(K); }
        return *this;
    }

    inline BitVector& BitVector::operator=(BitVector&& rref)
    {
        if (this != &rref)
        {
            mData = std::move(rref.mData);
            mNumBits = std::exchange(rref.mNumBits, 0);
            mAllocBlocks = std::exchange(rref.mAllocBlocks, 0);
        }
        return *this;
    }

    inline BitReference BitVector::operator[](const u64 idx) const
    {
        if (idx >= mNumBits) throw std::runtime_error("rt error at " LOCATION);
        return BitReference(data() + (idx / 8), static_cast<u8>(idx % 8));
    }

    inline BitVector BitVector::operator^(const BitVector& B)const
    {
        BitVector ret(*this);

        ret ^= B;

        return ret;
    }

    inline BitVector BitVector::operator&(const BitVector & B) const
    {
        BitVector ret(*this);

        ret &= B;

        return ret;
    }

    inline BitVector BitVector::operator|(const BitVector & B) const
    {
        BitVector ret(*this);

        ret |= B;

        return ret;
    }

    inline BitIterator BitVector::begin() const
    {
        return BitIterator(data(), 0);
    }

    inline BitIterator BitVector::end() const
    {
        return BitIterator(data() + (mNumBits >> 3), mNumBits & 7);
    }

    template<class T>
    inline span<T> BitVector::getArrayView() const
    {
        return span<T>((T*)data(), (T*)data() + (sizeBytes() / sizeof(T)));
    }

    template<class T>
    inline span<T> BitVector::getSpan() const
    {
        return span<T>((T*)data(), (T*)data() + (sizeBytes() / sizeof(T)));
    }

    std::ostream& operator<<(std::ostream& in, const BitVector& val);

#ifdef ENABLE_BOOST
    template<>
    inline u8* channelBuffData<BitVector>(const BitVector& container)
    {
        return (u8*)container.data();
    }

    template<>
    inline BitVector::size_type channelBuffSize<BitVector>(const BitVector& container)
    {
        return container.sizeBytes();
    }

    template<>
    inline bool channelBuffResize<BitVector>(BitVector& container, u64 size)
    {
        return size == container.sizeBytes();
    }
#endif

    inline oc::u64 u8Size(oc::BitVector& bv)
    {
        return bv.sizeBytes();
    }
    inline oc::u64 u8Size(const oc::BitVector& bv)
    {
        return bv.sizeBytes();
    }
}

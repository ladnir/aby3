#pragma once
// This file and the associated implementation has been placed in the public domain, waiving all copyright. No restrictions are placed on its use.
#include <cryptoTools/Common/Defines.h>
#include <cassert>

namespace osuCrypto
{

    // A class to reference a specific bit.
    class BitReference
    {
    public:

        // Default copy constructor
        BitReference(const BitReference& rhs) = default;

        // Construct a reference to the bit in the provided byte offset by the shift.
        BitReference(u8* byte, u64 shift)
            :mByte(byte + (shift / 8)), mShift(shift % 8) {}

        // Construct a reference to the bit in the provided byte offset by the shift and mask.
        // Shift should be less than 8. and the mask should equal 1 << shift.
        BitReference(u8* byte, u8 mask, u8 shift)
            : BitReference(byte, shift) {}

        // Copy the underlying values of the rhs to the lhs.
        void operator=(const BitReference& rhs) { *this = (u8)rhs; }

        // Copy the value of the rhs to the lhs.
        inline void operator=(u8 n) {
            bool b = n;
            *mByte ^= (*mByte ^ ((b & 1) << mShift)) & (1 << mShift);
        }

        inline void operator^=(bool b)
        {
            *mByte ^= ((b & 1) << mShift);
        }

        // Convert the reference to the underlying value
        operator u8() const {
            return (*mByte >> mShift) & 1;
        }

    private:
        u8* mByte;
        u8 mShift;
    };

    // Function to allow the printing of a BitReference.
    inline std::ostream& operator<<(std::ostream& out, const BitReference& bit)
    {
        out << (u32)bit;
        return out;
    }

    // A class to allow the iteration of bits.
    class BitIterator
    {
    public:
        // Would be random access with some extra methods.
        typedef std::bidirectional_iterator_tag iterator_category;
        typedef ptrdiff_t difference_type;
        typedef u8 value_type;
        typedef BitReference reference;
        typedef void pointer; // Can't support operator->

        BitIterator() = default;
        // Default copy constructor
        BitIterator(const BitIterator& cp) = default;

        // Construct a reference to the bit in the provided byte offset by the shift.
        // Shift should be less than 8.
        BitIterator(u8* byte, u64 shift = 0)
            :mByte(byte + (shift / 8)), mShift(shift & 7) {}

        // Construct a reference to the current bit pointed to by the iterator.
        BitReference operator*() { return BitReference(mByte, mShift); }

        // Pre increment the iterator by 1.
        BitIterator& operator++()
        {
            mByte += (mShift == 7) & 1;
            ++mShift &= 7;
            return *this;
        }

        // Post increment the iterator by 1. Returns a copy of this class.
        BitIterator operator++(int)
        {
            BitIterator ret(*this);

            mByte += (mShift == 7) & 1;
            ++mShift &= 7;

            return ret;
        }

        // Pre decrement the iterator by 1.
        BitIterator& operator--()
        {
            mByte -= (mShift == 0) & 1;
            --mShift &= 7;
            return *this;
        }

        // Post decrement the iterator by 1.
        BitIterator operator--(int)
        {
            auto ret = *this;
            --(*this);
            return ret;
        }

        // Return the Iterator that has been incremented by v.
        // v must be possitive.
        BitIterator operator+(i64 v)const
        {
            assert(v >= 0);
            BitIterator ret(mByte, mShift + v);

            return ret;
        }

        // Check if two iterators point to the same bit.
        bool operator==(const BitIterator& cmp) const
        {
            return mByte == cmp.mByte && mShift == cmp.mShift;
        }

        bool operator!=(const BitIterator& cmp) const
        {
            return !(*this == cmp);
        }

        u8* mByte;
        u8 mShift;
    };
}

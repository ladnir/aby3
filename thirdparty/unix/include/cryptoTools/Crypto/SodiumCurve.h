#pragma once

#include <cryptoTools/Common/Defines.h>

#ifdef ENABLE_SODIUM

#include <string.h>
#include <type_traits>
#include <cryptoTools/Crypto/PRNG.h>
#include <cryptoTools/Crypto/Rijndael256.h>
#include <cryptoTools/Crypto/RandomOracle.h>
#include <sodium.h>

namespace osuCrypto
{
namespace Sodium
{

// 255 bit integers, for input to scalar multiplication, and maybe for curve coordinates.
// Arithmetic operations are unimplemented.
struct Scalar25519
{
    Scalar25519() = default;

    Scalar25519(unsigned char* data_)
    {
        memcpy(data, data_, size);
        data[size - 1] &= 0x7f;
    }

    // Generate a random scalar. If clamp, clear the bottom 3 bits and set bit 254 so that curve
    // multiplication will always clear the cofactor and it will always have the same bit length.
    Scalar25519(PRNG& prng, bool clamp_ = true)
    {
        prng.get(data, size);
        data[size - 1] &= 0x7f;

        if (clamp_)
            clamp();
    }

    template<typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    Scalar25519(T val)
    {
        for (size_t i = 0; i < sizeof(T); i++)
            data[i] = val >> 8 * i;

        if (std::is_signed<T>::value)
        {
            // Shift twice to avoid shift overflow.
            unsigned char sign_extend = (val >> 4 * sizeof(T)) >> 4 * sizeof(T);
            memset(data + sizeof(T), sign_extend, size - sizeof(T));
        }
    }

    void clamp()
    {
        data[0] &= 0xf8;
        data[size - 1] |= 0x40;
    }

    size_t sizeBytes() const
    {
        return size;
    }

    void toBytes(unsigned char* dest) const
    {
        memcpy(dest, data, size);
    }

    void fromBytes(const unsigned char* src)
    {
        memcpy(data, src, size);
    }

    void randomize(PRNG& prng)
    {
        *this = Scalar25519(prng);
    }

    bool operator==(const Scalar25519& cmp) const;

    bool operator!=(const Scalar25519& cmp) const
    {
        return !(*this == cmp);
    }

    bool iszero() const;

    static_assert(crypto_core_ed25519_SCALARBYTES == crypto_scalarmult_SCALARBYTES,"");
    static_assert(crypto_core_ed25519_SCALARBYTES == crypto_core_ristretto255_SCALARBYTES,"");
    static const size_t size = crypto_core_ed25519_SCALARBYTES;
    unsigned char data[size];
};

struct Prime25519;

Prime25519 operator-(const Prime25519&);
Prime25519 operator+(const Prime25519&, const Prime25519&);
Prime25519 operator-(const Prime25519&, const Prime25519&);
Prime25519 operator*(const Prime25519&, const Prime25519&);
inline Prime25519 operator/(const Prime25519& a, const Prime25519& b);

// Z/pZ, where p is the order of the largest prime subgroup of 25519
struct Prime25519 : public Scalar25519
{
    Prime25519() = default;
    Prime25519(const Scalar25519&);
    Prime25519(long val) : Scalar25519(val) {}

    // Generate a uniformly random value within the group.
    Prime25519(PRNG& prng) : Prime25519(Scalar25519(prng, false)) {}

    Prime25519& operator++()
    {
        *this += 1;
        return *this;
    }

    Prime25519& operator--()
    {
        *this -= 1;
        return *this;
    }

    Prime25519 operator++(int unused)
    {
        Prime25519 x = *this;
        ++*this;
        return x;
    }

    Prime25519 operator--(int unused)
    {
        Prime25519 x = *this;
        --*this;
        return x;
    }

    Prime25519& operator+=(const Prime25519& b)
    {
        *this = *this + b;
        return *this;
    }

    Prime25519& operator-=(const Prime25519& b)
    {
        *this = *this - b;
        return *this;
    }

    Prime25519& operator*=(const Prime25519& b)
    {
        *this = *this * b;
        return *this;
    }

    Prime25519& operator/=(const Prime25519& b)
    {
        *this = *this / b;
        return *this;
    }

    Prime25519 negate() const
    {
        return -*this;
    }

    Prime25519 inverse() const;

    void fromBytes(const unsigned char* src)
    {
        Scalar25519 x;
        x.fromBytes(src);

        // This reduces x modulo p.
        *this = x;
    }

    void randomize(PRNG& prng)
    {
        *this = Prime25519(prng);
    }
};

inline Prime25519 operator/(const Prime25519& a, const Prime25519& b)
{
    return a * b.inverse();
}

static_assert(std::is_pod<Prime25519>::value, "");

struct Ed25519;

Ed25519 operator*(const Prime25519&, const Ed25519&);

// Edwards curve for 25519.
struct Ed25519
{
    Ed25519() = default;

    Ed25519& operator+=(const Ed25519& b)
    {
        *this = *this + b;
        return *this;
    }

    Ed25519& operator-=(const Ed25519& b)
    {
        *this = *this - b;
        return *this;
    }

    Ed25519& operator*=(const Prime25519& b)
    {
        *this = *this * b;
        return *this;
    }

    Ed25519 operator+(const Ed25519&) const;
    Ed25519 operator-(const Ed25519&) const;

    Ed25519 operator*(const Prime25519& b) const
    {
        return b * *this;
    }

    // Multiply a scalar by the generator of the prime order subgroup.
    static Ed25519 mulGenerator(const Prime25519& n);

    size_t sizeBytes() const
    {
        return size;
    }

    void toBytes(unsigned char* dest) const
    {
        memcpy(dest, data, size);
    }

    void fromBytes(const unsigned char* src)
    {
        memcpy(data, src, size);
    }

    bool operator==(const Ed25519& cmp) const;
    bool operator!=(const Ed25519& cmp) const
    {
        return !(*this == cmp);
    }

    static const size_t size = crypto_core_ed25519_BYTES;
    unsigned char data[size];
};

static_assert(std::is_pod<Ed25519>::value, "");

struct Rist25519;

Rist25519 operator*(const Prime25519&, const Rist25519&);

// Ristretto for curve 25519. Likely fast than Edwards curve because it avoids checking if a point
// is on the prime order subgroup, by instead quotienting by the cofactor.
struct Rist25519
{
    Rist25519() = default;

    Rist25519(PRNG& prng)
    {
        unsigned char h[fromHashLength];
        prng.get(h, sizeof(h));
        *this = fromHash(h);
    }

    Rist25519(const block& seed)
    {
        PRNG prng(seed);
        *this = Rist25519(prng);
    }

    Rist25519& operator+=(const Rist25519& b)
    {
        *this = *this + b;
        return *this;
    }

    Rist25519& operator-=(const Rist25519& b)
    {
        *this = *this - b;
        return *this;
    }

    Rist25519& operator*=(const Prime25519& b)
    {
        *this = *this * b;
        return *this;
    }

    Rist25519 operator+(const Rist25519&) const;
    Rist25519 operator-(const Rist25519&) const;

    Rist25519 operator*(const Prime25519& b) const
    {
        return b * *this;
    }

    // Multiply a scalar by the generator of the whole group, which has prime order.
    static Rist25519 mulGenerator(const Prime25519& n);

    // Generate randomly from a 512 bit hash, using elligator. d must point to fromHashLength
    // uniformly random bytes.
    static Rist25519 fromHash(const unsigned char* d);
    static const size_t fromHashLength = 0x40;

    static Rist25519 fromHash(RandomOracle ro)
    {
        std::array<unsigned char, fromHashLength> h;
        ro.Final(h);
        return fromHash(h.data());
    }

    // Feed data[0..len] into a hash function, then map the hash to the curve.
    void fromHash(const unsigned char* data, size_t len)
    {
        RandomOracle ro(fromHashLength);
        ro.Update(data, len);
        *this = fromHash(ro);
    }

    size_t sizeBytes() const
    {
        return size;
    }

    void toBytes(unsigned char* dest) const
    {
        memcpy(dest, data, size);
    }

    void fromBytes(const unsigned char* src)
    {
        memcpy(data, src, size);
    }

    void randomize(PRNG& prng)
    {
        *this = Rist25519(prng);
    }

    void randomize(const block& seed)
    {
        *this = Rist25519(seed);
    }

    bool operator==(const Rist25519& cmp) const;
    bool operator!=(const Rist25519& cmp) const
    {
        return !(*this == cmp);
    }

    static const size_t size = crypto_core_ristretto255_BYTES;
    unsigned char data[size];
};

static_assert(std::is_pod<Rist25519>::value, "");

#ifdef SODIUM_MONTGOMERY

struct Monty25519;

Monty25519 operator*(const Scalar25519&, const Monty25519&);

// Montgomery curve for 25519. It only stores the x coordinate, so point addition is impossible,
// only multiplication and (internally) differential addition. Represents the curve and its twist
// equally well.
struct Monty25519
{
    Monty25519() = default;

    // Given a uniformly random scalar, generate a uniformly random point that is either on the
    // curve or its twist.
    Monty25519(const Scalar25519& x)
    {
        static_assert(size == Scalar25519::size,"");
        memcpy(data, x.data, size);
        data[size - 1] &= 0x7f;
    }

    Monty25519(PRNG& prng) : Monty25519(Scalar25519(prng, false)) {}
    Monty25519(const block& seed)
    {
        PRNG prng(seed);
        *this = Monty25519(prng);
    }

    Monty25519& operator*=(const Scalar25519& b)
    {
        *this = *this * b;
        return *this;
    }

    Monty25519 operator*(const Scalar25519& b) const
    {
        return b * *this;
    }

    // Multiply a scalar by the generator of the prime order subgroup.
    static Monty25519 mulGenerator(const Scalar25519& n);

    static Monty25519 fromHash(RandomOracle ro)
    {
        Scalar25519 x;
        ro.Final(x);
        return Monty25519(x);
    }

    size_t sizeBytes() const
    {
        return size;
    }

    void toBytes(unsigned char* dest) const
    {
        memcpy(dest, data, size);
    }

    void fromBytes(const unsigned char* src)
    {
        memcpy(data, src, size);
    }

    void randomize(PRNG& prng)
    {
        *this = Monty25519(prng);
    }

    void randomize(const block& seed)
    {
        *this = Monty25519(seed);
    }

    bool operator==(const Monty25519& cmp) const;
    bool operator!=(const Monty25519& cmp) const
    {
        return !(*this == cmp);
    }

    static const size_t size = crypto_scalarmult_BYTES;
    unsigned char data[size];

    static const Monty25519 primeSubgroupGenerator;
    static const Monty25519 primeTwistSubgroupGenerator;
    static const Monty25519 wholeGroupGenerator;
    static const Monty25519 wholeTwistGroupGenerator;
};

static_assert(std::is_pod<Monty25519>::value, "");

#endif

}
}
#endif

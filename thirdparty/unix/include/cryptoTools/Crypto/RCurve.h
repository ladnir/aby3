#pragma once

#include <cryptoTools/Common/Defines.h>

#ifdef ENABLE_RELIC

#include <string.h>
extern "C" {
    #include <relic/relic_bn.h>
    #include <relic/relic_ep.h>
}
#ifdef MONTY
#undef MONTY
#endif
#include <cryptoTools/Crypto/PRNG.h>
#include <cryptoTools/Crypto/RandomOracle.h>
#include "Hashable.h"

#ifndef RLC_FP_BYTES
#define RLC_FP_BYTES FP_BYTES
#endif

namespace osuCrypto
{


    class REllipticCurve;
    class REccPoint;
    class EccBrick;


    class REccNumber
    {
    public:

        REccNumber();
        REccNumber(const REccNumber& num);

        REccNumber(REccNumber&& moveFrom)
        {
            memcpy(&mVal, &moveFrom.mVal, sizeof(bn_t));
            bn_null(moveFrom.mVal);
        }

        REccNumber(PRNG& prng);
        REccNumber(const i32& val);

        // backwards compatible constructors
        REccNumber(REllipticCurve&);
        REccNumber(REllipticCurve&, const REccNumber& num);
        REccNumber(REllipticCurve&, PRNG& prng);
        REccNumber(REllipticCurve&, const i32& val);

        ~REccNumber();

        REccNumber& operator=(const REccNumber& c);
        REccNumber& operator=(REccNumber&& moveFrom)
        {
            std::swap(mVal, moveFrom.mVal);
            return *this;
        }

        REccNumber& operator=(const bn_t c);
        REccNumber& operator=(int i);


        REccNumber& operator++();
        REccNumber& operator--();
        REccNumber& operator+=(int i);
        REccNumber& operator-=(int i);
        REccNumber& operator+=(const REccNumber& b);
        REccNumber& operator-=(const REccNumber& b);
        REccNumber& operator*=(const REccNumber& b);
        REccNumber& operator*=(int i);
        REccNumber& operator/=(const REccNumber& b);
        REccNumber& operator/=(int i);
        //void inplaceNegate();
        REccNumber negate() const;
        REccNumber inverse() const;

        bool operator==(const REccNumber& cmp) const;
        bool operator==(const int& cmp)const;
        friend bool operator==(const int& cmp1, const REccNumber& cmp2);
        bool operator!=(const REccNumber& cmp)const;
        bool operator!=(const int& cmp)const;
        friend bool operator!=(const int& cmp1, const REccNumber& cmp2);

        bool operator>=(const REccNumber& cmp)const;
        bool operator>=(const int& cmp)const;

        bool operator<=(const REccNumber& cmp)const;
        bool operator<=(const int& cmp)const;

        bool operator>(const REccNumber& cmp)const;
        bool operator>(const int& cmp)const;

        bool operator<(const REccNumber& cmp)const;
        bool operator<(const int& cmp)const;

        bool isPrime() const;
        bool iszero() const;

        //const REccNumber& modulus() const;

        friend REccNumber operator-(const REccNumber&);
        friend REccNumber operator+(const REccNumber&, int);
        friend REccNumber operator+(int, const REccNumber&);
        friend REccNumber operator+(const REccNumber&, const REccNumber&);

        friend REccNumber operator-(const REccNumber&, int);
        friend REccNumber operator-(int, const REccNumber&);
        friend REccNumber operator-(const REccNumber&, const REccNumber&);

        friend REccNumber operator*(const REccNumber&, int);
        friend REccNumber operator*(int, const REccNumber&);
        friend REccNumber operator*(const REccNumber&, const REccNumber&);

        friend REccNumber operator/(const REccNumber&, int);
        friend REccNumber operator/(int, const REccNumber&);
        friend REccNumber operator/(const REccNumber&, const REccNumber&);

        friend REccNumber operator^(const REccNumber& base, const REccNumber& exp);

        u64 sizeDigits() const;
        u64 sizeBytes() const;
        void toBytes(u8* dest) const;
        void fromBytes(const u8* src);
        void fromHex(const char* src);
        //void fromDec(const char* src);

        void randomize(PRNG& prng);
        void randomize(const block& seed);

        operator bn_t& () { return mVal; }
        operator const bn_t& () const { return mVal; }

    private:

        void init();
        void reduce();

        const bn_st* modulus()const;

    public:
        bn_t  mVal;

        friend class REllipticCurve;
        friend REccPoint;
        friend std::ostream& operator<<(std::ostream& out, const REccNumber& val);
    };
    std::ostream& operator<<(std::ostream& out, const REccNumber& val);


    class REccPoint
    {
    public:

        REccPoint() { ep_new(mVal); };
        REccPoint(PRNG& prng) { ep_new(mVal); randomize(prng); }
        REccPoint(const REccPoint& copy) { ep_new(mVal); ep_copy(*this, copy); }

        REccPoint(REccPoint&& moveFrom)
        {
            memcpy(&mVal, &moveFrom.mVal, sizeof(ep_t));
            ep_null(moveFrom.mVal);
        }

        // backwards compatible constructors
        REccPoint(REllipticCurve&) { ep_new(mVal); };
        REccPoint(REllipticCurve&, const REccPoint& copy) { ep_new(mVal); ep_copy(*this, copy);}

        ~REccPoint() { ep_free(mVal); }

        REccPoint& operator=(const REccPoint& copy);
        REccPoint& operator=(REccPoint&& moveFrom)
        {
            std::swap(mVal, moveFrom.mVal);
            return *this;
        }

        REccPoint& operator+=(const REccPoint& addIn);
        REccPoint& operator-=(const REccPoint& subtractIn);
        REccPoint& operator*=(const REccNumber& multIn);


        REccPoint operator+(const REccPoint& addIn) const;
        REccPoint operator-(const REccPoint& subtractIn) const;
        REccPoint operator*(const REccNumber& multIn) const;

        // Multiply a scalar by the generator of the elliptic curve. Unsure if this is the whole
        // curve or a prime order subgroup, but it should be the same as
        // REllipticCurve::getGenerator() * n.
        static REccPoint mulGenerator(const REccNumber& n);

        bool operator==(const REccPoint& cmp) const;
        bool operator!=(const REccPoint& cmp) const;

        // Generate randomly from a 256 bit hash. d must point to fromHashLength uniformly random
        // bytes.
        static REccPoint fromHash(const unsigned char* d)
        {
            REccPoint p;
            p.fromHash(d, fromHashLength);
            return p;
        }

        static REccPoint fromHash(RandomOracle ro)
        {
            std::array<unsigned char, fromHashLength> h;
            ro.Final(h);
            return fromHash(h.data());
        }

        // Feed data[0..len] into a hash function, then map the hash to the curve.
        void fromHash(const unsigned char* data, size_t len);

        static const size_t fromHashLength = 0x20;

        u64 sizeBytes() const { return size; }
        void toBytes(u8* dest) const;
        void fromBytes(u8* src);

        bool iszero() const;
            //void fromHex(char* x, char* y);
        //void fromDec(char* x, char* y);
        //void fromNum(REccNumber& x, REccNumber& y);

        void randomize(PRNG& prng);
        void randomize(const block& seed);
        void randomize();


        operator ep_t& () { return mVal; }
        operator const ep_t& () const { return mVal; }

        static const u64 size = 1 + RLC_FP_BYTES;

        ep_t mVal;
    private:

        friend EccBrick;
        friend REccNumber;
        friend std::ostream& operator<<(std::ostream& out, const REccPoint& val);
    };

    std::ostream& operator<<(std::ostream& out, const REccPoint& val);

    //class EccBrick
    //{
    //public:
    //    EccBrick(const REccPoint& copy);
    //    EccBrick(EccBrick&& copy);

    //    REccPoint operator*(const REccNumber& multIn) const;

    //    void multiply(const REccNumber& multIn, REccPoint& result) const;

    //private:

    //    ebrick2 mBrick2;
    //    ebrick mBrick;
    //    REllipticCurve* mCurve;

    //};

    class REllipticCurve
    {
    public:
        typedef REccPoint Point;



        REllipticCurve(u64 curveID = 0);


        Point getGenerator() const;
        std::vector<Point> getGenerators() const;
        REccNumber getOrder() const;

    private:

        friend Point;
        friend REccNumber;
    };

    template<>
    struct Hashable<REccPoint, void> : std::true_type
    {
        template<typename Hasher>
        static void hash(const REccPoint& p, Hasher& hasher)
        {
            u8 buff[REccPoint::size];
            p.toBytes(buff);
            hasher.Update(buff, REccPoint::size);
        }
    };
}
#endif

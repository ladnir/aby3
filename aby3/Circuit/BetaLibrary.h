#pragma once
#include "BetaCircuit.h"
#include <cryptoTools/Common/Defines.h>
#include <unordered_map>
#include <cryptoTools/Common/BitVector.h>

namespace osuCrypto
{
    class BetaLibrary
    {
    public:
        BetaLibrary();
        ~BetaLibrary();
		enum class Optimized
		{
			Size,
			Depth
		};

        std::unordered_map<std::string, BetaCircuit*> mCirMap;

		BetaCircuit* int_int_add(u64 aSize, u64 bSize, u64 cSize, Optimized op = Optimized::Size);
		BetaCircuit* int_int_add_msb(u64 aSize);

        BetaCircuit* uint_uint_add(u64 aSize, u64 bSize, u64 cSize);
        BetaCircuit* int_intConst_add(u64 aSize, u64 bSize, i64 bVal, u64 cSize);
        BetaCircuit* int_int_subtract(u64 aSize, u64 bSize, u64 cSize);
        BetaCircuit* uint_uint_subtract(u64 aSize, u64 bSize, u64 cSize);

        BetaCircuit* int_intConst_subtract(u64 aSize, u64 bSize, i64 bVal, u64 cSize);
        BetaCircuit* int_int_mult(u64 aSize, u64 bSize, u64 cSize, Optimized op = Optimized::Size);
        BetaCircuit* int_int_div(u64 aSize, u64 bSize, u64 cSize);

        BetaCircuit* int_int_lt(u64 aSize, u64 bSize);
        BetaCircuit* int_int_gteq(u64 aSize, u64 bSize);

        BetaCircuit* uint_uint_lt(u64 aSize, u64 bSize);
        BetaCircuit* uint_uint_gteq(u64 aSize, u64 bSize);

        BetaCircuit* int_int_multiplex(u64 aSize);

        BetaCircuit* int_removeSign(u64 aSize);
        BetaCircuit* int_addSign(u64 aSize);
        BetaCircuit* int_negate(u64 aSize);

        BetaCircuit* int_bitInvert(u64 aSize);
        BetaCircuit* int_int_bitwiseAnd(u64 aSize, u64 bSize, u64 cSize);
		BetaCircuit* int_int_bitwiseOr(u64 aSize, u64 bSize, u64 cSize);
		BetaCircuit* int_int_bitwiseXor(u64 aSize, u64 bSize, u64 cSize);


		BetaCircuit* int_Piecewise_helper(u64 aSize, u64 numThesholds);


		void int_piecewise_build_do(
			BetaCircuit& cd,
			span<BetaBundle> aa,
			BetaBundle & b,
			span<BetaBundle> c);



        void int_int_add_build_so(
            BetaCircuit& cd,
            BetaBundle & a1,
            BetaBundle & a2,
            BetaBundle & sum,
            BetaBundle & temps);

		void int_int_add_build_do(
			BetaCircuit& cd,
			BetaBundle & a1,
			BetaBundle & a2,
			BetaBundle & sum,
			BetaBundle & temps);

		void int_int_add_msb_build_do(
			BetaCircuit& cd,
			BetaBundle & a1,
			BetaBundle & a2,
			BetaBundle & sum,
			BetaBundle & temps);

        void uint_uint_add_build(
            BetaCircuit& cd,
            BetaBundle & a1,
            BetaBundle & a2,
            BetaBundle & sum,
            BetaBundle & temps);

        void int_int_subtract_build(
            BetaCircuit& cd,
            BetaBundle & a1,
            BetaBundle & a2,
            BetaBundle & diff,
            BetaBundle & temps);


        void uint_uint_subtract_build(
            BetaCircuit& cd,
            BetaBundle & a1,
            BetaBundle & a2,
            BetaBundle & diff,
            BetaBundle & temps);

        void int_int_mult_build(
            BetaCircuit& cd,
            BetaBundle & a1,
            BetaBundle & a2,
            BetaBundle & prod,
			Optimized op = Optimized::Size);

        void int_int_div_rem_build(
            BetaCircuit& cd,
            BetaBundle& a1,
            BetaBundle& a2,
            BetaBundle& quot,
            BetaBundle& rem
            //,BetaBundle & divByZero,
            //bool checkDivByZero
        );
        void uint_uint_div_rem_build(
            BetaCircuit& cd,
            BetaBundle& a1,
            BetaBundle& a2,
            BetaBundle& quot,
            BetaBundle& rem
            //,BetaBundle & divByZero,
            //bool checkDivByZero
        );

        void int_int_lt_build(
            BetaCircuit& cd,
            BetaBundle & a1,
            BetaBundle & a2,
            BetaBundle & out);

        void int_int_gteq_build(
            BetaCircuit& cd,
            BetaBundle & a1,
            BetaBundle & a2,
            BetaBundle & out);

        void uint_uint_lt_build(
            BetaCircuit& cd,
            BetaBundle & a1,
            BetaBundle & a2,
            BetaBundle & out);

        void uint_uint_gteq_build(
            BetaCircuit& cd,
            BetaBundle & a1,
            BetaBundle & a2,
            BetaBundle & out);

        void int_removeSign_build(
            BetaCircuit& cd,
            BetaBundle & a1,
            BetaBundle & out,
            BetaBundle & temp);

        void int_addSign_build(
            BetaCircuit& cd,
            BetaBundle & a1,
            BetaBundle & sign,
            BetaBundle & out,
            BetaBundle & temp);

        void int_bitInvert_build(
            BetaCircuit& cd,
            BetaBundle & a1,
            BetaBundle & out);

        void int_negate_build(
            BetaCircuit& cd,
            BetaBundle & a1,
            BetaBundle & out,
            BetaBundle & temp);

        void int_int_bitwiseAnd_build(
            BetaCircuit& cd,
            BetaBundle & a1,
            BetaBundle & a2,
            BetaBundle & out);

		void int_int_bitwiseOr_build(
			BetaCircuit& cd,
			BetaBundle & a1,
			BetaBundle & a2,
			BetaBundle & out);

		void int_int_bitwiseXor_build(
			BetaCircuit& cd,
			BetaBundle & a1,
			BetaBundle & a2,
			BetaBundle & out);

		// if choice = 1, the take the first parameter (ifTrue). Otherwise take the second parameter (ifFalse).
        void int_int_multiplex_build(
            BetaCircuit& cd,
            BetaBundle & ifTrue,
            BetaBundle & ifFalse,
            BetaBundle & choice,
            BetaBundle & out,
            BetaBundle & temp);

        bool areDistint(BetaBundle& a1, BetaBundle& a2);
        //u64 aSize, u64 bSize, u64 cSize);

    };

}
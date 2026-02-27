#pragma once
#include "cryptoTools/Common/CLP.h"


void BetaCircuit_SequentialOp_Test();
void BetaCircuit_int_Adder_Test();
void BetaCircuit_int_Adder_const_Test();
void BetaCircuit_int_Subtractor_Test();
void BetaCircuit_int_Subtractor_const_Test();
void BetaCircuit_int_Multiply_Test();
void BetaCircuit_int_Divide_Test();
void BetaCircuit_int_LessThan_Test();
void BetaCircuit_int_GreaterThanEq_Test();

void BetaCircuit_negate_Test();
void BetaCircuit_bitInvert_Test();
void BetaCircuit_removeSign_Test();
void BetaCircuit_addSign_Test();

void BetaCircuit_uint_Adder_Test();
void BetaCircuit_uint_Subtractor_Test();
void BetaCircuit_uint_Multiply_Test();
void BetaCircuit_uint_LessThan_Test();
void BetaCircuit_uint_GreaterThanEq_Test();

void BetaCircuit_multiplex_Test();

void BetaCircuit_xor_and_lvl_test(const oc::CLP& cmd);

void BetaCircuit_aes_test();
void BetaCircuit_json_Tests();
void BetaCircuit_bin_Tests();

oc::i64 signExtend(oc::i64 v, oc::u64 b);

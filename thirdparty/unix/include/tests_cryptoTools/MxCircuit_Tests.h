#pragma once

#include "cryptoTools/Common/CLP.h"

void MxCircuit_Bit_Ops_Test(const oc::CLP& cmd);
void MxCircuit_BInt_Ops_Test(const oc::CLP& cmd);
void MxCircuit_BUInt_Ops_Test(const oc::CLP& cmd);
void MxCircuit_BDynInt_Ops_Test(const oc::CLP& cmd);
void MxCircuit_BDynUInt_Ops_Test(const oc::CLP& cmd);
void MxCircuit_Cast_Test(const oc::CLP& cmd);
void MxCircuit_asBetaCircuit_Test(const oc::CLP& cmd);
void MxCircuit_parallelPrefix_Test(const oc::CLP& cmd);
void MxCircuit_rippleAdder_Test(const oc::CLP& cmd);
void MxCircuit_parallelSummation_Test(const oc::CLP& cmd);
void MxCircuit_multiply_Test(const oc::CLP& cmd);
void MxCircuit_divideRemainder_Test(const oc::CLP& cmd);

void MxCircuit_Subtractor_Test();
void MxCircuit_Subtractor_const_Test();
void MxCircuit_Multiply_Test();
void MxCircuit_Divide_Test();
void MxCircuit_LessThan_Test();
void MxCircuit_GreaterThanEq_Test();

void MxCircuit_negate_Test();
void MxCircuit_bitInvert_Test();
void MxCircuit_removeSign_Test();
void MxCircuit_addSign_Test();

void MxCircuit_uint_Adder_Test();
void MxCircuit_uint_Subtractor_Test();
void MxCircuit_uint_Multiply_Test();
void MxCircuit_uint_LessThan_Test();
void MxCircuit_uint_GreaterThanEq_Test();

void MxCircuit_multiplex_Test();

void MxCircuit_xor_and_lvl_test(const oc::CLP& cmd);

void MxCircuit_aes_test();
void MxCircuit_json_Tests();
void MxCircuit_bin_Tests();

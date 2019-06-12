#pragma once


#include <cryptoTools/Common/CLP.h>

void Sh3_Evaluator_mul_test();
void Sh3_Evaluator_asyncMul_test();
void Sh3_Evaluator_asyncMul_fixed_test();
void Sh3_Evaluator_truncationPai_test(const oc::CLP&);
void Sh3_Evaluator_asyncMul_matrixFixed_test(const oc::CLP&);


void sh3_asyncArithBinMul_test();
void sh3_asyncPubArithBinMul_test();


void Sh3_f64_basics_test(); 

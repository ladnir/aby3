#include "aby3_tests.h"

oc::TestCollection aby3_tests([](oc::TestCollection& tc) {
    
    tc.add("BetaCircuit_int_Sh3Piecewise_Test       ", BetaCircuit_int_Sh3Piecewise_Test);
    tc.add("garble_Test                             ", garble_Test);
                
    tc.add("Task_schedule_test                      ", Task_schedule_test);
    tc.add("Sh3_Runtime_schedule_test               ", Sh3_Runtime_schedule_test); 
    tc.add("Sh3_Runtime_repeatInput_test            ", Sh3_Runtime_repeatInput_test);
    tc.add("Sh3_trim_test                           ", Sh3_trim_test);
    tc.add("Sh3_Encryptor_IO_test                   ", Sh3_Encryptor_IO_test);
    tc.add("Sh3_Encryptor_asyncIO_test              ", Sh3_Encryptor_asyncIO_test);
    tc.add("Sh3_Evaluator_mul_test                  ", Sh3_Evaluator_mul_test);
    tc.add("Sh3_Evaluator_asyncMul_test             ", Sh3_Evaluator_asyncMul_test);
	tc.add("Sh3_Evaluator_truncationPai_test        ", Sh3_Evaluator_truncationPai_test);
	tc.add("Sh3_Evaluator_asyncMul_fixed_test       ", Sh3_Evaluator_asyncMul_fixed_test);
	tc.add("Sh3_Evaluator_asyncMul_matrixFixed_test ", Sh3_Evaluator_asyncMul_matrixFixed_test);

	tc.add("SharedOT_eval_test                      ", SharedOT_eval_test);
	tc.add("sh3_asyncArithBinMul_test 	            ", sh3_asyncArithBinMul_test);
	tc.add("sh3_asyncPubArithBinMul_test            ", sh3_asyncPubArithBinMul_test);

    tc.add("Sh3_f64_basics_test                     ", Sh3_f64_basics_test);
    tc.add("Sh3_BinaryEngine_and_test               ", Sh3_BinaryEngine_and_test);
    tc.add("Sh3_BinaryEngine_add_test               ", Sh3_BinaryEngine_add_test);
    tc.add("Sh3_BinaryEngine_add_msb_test;          ", Sh3_BinaryEngine_add_msb_test);
    tc.add("Sh3_convert_b64Matrix_PackedBin_test    ", Sh3_convert_b64Matrix_PackedBin_test);

	tc.add("Sh3_Piecewise_plain_test                ", Sh3_Piecewise_plain_test);
	tc.add("Sh3_Piecewise_test                      ", Sh3_Piecewise_test);
});
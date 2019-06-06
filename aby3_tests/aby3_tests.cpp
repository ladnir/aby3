#include "aby3_tests.h"

oc::TestCollection aby3_tests([](oc::TestCollection& tc) {
    
    tc.add("BetaCircuit_int_Sh3Piecewise_Test       ", BetaCircuit_int_Sh3Piecewise_Test);
                                                 
    //tc.add("Lynx_matrixOperations_tests          ", Lynx_matrixOperations_tests);
    //tc.add("Lynx_BinaryEngine_and_test           ", Lynx_BinaryEngine_and_test);
    //tc.add("Lynx_BinaryEngine_add_test           ", Lynx_BinaryEngine_add_test);
    //tc.add("Lynx_BinaryEngine_add_msb_test       ", Lynx_BinaryEngine_add_msb_test);
    //tc.add("Lynx_cir_Sh3Piecewise_Helper_test       ", Lynx_cir_Sh3Piecewise_Helper_test);
    //tc.add("Lynx_Sh3Piecewise_plain_test            ", Lynx_Sh3Piecewise_plain_test);
    //tc.add("Lynx_Sh3Piecewise_test                  ", Lynx_Sh3Piecewise_test);
    //tc.add("Lynx_preproc_test                    ", Lynx_preproc_test);
    //tc.add("Lynx_argmax_test                     ", Lynx_argmax_test);
    //tc.add("Lynx_asyncArithBinMul_test           ", Lynx_asyncArithBinMul_test);
    //tc.add("Lynx_asyncPubArithBinMul_test        ", Lynx_asyncPubArithBinMul_test);
    //tc.add("Lynx_asyncInputBinary_test           ", Lynx_asyncInputBinary_test);
    //tc.add("Lynx_ConvertToPackedBinary_test      ", Lynx_asyncConvertToPackedBinary_test);
                         
	tc.add("Sh3_Runtime_schedule_test               ", Sh3_Runtime_schedule_test);
    tc.add("Sh3_trim_test                           ", Sh3_trim_test);
    tc.add("Sh3_Encryptor_IO_test                   ", Sh3_Encryptor_IO_test);
    tc.add("Sh3_Encryptor_asyncIO_test              ", Sh3_Encryptor_asyncIO_test);
    tc.add("Sh3_Evaluator_mul_test                  ", Sh3_Evaluator_mul_test);
    tc.add("Sh3_Evaluator_asyncMul_test             ", Sh3_Evaluator_asyncMul_test);
	tc.add("Sh3_Evaluator_truncationPai_test        ", Sh3_Evaluator_truncationPai_test);
	tc.add("Sh3_Evaluator_asyncMul_fixed_test       ", Sh3_Evaluator_asyncMul_fixed_test);
	tc.add("Sh3_Evaluator_asyncMul_matrixFixed_test ", Sh3_Evaluator_asyncMul_matrixFixed_test);
    tc.add("Sh3_f64_basics_test                     ", Sh3_f64_basics_test);
    tc.add("Sh3_BinaryEngine_and_test               ", Sh3_BinaryEngine_and_test);
    tc.add("Sh3_BinaryEngine_add_test               ", Sh3_BinaryEngine_add_test);
    tc.add("Sh3_BinaryEngine_add_msb_test;          ", Sh3_BinaryEngine_add_msb_test);
    tc.add("Sh3_convert_b64Matrix_PackedBin_test    ", Sh3_convert_b64Matrix_PackedBin_test);
});
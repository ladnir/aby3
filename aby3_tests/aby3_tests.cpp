#include "aby3_tests.h"

oc::TestCollection aby3_tests([](oc::TestCollection& tc) {
    
    tc.add("BetaCircuit_int_Adder_Test           ", BetaCircuit_int_Adder_Test    );
    tc.add("BetaCircuit_int_piecewise_Test       ", BetaCircuit_int_piecewise_Test);
    tc.add("BetaCircuit_json_Tests               ", BetaCircuit_json_Tests);
    tc.add("BetaCircuit_bin_Tests                ", BetaCircuit_bin_Tests);
                                                 
    tc.add("Lynx_matrixOperations_tests          ", Lynx_matrixOperations_tests);
    tc.add("Lynx_BinaryEngine_and_test           ", Lynx_BinaryEngine_and_test);
    tc.add("Lynx_BinaryEngine_add_test           ", Lynx_BinaryEngine_add_test);
    tc.add("Lynx_BinaryEngine_add_msb_test       ", Lynx_BinaryEngine_add_msb_test);
    tc.add("Lynx_cir_piecewise_Helper_test       ", Lynx_cir_piecewise_Helper_test);
    tc.add("Lynx_Piecewise_plain_test            ", Lynx_Piecewise_plain_test);
    tc.add("Lynx_Piecewise_test                  ", Lynx_Piecewise_test);
    tc.add("Lynx_preproc_test                    ", Lynx_preproc_test);
    tc.add("Lynx_argmax_test                     ", Lynx_argmax_test);
    tc.add("Lynx_asyncArithBinMul_test           ", Lynx_asyncArithBinMul_test);
    tc.add("Lynx_asyncPubArithBinMul_test        ", Lynx_asyncPubArithBinMul_test);
    tc.add("Lynx_asyncInputBinary_test           ", Lynx_asyncInputBinary_test);
    tc.add("Lynx_ConvertToPackedBinary_test      ", Lynx_asyncConvertToPackedBinary_test);
                             
    tc.add("Sh3_trim_test                        ", Sh3_trim_test);
    tc.add("Sh3_Encryptor_IO_test                ", Sh3_Encryptor_IO_test);
    tc.add("Sh3_Encryptor_asyncIO_test           ", Sh3_Encryptor_asyncIO_test);
    tc.add("Sh3_Evaluator_mul_test               ", Sh3_Evaluator_mul_test);
    tc.add("Sh3_Evaluator_asyncMul_test          ", Sh3_Evaluator_asyncMul_test);
    tc.add("Sh3_BinaryEngine_and_test            ", Sh3_BinaryEngine_and_test);
    tc.add("Sh3_BinaryEngine_add_test            ", Sh3_BinaryEngine_add_test);
    tc.add("Sh3_BinaryEngine_add_msb_test;       ", Sh3_BinaryEngine_add_msb_test);
    tc.add("Sh3_convert_b64Matrix_PackedBin_test ", Sh3_convert_b64Matrix_PackedBin_test);
});
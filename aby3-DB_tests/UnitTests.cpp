#include "UnitTests.h"

void xtabs_test();

oc::TestCollection DB_tests([](oc::TestCollection& tc) {
    tc.add("lowMC_Circuit_test           ", lowMC_Circuit_test);
    tc.add("lowMC_FileCircuit_test       ", lowMC_FileCircuit_test);
    tc.add("lowMC_BinFileCircuit_test    ", lowMC_BinFileCircuit_test);

    tc.add("DB_computeKeys_test          ", DB_computeKeys_test);

    tc.add("Perm3p_overwrite_Test        ", Perm3p_overwrite_Test);
    tc.add("Perm3p_additive_Test         ", Perm3p_additive_Test);
    tc.add("Perm3p_subset_Test           ", Perm3p_subset_Test);
    tc.add("switch_select_test           ", switch_select_test);
    tc.add("switch_duplicate_test        ", switch_duplicate_test);
    tc.add("switch_full_test             ", switch_full_test);
    tc.add("DB_cuckooHash_test           ", DB_cuckooHash_test);
    tc.add("DB_compare_test              ", DB_compare_test);
    tc.add("DB_Intersect_test            ", DB_Intersect_test);
    tc.add("DB_Intersect_sl_test         ", DB_Intersect_sl_test);
    tc.add("DB_Intersect_ls_test         ", DB_Intersect_ls_test);
    tc.add("DB_leftUnion_test            ", DB_leftUnion_test);
    tc.add("xtabs_test                   ", xtabs_test);
});
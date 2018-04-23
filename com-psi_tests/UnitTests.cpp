#include "UnitTests.h"

oc::TestCollection ComPsi_tests([](oc::TestCollection& tc) {
    tc.add("lowMC_Circuit_test           ", lowMC_Circuit_test);
    tc.add("lowMC_FileCircuit_test       ", lowMC_FileCircuit_test);
    tc.add("lowMC_BinFileCircuit_test    ", lowMC_BinFileCircuit_test);

    tc.add("ComPsi_computeKeys_test      ", ComPsi_computeKeys_test);

    tc.add("Perm3p_overwrite_Test        ", Perm3p_overwrite_Test);
    tc.add("Perm3p_additive_Test         ", Perm3p_additive_Test);
    tc.add("Perm3p_subset_Test           ", Perm3p_subset_Test);
    tc.add("switch_select_test           ", switch_select_test);
    tc.add("switch_duplicate_test        ", switch_duplicate_test);
    tc.add("switch_full_test             ", switch_full_test);
    tc.add("ComPsi_cuckooHash_test       ", ComPsi_cuckooHash_test);
});
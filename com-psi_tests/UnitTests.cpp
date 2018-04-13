#include "UnitTests.h"

oc::TestCollection ComPsi_tests([](oc::TestCollection& tc) {
    tc.add("lowMC_Circuit_test           ", lowMC_Circuit_test); 
    tc.add("lowMC_FileCircuit_test       ", lowMC_FileCircuit_test);
    tc.add("lowMC_BinFileCircuit_test    ", lowMC_BinFileCircuit_test);

    tc.add("ComPsi_computeKeys_test      ", ComPsi_computeKeys_test);
}); 
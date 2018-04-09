#include "UnitTests.h"

oc::TestCollection ComPsi_tests([](oc::TestCollection& tc) {
    tc.add("lowMC_Circuit_test           ", lowMC_Circuit_test);
});
#include "tests.h"
#include "FullDecisionTree.h"

namespace aby3
{


    oc::TestCollection test_DT([](oc::TestCollection& tc) {
        tc.add("FullTree_innerProd_test      ", tests::FullTree_innerProd_test);
        });

}
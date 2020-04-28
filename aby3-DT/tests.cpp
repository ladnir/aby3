#include "tests.h"
#include "FullDecisionTree.h"
#include "SparseDecisionTree.h"

namespace aby3
{


    oc::TestCollection test_DT([](oc::TestCollection& tc) {

        tc.add("Sparse_shuffle_test          ", tests::Sparse_shuffle_test);
        tc.add("Sparse_NodeNames_test        ", tests::Sparse_NodeNames_test);
        tc.add("Sparse_traversTree_test      ", tests::Sparse_traversTree_test);

        return;
        tc.add("FullTree_innerProd_test      ", tests::FullTree_innerProd_test);
        tc.add("FullTree_compare_test        ", tests::FullTree_compare_test);
        tc.add("FullTree_reduce_test         ", tests::FullTree_reduce_test);
        tc.add("FullTree_vote_test           ", tests::FullTree_vote_test);
        tc.add("FullTree_endToEnd_test       ", tests::FullTree_endToEnd_test);
        });

}
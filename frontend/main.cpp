
#include <cryptoTools/Common/CLP.h>

#include "aby3_tests/aby3_tests.h"
#include "com-psi_tests/UnitTests.h"
#include <tests_cryptoTools/UnitTests.h>

std::vector<std::string> unitTestTag{"u", "unitTest"};


using u64 = oc::u64;

int main(int argc, char** argv)
{
    oc::CLP cmd(argc, argv);

    
    if (cmd.isSet(unitTestTag))
    {
        auto tests = aby3_tests;
        tests += ComPsi_tests;
        //tests += tests_cryptoTools::Tests;

        if (cmd.isSet("list"))
        {
            tests.list();
        }
        else
        {
            cmd.setDefault("loop", 1);
            auto loop = cmd.get<u64>("loop");

            if (cmd.hasValue(unitTestTag))
                tests.run(cmd.getMany<u64>(unitTestTag), loop);
            else
                tests.runAll(loop);
        }
    }
}
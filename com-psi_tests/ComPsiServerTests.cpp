
#include "com-psi/ComPsiServer.h"
#include <cryptoTools/Network/IOService.h>

using namespace oc;

void ComPsi_computeKeys_test()
{

    IOService ios;
    Session s01(ios, "127.0.0.1", SessionMode::Server, "01");
    Session s10(ios, "127.0.0.1", SessionMode::Client, "01");
    Session s02(ios, "127.0.0.1", SessionMode::Server, "02");
    Session s20(ios, "127.0.0.1", SessionMode::Client, "02");
    Session s12(ios, "127.0.0.1", SessionMode::Server, "12");
    Session s21(ios, "127.0.0.1", SessionMode::Client, "12");


    ComPsiServer srvs[3];
    srvs[0].init(0, s02, s01);
    srvs[1].init(1, s10, s12);
    srvs[2].init(2, s21, s20);

    auto size = 1 << 10;
    Table a;
    a.mKeys.resize(size, (srvs[0].mKeyBitCount+63) / 64);

    for (u64 i = 0; i < size; ++i)
    {
        for (u64 j = 0; j < a.mKeys.cols(); ++j)
        {
            a.mKeys(i, j) = i >> 1;
        }
    }

    auto t0 = std::thread([&]() {
        auto i = 0;
        setThreadName("t_" + ToString(i));
        auto A = srvs[i].localInput(a);

        std::vector<SharedTable*> tables{ &A };
        std::vector<u64> reveals{ 0 };
        auto r = srvs[i].computeKeys(tables, reveals);


        for (u64 i = 0; i < size - 1; i += 2)
        {
            for (u64 j = 0; j < a.mKeys.cols(); ++j)
            {
                //std::cout << (j ? ", " : ToString(i) + " : ") << r(i, j);
                if (r(i, j) != r(i + 1, j))
                {
                    throw std::runtime_error(LOCATION);
                }
            }
            //std::cout << std::endl;
        }

    });


    auto routine = [&](int i)
    {
        setThreadName("t_" + ToString(i));

        auto A = srvs[i].remoteInput(0, size);
        std::vector<SharedTable*> tables{ &A };
        std::vector<u64> reveals{ 0 };
        auto r = srvs[i].computeKeys(tables, reveals);
    };

    auto t1 = std::thread(routine, 1);
    auto t2 = std::thread(routine, 2);

    t0.join();
    t1.join();
    t2.join();
}


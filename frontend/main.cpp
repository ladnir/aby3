
#include <cryptoTools/Common/CLP.h>

#include "aby3_tests/aby3_tests.h"
#include "com-psi_tests/UnitTests.h"
#include <tests_cryptoTools/UnitTests.h>
#include "cryptoTools/Common/BitIterator.h"
#include "cryptoTools/Common/Timer.h"
#include "cryptoTools/Crypto/PRNG.h"
#include <cryptoTools/Network/IOService.h>
#include <atomic>
#include "com-psi/ComPsiServer.h"
#include "eric.h"

using namespace oc;
std::vector<std::string> unitTestTag{"u", "unitTest"};





void ComPsi_Intersect(u32 rows)
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


    // 80 bits;
    u32 hashSize = 80;
    

    PRNG prng(ZeroBlock);
    
    auto keyBitCount = srvs[0].mKeyBitCount;
    Table a(rows, { ColumnInfo{ "key", TypeID::IntID, keyBitCount } })
        , b(rows, { ColumnInfo{ "key", TypeID::IntID, keyBitCount } });
    auto intersectionSize = (rows + 1) / 2;


    for (u64 i = 0; i < rows; ++i)
    {
        auto out = (i >= intersectionSize);
        for (u64 j = 0; j < a.mColumns[0].mData.cols(); ++j)
        {
            a.mColumns[0].mData(i, j) = i + 1;
            b.mColumns[0].mData(i, j) = i + 1 + (rows * out);
        }
    }


    bool failed = false;
    auto t0 = std::thread([&]() {
        setThreadName("t0");
        auto i = 0;
        Timer timer;
        timer.setTimePoint("start");
        auto A = srvs[i].localInput(a);
        auto B = srvs[i].localInput(b);

        timer.setTimePoint("inputs");
        srvs[i].setTimer(timer);

        auto C = srvs[i].intersect(A, B); 
        timer.setTimePoint("intersect");


        if (C.rows())
        {
            aby3::Sh3::i64Matrix c(C.mColumns[0].rows(), C.mColumns[0].i64Cols());
            srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[0], c);
            timer.setTimePoint("reveal");

            if (c.rows() != intersectionSize)
            {
                failed = true;
                std::cout << "bad size, exp: " << intersectionSize << ", act: " << c.rows() << std::endl;
            }
        }
        else
        {
            failed = true;
        }
        std::cout << timer << std::endl << srvs[i].getTimer() << std::endl;
    });

    auto t1 = std::thread([&]() {
        setThreadName("t1");
        auto i = 1;
        auto A = srvs[i].remoteInput(0);
        auto B = srvs[i].remoteInput(0); 

        auto C = srvs[i].intersect(A, B);
        if (C.rows())
        {
            aby3::Sh3::i64Matrix c(C.mColumns[0].rows(), C.mColumns[0].i64Cols());
            srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[0], c);
        }
    });
    auto t2 = std::thread([&]() {
        setThreadName("t2");
        auto i = 2;
        auto A = srvs[i].remoteInput(0);
        auto B = srvs[i].remoteInput(0);

        auto C = srvs[i].intersect(A, B);

        if (C.rows())
        {
            aby3::Sh3::i64Matrix c(C.mColumns[0].rows(), C.mColumns[0].i64Cols());
            srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[0], c);
        }
    });



    t0.join();
    t1.join();
    t2.join();

    if (failed)
    {
        std::cout << "bad intersection" << std::endl;
        throw std::runtime_error("");
    }

    //srvs[0].intersect(A, B);
}




int main(int argc, char** argv)
{
    oc::CLP cmd(argc, argv);

    if (cmd.isSet("h"))
    {
        std::cout << "-u                 ~~ to run all tests" << std::endl;
        std::cout << "-u n1 [n2 ...]     ~~ to run test n1, n2, ..." << std::endl;
        std::cout << "-u -list           ~~ to list all tests" << std::endl;
        std::cout << "-intersect -nn NN  ~~ to run the intersection benchmark with 2^NN set sizes" << std::endl;
    }
    
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
    if (cmd.isSet("eric"))
    {
        auto nn = cmd.getMany<int>("nn");
        if (nn.size() == 0)
            nn.push_back(16);

        for (auto n : nn)
        {
            eric( 1 << n);
        }
    }


    if (cmd.isSet("intersect"))
    {
        auto nn = cmd.getMany<int>("nn");
        if (nn.size() == 0)
            nn.push_back(1 << 16);

        for (auto n : nn)
        {
            auto size = 1 << n;
            ComPsi_Intersect(size);
        }
    }

 }
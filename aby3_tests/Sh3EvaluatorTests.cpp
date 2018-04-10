#include "Sh3EvaluatorTests.h"
#include "aby3/Engines/sh3/Sh3Evaluator.h"
#include "aby3/Engines/sh3/Sh3Encryptor.h"
#include <cryptoTools/Network/Channel.h>
#include <cryptoTools/Network/IOService.h>
#include <cryptoTools/Crypto/PRNG.h>
#include <cryptoTools/Common/BitVector.h>

using namespace aby3;
using namespace oc;
using namespace aby3::Sh3;

void rand(Sh3::i64Matrix& A, PRNG& prng)
{
    prng.get(A.data(), A.size());
}

void Sh3_Evaluator_asyncMul_test()
{

    IOService ios;
    auto chl01 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "01").addChannel();
    auto chl10 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "01").addChannel();
    auto chl02 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "02").addChannel();
    auto chl20 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "02").addChannel();
    auto chl12 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "12").addChannel();
    auto chl21 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "12").addChannel();


    int trials = 10;
    CommPkg comms[3];
    comms[0] = { chl02, chl01 };
    comms[1] = { chl10, chl12 };
    comms[2] = { chl21, chl20 };

    Sh3Encryptor encs[3];
    Sh3Evaluator evals[3];

    encs[0].init(0, toBlock(0, 0), toBlock(0, 1));
    encs[1].init(1, toBlock(0, 1), toBlock(0, 2));
    encs[2].init(2, toBlock(0, 2), toBlock(0, 0));

    evals[0].init(0, toBlock(1, 0), toBlock(1, 1));
    evals[1].init(1, toBlock(1, 1), toBlock(1, 2));
    evals[2].init(2, toBlock(1, 2), toBlock(1, 0));

    bool failed = false;
    auto t0 = std::thread([&]() {
        auto& enc = encs[0];
        auto& eval = evals[0];
        auto& comm = comms[0];
        Sh3Runtime rt;
        rt.init(0, comm);

        PRNG prng(ZeroBlock);

        for (u64 i = 0; i < trials; ++i)
        {
            Sh3::i64Matrix a(trials, trials), b(trials, trials), c(trials, trials), cc(trials, trials);
            rand(a, prng);
            rand(b, prng);
            Sh3::si64Matrix A(trials, trials), B(trials, trials), C(trials, trials);

            enc.localIntMatrix(comm, a, A);
            enc.localIntMatrix(comm, b, B);

            auto task = rt.noDependencies();

            for (u64 j = 0; j < trials; ++j)
            {
                task = eval.asyncMul(task, A, B, C);
                task = task.then([&](Sh3::CommPkg& comm, Sh3Task& self) {
                    A = C + A;
                });
            }

            task.get();

            enc.reveal(comm, C, cc);

            for (u64 j = 0; j < trials; ++j)
            {
                c = a * b;
                a = c + a;
            }
            if (c != cc)
            {
                failed = true;
                std::cout << c << std::endl;
                std::cout << cc << std::endl;
            }
        }
    });


    auto rr = [&](int i)
    {
        auto& enc = encs[i];
        auto& eval = evals[i];
        auto& comm = comms[i];
        Sh3Runtime rt;
        rt.init(i, comm);

        for (u64 i = 0; i < trials; ++i)
        {
            Sh3::si64Matrix A(trials, trials), B(trials, trials), C(trials, trials);
            enc.remoteIntMatrix(comm, A);
            enc.remoteIntMatrix(comm, B);

            auto task = rt.noDependencies();
            for (u64 j = 0; j < trials; ++j)
            {
                task = eval.asyncMul(task, A, B, C);
                task = task.then([&](Sh3::CommPkg& comm, Sh3Task& self) {
                    A = C + A;
                });
            }
            task.get();

            enc.reveal(comm, C, 0);
        }
    };

    auto t1 = std::thread(rr, 1);
    auto t2 = std::thread(rr, 2);

    t0.join();
    t1.join();
    t2.join();

    if (failed)
        throw std::runtime_error(LOCATION);
}


void Sh3_Evaluator_mul_test()
{

    IOService ios;
    auto chl01 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "01").addChannel();
    auto chl10 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "01").addChannel();
    auto chl02 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "02").addChannel();
    auto chl20 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "02").addChannel();
    auto chl12 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "12").addChannel();
    auto chl21 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "12").addChannel();


    int trials = 10;
    CommPkg comms[3];
    comms[0] = { chl02, chl01 };
    comms[1] = { chl10, chl12 };
    comms[2] = { chl21, chl20 };

    Sh3Encryptor encs[3];
    Sh3Evaluator evals[3];

    encs[0].init(0, toBlock(0, 0), toBlock(0, 1));
    encs[1].init(1, toBlock(0, 1), toBlock(0, 2));
    encs[2].init(2, toBlock(0, 2), toBlock(0, 0));

    evals[0].init(0, toBlock(1, 0), toBlock(1, 1));
    evals[1].init(1, toBlock(1, 1), toBlock(1, 2));
    evals[2].init(2, toBlock(1, 2), toBlock(1, 0));

    bool failed = false;
    auto t0 = std::thread([&]() {
        auto& enc = encs[0];
        auto& eval = evals[0];
        auto& comm = comms[0];
        PRNG prng(ZeroBlock);

        for (u64 i = 0; i < trials; ++i)
        {
            Sh3::i64Matrix a(trials, trials), b(trials, trials), c(trials, trials), cc(trials, trials);
            rand(a, prng);
            rand(b, prng);
            Sh3::si64Matrix A(trials, trials), B(trials, trials), C(trials, trials);

            enc.localIntMatrix(comm, a, A);
            enc.localIntMatrix(comm, b, B);

            eval.mul(comm, A, B, C);

            c = a * b;

            enc.reveal(comm, C, cc);

            if (c != cc)
            {
                failed = true;
                std::cout << c << std::endl;
                std::cout << cc << std::endl;
            }
        }
    });


    auto rr = [&](int i)
    {
        auto& enc = encs[i];
        auto& eval = evals[i];
        auto& comm = comms[i];

        for (u64 i = 0; i < trials; ++i)
        {
            Sh3::si64Matrix A(trials, trials), B(trials, trials), C(trials, trials);
            enc.remoteIntMatrix(comm, A);
            enc.remoteIntMatrix(comm, B);

            eval.mul(comm, A, B, C);

            enc.reveal(comm, C, 0);
        }
    };

    auto t1 = std::thread(rr, 1);
    auto t2 = std::thread(rr, 2);

    t0.join();
    t1.join();
    t2.join();

    if (failed)
        throw std::runtime_error(LOCATION);
}

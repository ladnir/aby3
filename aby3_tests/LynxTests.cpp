#include "LynxTests.h"

#include "aby3/Engines/Lynx/LynxPiecewise.h"
#include "aby3/Engines/LynxEngine.h"
#include "aby3/Engines/Lynx/LynxBinaryEngine.h"
#include "cryptoTools/Network/IOService.h"
#include "cryptoTools/Common/Log.h"
#include <random>
//void initSessions(IOService&ios, span<Session> eps, u64 pIdx)
//{
//	auto eIter = eps.begin();
//	for (u64 i = 0; i < pIdx; ++i)
//	{
//		eIter->start(ios, "localhost", SessionMode::Server, std::to_string(i) + "-" + std::to_string(pIdx));
//	}
//}
//
//std::vector<Channel> addChannel(span<Session> eps)
//{
//
//}
using namespace oc;
void Lynx_matrixOperations_tests()
{

    IOService ios;
    Session ep01(ios, "127.0.0.1", SessionMode::Server, "01");
    Session ep10(ios, "127.0.0.1", SessionMode::Client, "01");
    Session ep02(ios, "127.0.0.1", SessionMode::Server, "02");
    Session ep20(ios, "127.0.0.1", SessionMode::Client, "02");
    Session ep12(ios, "127.0.0.1", SessionMode::Server, "12");
    Session ep21(ios, "127.0.0.1", SessionMode::Client, "12");

    //Channel chl01 = ep01.addChannel("c");
    //Channel chl10 = ep10.addChannel("c");
    //Channel chl02 = ep02.addChannel("c");
    //Channel chl20 = ep20.addChannel("c");
    //Channel chl12 = ep12.addChannel("c");
    //Channel chl21 = ep21.addChannel("c");

    u64 dec = 8;
    PRNG prng0(ZeroBlock);
    PRNG prng1(toBlock(1));
    PRNG prng2(toBlock(2));


    u64 xSize(8), ySize(8);
    Lynx::Engine::value_type_matrix
        a(xSize, ySize),
        aa(xSize, ySize),
        b(xSize, ySize),
        bb(xSize, ySize),
        c(xSize, ySize),
        d(xSize, ySize),
        dd(xSize, ySize);



    auto program = [&](Lynx::Engine& p)
    {
        setThreadName("party " + std::to_string(p.mPartyIdx));

        p.sync();
        p.sync();

        //ostreamLock(std::cout) << "party " << p.mPartyIdx << " enter" << std::endl;

        Lynx::Engine::Matrix
            A(a.rows(), a.cols()),
            B(b.rows(), b.cols()),
            C(c.rows(), c.cols()),
            D(d.rows(), d.cols());

        Lynx::Engine::Share s;



        switch (p.mPartyIdx)
        {
        case 0:

            for (u64 i = 0; i < a.size(); ++i)
                a(i) = Lynx::Engine::value_type(prng0.get<i64>() % 100) / (1 << dec);

            s = p.localInput(23);
            p.localInput(a, A);
            p.remoteInput(1, B);
            p.remoteInput(2, C);

            break;
        case 1:

            for (u64 i = 0; i < a.size(); ++i)
                b(i) = Lynx::Engine::value_type(prng1.get<i64>() % 100) / (1 << dec);

            s = p.remoteInput(0);
            p.remoteInput(0, A);
            p.localInput(b, B);
            p.remoteInput(2, C);

            break;
        case 2:

            for (u64 i = 0; i < a.size(); ++i)
                c(i) = Lynx::Engine::value_type(prng2.get<i64>() % 100) / (1 << dec);

            s = p.remoteInput(0);

            p.remoteInput(0, A);
            p.remoteInput(1, B);
            p.localInput(c, C);

            break;
        }

        auto ss = p.reveal(s);
        //ostreamLock(std::cout) << "party " << p.mPartyIdx << " s=" << ss << std::endl;


        if (p.mPartyIdx == 0) aa = p.reveal(A);
        else  p.reveal(A);



        D = p.mul(A, B) + C;
        //D = A + B;

        if (p.mPartyIdx == 0) dd = p.reveal(D);
        else  p.reveal(D);
    };

    auto trials = 100;

    for (u64 i = 0; i < trials; ++i)
    {


        Lynx::Engine p0, p1, p2;
        auto t0 = std::thread([&]() {p0.init(0, ep02, ep01, dec, toBlock(34265)); program(p0); });
        auto t1 = std::thread([&]() {p1.init(1, ep10, ep12, dec, toBlock(42534)); program(p1); });
        auto t2 = std::thread([&]() {p2.init(2, ep21, ep20, dec, toBlock(35123)); program(p2); });

        t1.join();
        t2.join();
        t0.join();

        d = a * b + c;

        if (aa != a)
        {
            std::cout << "a exp\n" << a << std::endl;
            std::cout << "a act\n" << aa << std::endl;
            throw std::runtime_error(LOCATION);
        }

        auto error = dd - d;
        auto m = std::abs(error(0));
        for (u64 i = 1; i < error.size(); ++i)
        {
            if (std::abs(error(i)) > m)
                m = std::abs(error(i));
        }


        if (m > 2.0 / (1 << dec))
        {
            std::cout << "d exp\n" << d << std::endl;
            std::cout << "d act\n" << dd << std::endl;

            std::cout << "error: \n" << error << "\n\n" << m << " > " << 2.0 / (1 << dec) << std::endl;
            throw std::runtime_error(LOCATION);
        }
    }
}


void createBinarySharing(
    PRNG& prng,
    Eigen::Matrix<i64, Eigen::Dynamic, Eigen::Dynamic>& value,
    Lynx::Engine::Matrix& s0,
    Lynx::Engine::Matrix& s1,
    Lynx::Engine::Matrix& s2)
{
    s0.mShares[0] = value;

    s1.mShares[0].resizeLike(value);
    s2.mShares[0].resizeLike(value);

    //if (zeroshare) {
    //	s1.mShares[0].setZero();
    //	s2.mShares[0].setZero();
    //}
    //else 
    {
        prng.get(s1.mShares[0].data(), s1.mShares[0].size());
        prng.get(s2.mShares[0].data(), s2.mShares[0].size());

        for (u64 i = 0; i < s0.mShares[0].size(); ++i)
            s0.mShares[0](i) ^= s1.mShares[0](i) ^ s2.mShares[0](i);
    }

    s0.mShares[1] = s2.mShares[0];
    s1.mShares[1] = s0.mShares[0];
    s2.mShares[1] = s1.mShares[0];
}


void createSharing(
    PRNG& prng,
    Lynx::Engine::word_matrix& value,
    Lynx::Engine::Matrix& s0,
    Lynx::Engine::Matrix& s1,
    Lynx::Engine::Matrix& s2)
{
    s0.mShares[0] = value;

    s1.mShares[0].resizeLike(value);
    s2.mShares[0].resizeLike(value);

    //if (zeroshare) {
    //	s1.mShares[0].setZero();
    //	s2.mShares[0].setZero();
    //}
    //else 
    {
        prng.get(s1.mShares[0].data(), s1.mShares[0].size());
        prng.get(s2.mShares[0].data(), s2.mShares[0].size());

        for (u64 i = 0; i < s0.mShares[0].size(); ++i)
            s0.mShares[0](i) -= s1.mShares[0](i) + s2.mShares[0](i);
    }

    s0.mShares[1] = s2.mShares[0];
    s1.mShares[1] = s0.mShares[0];
    s2.mShares[1] = s1.mShares[0];
}

void createSharing(
    PRNG& prng,
    Lynx::Engine::value_type_matrix& values,
    Lynx::Engine::Matrix& s0,
    Lynx::Engine::Matrix& s1,
    Lynx::Engine::Matrix& s2,
    u64 dec)
{
    Lynx::Engine::word_matrix v(values.rows(), values.cols());

    for (u64 i = 0; i < values.size(); ++i)
    {
        v(i) = values(i) * (1ull << dec);
    }
    createSharing(prng, v, s0, s1, s2);
}


i64 eval(BetaCircuit* cir, i64 a, u64 b)
{
    std::vector<BitVector> in(2), out(1);

    in[0].append((u8*)&a, sizeof(i64) * 8);
    in[1].append((u8*)&b, sizeof(i64) * 8);


    out[0].resize(cir->mOutputs[0].mWires.size());


    cir->evaluate(in, out);

    if (out[0].size() == 1)
    {
        return out[0][0];
    }
    else if (out[0].size() == 64)
    {
        return *(i64*)out[0].data();
    }
    else
        throw std::runtime_error(LOCATION);
}


i64 Lynx_BinaryEngine_test(BetaCircuit* cir, std::function<i64(i64, i64)> binOp)
{

    IOService ios;
    Session ep01(ios, "127.0.0.1", SessionMode::Server, "01");
    Session ep10(ios, "127.0.0.1", SessionMode::Client, "01");
    Session ep02(ios, "127.0.0.1", SessionMode::Server, "02");
    Session ep20(ios, "127.0.0.1", SessionMode::Client, "02");
    Session ep12(ios, "127.0.0.1", SessionMode::Server, "12");
    Session ep21(ios, "127.0.0.1", SessionMode::Client, "12");

    Channel chl01 = ep01.addChannel("c");
    Channel chl10 = ep10.addChannel("c");
    Channel chl02 = ep02.addChannel("c");
    Channel chl20 = ep20.addChannel("c");
    Channel chl12 = ep12.addChannel("c");
    Channel chl21 = ep21.addChannel("c");

    std::array<Lynx::BinaryEngine, 3> eng{
        Lynx::BinaryEngine(0, chl02, chl01),
        Lynx::BinaryEngine(1, chl10, chl12),
        Lynx::BinaryEngine(2, chl21, chl20)
    };


    BetaLibrary lib;
    u64 size = 64, width = 100;
    cir->levelByAndDepth();

    typedef  Eigen::Matrix<i64, Eigen::Dynamic, Eigen::Dynamic> valMatrix;

    std::array<valMatrix, 2> input;
    valMatrix expOut, actOut;
    std::array<std::array<Lynx::Engine::Matrix, 2>, 3> share;

    std::array<std::thread, 3> thrds;

    PRNG prng(ZeroBlock);

    u64 max = 100;
    input[0].resize(width, 1);
    input[1].resize(width, 1);
    expOut.resize(width, 1);
    actOut.resize(width, 1);
    for (u64 i = 0; i < width; ++i)
    {
        input[0](i) = prng.get<u64>() % max;
        input[1](i) = prng.get<u64>() % max;


        expOut(i) = binOp(input[0](i), input[1](i));
        actOut(i) = 0;

        //std::cout << i << " " << BitVector((u8*)&input[0](i), 64) << " " << BitVector((u8*)&input[1](i), 64) << std::endl;

        //std::cout << expOut(i) << " = " << input[0](i) << " + " << input[1](i) << std::endl;


        auto o = eval(cir, input[0](i), input[1](i));

        //std::cout << o << std::endl;
        auto exp = expOut(i);
        if (o != exp)
        {
            throw std::runtime_error(LOCATION);
        }
    }

    createBinarySharing(prng, input[0], share[0][0], share[1][0], share[2][0]);
    createBinarySharing(prng, input[1], share[0][1], share[1][1], share[2][1]);

    for (u64 i = 0; i < 3; ++i)
    {
        eng[i].setCir(cir, width);
        eng[i].setInput(0, share[i][0]);
        eng[i].setInput(1, share[i][1]);
        thrds[i] = std::thread([&, i]() {
            eng[i].evaluate();
        });
    }

    for (u64 i = 0; i < 3; ++i)
        thrds[i].join();

    std::array<Lynx::Engine::Matrix, 3> outShare;

    for (u64 i = 0; i < 3; ++i)
    {
        outShare[i].resize(width, 1);
        eng[i].getOutput(0, outShare[i]);

        for (u64 j = 0; j < actOut.size(); ++j)
        {
            actOut(j) = actOut(j) ^ outShare[i].mShares[0](j);
        }
    }

    if (actOut != expOut)
    {
        std::cout << "act " << actOut << std::endl << "exp  " << expOut << std::endl;
        //std::cout << "   = ";
        //for (u64 i = 0; i < 3; ++i)
        //	std::cout << outShare[i].mShares[0](0) << " ^ ";
        //std::cout << std::endl;

        throw std::runtime_error(LOCATION);
    }

}

void Lynx_BinaryEngine_and_test()
{

    BetaLibrary lib;
    u64 size = 64, width = 1;

    // and
    {
        auto cir = lib.int_int_bitwiseAnd(size, size, size);
        cir->levelByAndDepth();

        Lynx_BinaryEngine_test(cir, [](i64 a, i64 b) {return a & b; });
    }

    // na_and
    {
        auto* cd = new BetaCircuit;

        BetaBundle a(size);
        BetaBundle b(size);
        BetaBundle c(size);

        cd->addInputBundle(a);
        cd->addInputBundle(b);

        cd->addOutputBundle(c);

        //int_int_bitwiseAnd_build(*cd, a, b, c);
        for (u64 j = 0; j < c.mWires.size(); ++j)
        {
            cd->addGate(
                a.mWires[j],
                b.mWires[j],
                GateType::na_And,
                c.mWires[j]);
        }


        Lynx_BinaryEngine_test(cd, [](i64 a, i64 b) {
            return ~a & b;
        });

    }

}


void Lynx_BinaryEngine_add_test()
{
    BetaLibrary lib;
    u64 size = 64, width = 1;
    auto cir = lib.int_int_add(size, size, size, BetaLibrary::Optimized::Depth);

    Lynx_BinaryEngine_test(cir, [](i64 a, i64 b) {return a + b; });

}


void Lynx_BinaryEngine_add_msb_test()
{
    BetaLibrary lib;
    u64 size = 64, width = 1;
    auto cir = lib.int_int_add_msb(size);

    Lynx_BinaryEngine_test(cir, [size](i64 a, i64 b) {return (a + b) >> (size - 1); });
}




void Lynx_cir_piecewise_Helper_test()
{
    u64 size = 64;
    u64 dec = 16;

    Lynx::Piecewise pp(dec);



}

void Lynx_Piecewise_plain_test()
{
    u64 dec = 16;
    u64 n = 64;

    Lynx::Engine::value_type_matrix
        input(n, 1),
        output(n, 1);

    PRNG prng(ZeroBlock);
    std::normal_distribution<double> dist(0.0, 2.0);

    for (u64 i = 0; i < n; ++i)
    {
        input(i) = dist(prng);
    }

    {
        Lynx::Piecewise maxZero(dec);
        maxZero.mThresholds.resize(1, 0);
        maxZero.mCoefficients.resize(2);
        maxZero.mCoefficients[1].resize(2, 0);
        maxZero.mCoefficients[1][1] = 1;


        maxZero.eval(input, output, false);


        for (u64 i = 0; i < n; ++i)
        {
            auto& out = output(i);
            auto& in = input(i);
            auto diff = std::abs(out - std::max(0.0, in));
            auto tol = 1.0 / (1 << dec);
            if (diff > tol)
                throw std::runtime_error(LOCATION);
        }
    }

    // piecewise linear logistic function
    {
        Lynx::Piecewise ll(dec);
        ll.mThresholds.resize(2, 0);

        ll.mThresholds[0] = -0.5;
        ll.mThresholds[1] = 0.5;


        ll.mCoefficients.resize(3);
        ll.mCoefficients[1].resize(2);
        ll.mCoefficients[1][0] = 0.5;
        ll.mCoefficients[1][1] = 1;

        ll.mCoefficients[2].resize(1);
        ll.mCoefficients[2][0] = 1;

        ll.eval(input, output);

        for (u64 i = 0; i < n; ++i)
        {
            auto& out = output(i);
            auto& in = input(i);
            auto diff = std::abs(out - std::min(std::max(0.0, in + 0.5), 1.0));
            auto tol = 1.0 / (1 << dec);
            if (diff > tol)
                throw std::runtime_error(LOCATION);
        }
    }


}

void Lynx_Piecewise_test()
{
    IOService ios;
    Session ep01(ios, "127.0.0.1:1313", SessionMode::Server, "01");
    Session ep10(ios, "127.0.0.1:1313", SessionMode::Client, "01");
    Session ep02(ios, "127.0.0.1:1313", SessionMode::Server, "02");
    Session ep20(ios, "127.0.0.1:1313", SessionMode::Client, "02");
    Session ep12(ios, "127.0.0.1:1313", SessionMode::Server, "12");
    Session ep21(ios, "127.0.0.1:1313", SessionMode::Client, "12");


    u64 size = 2;
    u64 trials = 1;
    u64 trials2 = 4;
    u64 numThresholds = 4;
    u64 dec = 16;
    Lynx::Engine p0, p1, p2;
    auto t0 = std::thread([&]() {p0.init(0, ep02, ep01, dec, toBlock(34265));  });
    auto t1 = std::thread([&]() {p1.init(1, ep10, ep12, dec, toBlock(42534));  });
    auto t2 = std::thread([&]() {p2.init(2, ep21, ep20, dec, toBlock(35123));  });
    t0.join();
    t1.join();
    t2.join();
    PRNG prng(ZeroBlock);
    std::normal_distribution<double> dist(3.0);

    for (u64 t = 0; t < trials; ++t)
    {

        Lynx::Piecewise pw0(dec), pw1(dec), pw2(dec);
        pw0.mCoefficients.resize(numThresholds + 1);
        pw1.mCoefficients.resize(numThresholds + 1);
        pw2.mCoefficients.resize(numThresholds + 1);

        std::vector<double> tt(numThresholds);

        for (u64 h = 0; h < numThresholds + 1; ++h)
        {

            auto degree = i64(prng.get<u64>() % 3) - 1;

            pw0.mCoefficients[h].resize(degree + 1);
            pw1.mCoefficients[h].resize(degree + 1);
            pw2.mCoefficients[h].resize(degree + 1);


            if (degree >= 0)
            {
                auto coef = dist(prng);
                pw0.mCoefficients[h][0] = coef;
                pw1.mCoefficients[h][0] = coef;
                pw2.mCoefficients[h][0] = coef;
            }

            if (degree >= 1)
            {
                auto coef = prng.get<i64>() % 100;
                pw0.mCoefficients[h][1] = coef;
                pw1.mCoefficients[h][1] = coef;
                pw2.mCoefficients[h][1] = coef;
            }

            if (h != numThresholds)
            {
                tt[h] = dist(prng);
            }
        }
        std::sort(tt.begin(), tt.end());

        pw0.mThresholds.resize(numThresholds);
        pw1.mThresholds.resize(numThresholds);
        pw2.mThresholds.resize(numThresholds);
        for (u64 h = 0; h < numThresholds; ++h)
        {
            pw0.mThresholds[h] = tt[h];
            pw1.mThresholds[h] = tt[h];
            pw2.mThresholds[h] = tt[h];
        }


        for (u64 t2 = 0; t2 < trials2; ++t2)
        {

            //std::cout << "====================================" << std::endl;
            Lynx::value_type_matrix
                plain_input(size, 1),
                plain_output(size, 1);

            Lynx::word_matrix
                word_input(size, 1),
                word_output(size, 1);

            Lynx::Matrix
                input0(size, 1),
                input1(size, 1),
                input2(size, 1),
                output0(size, 1),
                output1(size, 1),
                output2(size, 1);

            for (u64 i = 0; i < size; ++i)
            {
                plain_input(i) = dist(prng);
                word_input(i) = plain_input(i) * (1ull << dec);
            }
            createSharing(prng, word_input, input0, input1, input2);

            //if (t2 != 3) continue;

            pw0.eval(plain_input, plain_output, false);

            for (u64 i = 0; i < size; ++i)
                word_output(i) = plain_output(i) * (1ull << dec);


            auto thrd0 = std::thread([&]() {pw0.eval(p0, input0, output0, t2 == 3); });
            auto thrd1 = std::thread([&]() {pw1.eval(p1, input1, output1, t2 == 3); });
            auto thrd2 = std::thread([&]() {pw2.eval(p2, input2, output2, t2 == 3); });

            thrd0.join();
            thrd1.join();
            thrd2.join();

            if (output0.mShares[0] != output1.mShares[1]) throw std::runtime_error(LOCATION);
            if (output1.mShares[0] != output2.mShares[1]) throw std::runtime_error(LOCATION);
            if (output2.mShares[0] != output0.mShares[1]) throw std::runtime_error(LOCATION);

            auto out = (output0.mShares[0] + output0.mShares[1] + output1.mShares[0]).eval();

            for (u64 i = 0; i < size; ++i)
            {
                auto actual = out(i);
                auto exp = word_output(i);
                auto diff = std::abs(actual - exp);


                std::cout << t << " " << t2 << " " << i << ": "
                    << output0.mShares[0](i) << " "
                    << output0.mShares[1](i) << " "
                    << output1.mShares[0](i) << std::endl;

                if (diff > 100)
                {
                    std::cout << t << " " << t2 << " " << i << std::endl;
                    std::cout << exp << " " << actual << " " << diff << std::endl;
                    throw std::runtime_error(LOCATION);
                }
            }
        }

    }

}

void Lynx_preproc_test()
{
    u64 dec = 16;
    auto cir = Lynx::Engine::makePreprocCircuit(dec);


    cir.levelByAndDepth();

}
extern i64 signExtend(i64 v, u64 b, bool print = false);


void Lynx_argmax_test()
{
    u64 dec = 16, numArgs = 10, size = 40;

    auto cir = Lynx::Engine::makeArgMaxCircuit(dec, numArgs);

    std::vector<i64> a0(numArgs), a1(numArgs);
    std::vector<BitVector> inputs(numArgs * 2), outputs(1);
    outputs[0].resize(log2ceil(numArgs));

    PRNG prng(ZeroBlock);

    auto max = std::numeric_limits<i64>::min();
    auto argMax = 0;

    auto mask = (i64(1) << size) - 1;

    for (u64 i = 0; i < numArgs; ++i)
    {

        a0[i] = signExtend(prng.get<i64>(), size);
        a1[i] = signExtend(prng.get<i64>(), size);

        auto ai = signExtend(a0[i] + a1[i], size);

        //std::cout << "aa[" << i << "] = " << signExtend(ai, size) << " " << BitVector((u8*)&ai, size) << std::endl;;

        if (ai > max)
        {
            max = ai;
            argMax = i;
        }

        inputs[i * 2 + 0].append((u8*)&a0[i], size);
        inputs[i * 2 + 1].append((u8*)&a1[i], size);
    }

    BitVector exp((u8*)&argMax, log2ceil(numArgs));
    cir.evaluate(inputs, outputs, false);

    if (exp != outputs[0])
        throw std::runtime_error(LOCATION);

}

void Lynx_asyncArithBinMul_test()
{
    IOService ios;
    Session ep01(ios, "127.0.0.1:1313", SessionMode::Server, "01");
    Session ep10(ios, "127.0.0.1:1313", SessionMode::Client, "01");
    Session ep02(ios, "127.0.0.1:1313", SessionMode::Server, "02");
    Session ep20(ios, "127.0.0.1:1313", SessionMode::Client, "02");
    Session ep12(ios, "127.0.0.1:1313", SessionMode::Server, "12");
    Session ep21(ios, "127.0.0.1:1313", SessionMode::Client, "12");



    PRNG prng(ZeroBlock);
    u64 size = 100;
    u64 dec = 16;
    Lynx::Engine p0, p1, p2;
    auto t0 = std::thread([&]() {p0.init(0, ep02, ep01, dec, toBlock(34265));  });
    auto t1 = std::thread([&]() {p1.init(1, ep10, ep12, dec, toBlock(42534));  });
    auto t2 = std::thread([&]() {p2.init(2, ep21, ep20, dec, toBlock(35123));  });
    t0.join();
    t1.join();
    t2.join();

    Lynx::Engine::word_matrix a(size, 1), b(size, 1), c(size, 1), cc(size, 1);

    Lynx::Engine::Matrix
        a0(size, 1),
        a1(size, 1),
        a2(size, 1),
        b0(size, 1),
        b1(size, 1),
        b2(size, 1),
        c0(size, 1),
        c1(size, 1),
        c2(size, 1);

    u64 trials = 10;
    for (u64 t = 0; t < trials; ++t)
    {
        for (u64 i = 0; i < size; ++i)
            b(i) = prng.get<bool>();

        prng.get(a.data(), a.size());

        for (u64 i = 0; i < size; ++i)
        {
            a(i) = a(i) >> 32;
            c(i) = a(i) *b(i);
        }

        createBinarySharing(prng, b, b0, b1, b2);
        createSharing(prng, a, a0, a1, a2);

        for (u64 i = 0; i < size; ++i)
        {
            //if (a(i) != a0(i)[0] + a0(i)[1] + a1(i)[0])
            //	throw std::runtime_error(LOCATION);
            //std::cout << "a[" << i << "] = " << a0(i)[0] << " + " << a0(i)[1] << " + " << a1(i)[0] << std::endl;

            b0.mShares[0](i) &= 1;
            b0.mShares[1](i) &= 1;
            b1.mShares[0](i) &= 1;
            b1.mShares[1](i) &= 1;
            b2.mShares[0](i) &= 1;
            b2.mShares[1](i) &= 1;
        }

        auto as0 = p0.asyncArithBinMul(a0, b0, c0);
        auto as1 = p1.asyncArithBinMul(a1, b1, c1);
        auto as2 = p2.asyncArithBinMul(a2, b2, c2);

        as0.get();
        as1.get();
        as2.get();

        //t0 = std::thread([&]() { p0.asyncArithBinMul(a0, b0, c0).get(); });
        //t1 = std::thread([&]() { p1.asyncArithBinMul(a1, b1, c1).get(); });
        //t2 = std::thread([&]() { p2.asyncArithBinMul(a2, b2, c2).get(); });
        //t0.join();
        //t1.join();
        //t2.join();

        //typedef LynxEngine::Word Word;
        for (u64 i = 0; i < size; ++i)
        {

            if (c0.mShares[0] != c1.mShares[1])
                throw std::runtime_error(LOCATION);
            if (c1.mShares[0] != c2.mShares[1])
                throw std::runtime_error(LOCATION);
            if (c2.mShares[0] != c0.mShares[1])
                throw std::runtime_error(LOCATION);
            auto ci0 = c0.mShares[0](i);
            auto ci1 = c1.mShares[0](i);
            auto ci2 = c2.mShares[0](i);
            auto di0 = c0.mShares[1](i);
            auto di1 = c1.mShares[1](i);
            auto di2 = c2.mShares[1](i);
            //std::cout << "ci0 " << ci0 << " " << di1 << std::endl;
            //std::cout << "ci1 " << ci1 << " " << di2 << std::endl;
            //std::cout << "ci2 " << ci2 << " " << di0 << std::endl;

            cc(i) = p0.shareToWord(c0(i), c1(i)[0]);// ^ c2(i);
            auto ai = a(i);
            auto bi = b(i);
            auto cci = cc(i);
            auto ci = c(i);
            if (cc(i) != c(i))
            {
                throw std::runtime_error(LOCATION);
            }
        }
    }
}

void Lynx_asyncPubArithBinMul_test()
{
    IOService ios;
    Session ep01(ios, "127.0.0.1:1313", SessionMode::Server, "01");
    Session ep10(ios, "127.0.0.1:1313", SessionMode::Client, "01");
    Session ep02(ios, "127.0.0.1:1313", SessionMode::Server, "02");
    Session ep20(ios, "127.0.0.1:1313", SessionMode::Client, "02");
    Session ep12(ios, "127.0.0.1:1313", SessionMode::Server, "12");
    Session ep21(ios, "127.0.0.1:1313", SessionMode::Client, "12");

    PRNG prng(ZeroBlock);
    u64 size = 100;
    u64 dec = 16;
    Lynx::Engine p0, p1, p2;
    auto t0 = std::thread([&]() {p0.init(0, ep02, ep01, dec, toBlock(34265));  });
    auto t1 = std::thread([&]() {p1.init(1, ep10, ep12, dec, toBlock(42534));  });
    auto t2 = std::thread([&]() {p2.init(2, ep21, ep20, dec, toBlock(35123));  });
    t0.join();
    t1.join();
    t2.join();

    Lynx::Engine::word_matrix b(size, 1), c(size, 1), cc(size, 1);

    Lynx::Engine::Matrix
        b0(size, 1),
        b1(size, 1),
        b2(size, 1),
        c0(size, 1),
        c1(size, 1),
        c2(size, 1);

    Lynx::Word a = prng.get<Lynx::Word>();

    for (u64 i = 0; i < size; ++i)
        b(i) = prng.get<bool>();

    for (u64 i = 0; i < size; ++i)
    {
        c(i) = a * b(i);
    }

    createBinarySharing(prng, b, b0, b1, b2);

    for (u64 i = 0; i < size; ++i)
    {
        //if (a(i) != a0(i)[0] + a0(i)[1] + a1(i)[0])
        //	throw std::runtime_error(LOCATION);
        //std::cout << "a[" << i << "] = " << a0(i)[0] << " + " << a0(i)[1] << " + " << a1(i)[0] << std::endl;

        b0.mShares[0](i) &= 1;
        b0.mShares[1](i) &= 1;
        b1.mShares[0](i) &= 1;
        b1.mShares[1](i) &= 1;
        b2.mShares[0](i) &= 1;
        b2.mShares[1](i) &= 1;
    }

    auto as0 = p0.asyncArithBinMul(a, b0, c0);
    auto as1 = p1.asyncArithBinMul(a, b1, c1);
    auto as2 = p2.asyncArithBinMul(a, b2, c2);

    as0.get();
    as1.get();
    as2.get();

    //t0 = std::thread([&]() { p0.asyncArithBinMul(a0, b0, c0).get(); });
    //t1 = std::thread([&]() { p1.asyncArithBinMul(a1, b1, c1).get(); });
    //t2 = std::thread([&]() { p2.asyncArithBinMul(a2, b2, c2).get(); });
    //t0.join();
    //t1.join();
    //t2.join();

    //typedef LynxEngine::Word Word;
    for (u64 i = 0; i < size; ++i)
    {

        if (c0.mShares[0] != c1.mShares[1])
            throw std::runtime_error(LOCATION);
        if (c1.mShares[0] != c2.mShares[1])
            throw std::runtime_error(LOCATION);
        if (c2.mShares[0] != c0.mShares[1])
            throw std::runtime_error(LOCATION);
        auto ci0 = c0.mShares[0](i);
        auto ci1 = c1.mShares[0](i);
        auto ci2 = c2.mShares[0](i);
        auto di0 = c0.mShares[1](i);
        auto di1 = c1.mShares[1](i);
        auto di2 = c2.mShares[1](i);
        //std::cout << "ci0 " << ci0 << " " << di1 << std::endl;
        //std::cout << "ci1 " << ci1 << " " << di2 << std::endl;
        //std::cout << "ci2 " << ci2 << " " << di0 << std::endl;

        cc(i) = p0.shareToWord(c0(i), c1(i)[0]);// ^ c2(i);
        auto bi = b(i);
        auto cci = cc(i);
        auto ci = c(i);
        if (cc(i) != c(i))
        {
            throw std::runtime_error(LOCATION);
        }
    }
}

void Lynx_asyncInputBinary_test()
{
    using namespace Lynx;
    IOService ios;
    Session ep01(ios, "127.0.0.1:1313", SessionMode::Server, "01");
    Session ep10(ios, "127.0.0.1:1313", SessionMode::Client, "01");
    Session ep02(ios, "127.0.0.1:1313", SessionMode::Server, "02");
    Session ep20(ios, "127.0.0.1:1313", SessionMode::Client, "02");
    Session ep12(ios, "127.0.0.1:1313", SessionMode::Server, "12");
    Session ep21(ios, "127.0.0.1:1313", SessionMode::Client, "12");

    PRNG prng(ZeroBlock);
    u64 size = 100;
    u64 dec = 16;
    auto t0 = std::thread([&]() {
        Engine p0;
        p0.init(0, ep02, ep01, dec, toBlock(34265));
        Lynx::word_matrix in(100, 1);
        Lynx::Matrix inShare0(100, 1), inShare1(100, 1);
        for (u64 i = 0; i < in.size(); ++i)
            in(i) = i;
        p0.localBinaryInput(in, inShare0);


        for (u64 i = 0; i < in.size(); ++i)
            in(i) = 110 * i;
        p0.localBinaryInput(in, inShare1);

        for (u64 i = 0; i < in.size(); ++i) {
            inShare0.mShares[0](i) ^= inShare1.mShares[0](i);
            inShare0.mShares[1](i) ^= inShare1.mShares[1](i);
        }

        auto ret = p0.reconstructShare(inShare0, ShareType::Binary);
        bool failed = false;
        for (u64 i = 0; i < in.size(); ++i)
        {
            if (ret(i) != ((i * 110) ^ i))
            {
                failed = true;
                std::cout << "bad[" << i << "] " << ret(i) << "  exp " << ((i * 110) ^ i) << std::endl;
            }
        }
        if (failed)
            throw std::runtime_error(LOCATION);
        //p0.localBinaryInput()
    });
    auto t1 = std::thread([&]() {
        Engine p1;
        p1.init(1, ep10, ep12, dec, toBlock(42534));

        Lynx::Matrix inShare0(100, 1), inShare1(100, 1);
        p1.remoteBinaryInput(0, inShare0);
        p1.remoteBinaryInput(0, inShare1);
        for (u64 i = 0; i < inShare0.size(); ++i) {
            inShare0.mShares[0](i) ^= inShare1.mShares[0](i);
            inShare0.mShares[1](i) ^= inShare1.mShares[1](i);
        }
        p1.reconstructShare(inShare0, ShareType::Binary);

    });
    auto t2 = std::thread([&]() {
        Engine p2;
        p2.init(2, ep21, ep20, dec, toBlock(35123));

        Lynx::Matrix inShare0(100, 1), inShare1(100, 1);
        p2.remoteBinaryInput(0, inShare0);
        p2.remoteBinaryInput(0, inShare1);
        for (u64 i = 0; i < inShare0.size(); ++i) {
            inShare0.mShares[0](i) ^= inShare1.mShares[0](i);
            inShare0.mShares[1](i) ^= inShare1.mShares[1](i);
        }
        p2.reconstructShare(inShare0, ShareType::Binary);
    });

    t1.join();
    t2.join();
    t0.join();


}



void Lynx_asyncConvertToPackedBinary_test()
{
    using namespace Lynx;
    IOService ios;
    Session ep01(ios, "127.0.0.1:1313", SessionMode::Server, "01");
    Session ep10(ios, "127.0.0.1:1313", SessionMode::Client, "01");
    Session ep02(ios, "127.0.0.1:1313", SessionMode::Server, "02");
    Session ep20(ios, "127.0.0.1:1313", SessionMode::Client, "02");
    Session ep12(ios, "127.0.0.1:1313", SessionMode::Server, "12");
    Session ep21(ios, "127.0.0.1:1313", SessionMode::Client, "12");

    PRNG prng(ZeroBlock);
    u64 size = 100;
    u64 dec = 16;
    auto t0 = std::thread([&]() {
        Engine p0;
        p0.init(0, ep02, ep01, dec, toBlock(34265));
        Lynx::word_matrix in(100, 1);
        Lynx::Matrix inShare0(100, 1);
        Lynx::PackedBinMatrix pack0;
        for (u64 i = 0; i < in.size(); ++i)
            in(i) = i;

        p0.localInput(in, inShare0);
        p0.asyncConvertToPackedBinary(inShare0, pack0, { p0.mNext, p0.mPrev });
        p0.sync();
        auto ret = p0.reconstructShare(pack0, 0);

        bool failed = false;
        for (u64 i = 0; i < in.size(); ++i)
        {
            if (ret(i) != i)
            {
                failed = true;
                std::cout << "bad[" << i << "] " << ret(i) << "  exp " << i << std::endl;
            }
        }
        if (failed)
            throw std::runtime_error(LOCATION);
        //p0.localBinaryInput()
    });
    auto t1 = std::thread([&]() {
        Engine p1;
        p1.init(1, ep10, ep12, dec, toBlock(42534));

        Lynx::Matrix inShare0(100, 1);
        Lynx::PackedBinMatrix pack0;
        p1.remoteInput(0, inShare0);
        p1.asyncConvertToPackedBinary(inShare0, pack0, { p1.mNext, p1.mPrev });
        p1.sync();
        auto ret = p1.reconstructShare(pack0, 0);


    });
    auto t2 = std::thread([&]() {
        Engine p2;
        p2.init(2, ep21, ep20, dec, toBlock(35123));

        Lynx::Matrix inShare0(100, 1);
        Lynx::PackedBinMatrix pack0;
        p2.remoteInput(0, inShare0);
        p2.asyncConvertToPackedBinary(inShare0, pack0, { p2.mNext, p2.mPrev });
        p2.sync();
        auto ret = p2.reconstructShare(pack0, 0);

    });

    t1.join();
    t2.join();
    t0.join();


}

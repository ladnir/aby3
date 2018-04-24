

#include <cryptoTools/Network/IOService.h>
#include <cryptoTools/Network/Session.h>
#include <cryptoTools/Network/Channel.h>
#include <cryptoTools/Common/Matrix.h>
#include <com-psi/OblvPermutation.h>
#include <com-psi/OblvSwitchNet.h>
#include <cryptoTools/Crypto/PRNG.h>
#include "PermutaitonTests.h"
#include <iomanip>

using namespace oc;

void Perm3p_overwrite_Test()
{
    IOService ios;
    Session s01(ios, "127.0.0.1", SessionMode::Server, "01");
    Session s10(ios, "127.0.0.1", SessionMode::Client, "01");
    Session s02(ios, "127.0.0.1", SessionMode::Server, "02");
    Session s20(ios, "127.0.0.1", SessionMode::Client, "02");
    Session s12(ios, "127.0.0.1", SessionMode::Server, "12");
    Session s21(ios, "127.0.0.1", SessionMode::Client, "12");

    Channel chl01 = s01.addChannel();
    Channel chl10 = s10.addChannel();
    Channel chl02 = s02.addChannel();
    Channel chl20 = s20.addChannel();
    Channel chl12 = s12.addChannel();
    Channel chl21 = s21.addChannel();


    int rows = 100;
    int bytes = 16;

    std::vector<u32> perm(rows);
    for (u32 i = 0; i < perm.size(); ++i)
        perm[i] = i;


    PRNG prng(OneBlock);
    std::random_shuffle(perm.begin(), perm.end(), prng);

    Matrix<u8> mtx(rows, bytes);
    Matrix<u8> s1(rows, bytes);
    Matrix<u8> s2(rows, bytes);

    prng.get(mtx.data(), mtx.size());
    //std::cout << std::endl;
    //for (u32 i = 0; i < mtx.rows(); ++i)
    //{
    //    std::cout << i << " -> " << perm[i] << " : ";
    //    for (u32 j = 0; j < mtx.cols(); ++j)
    //    {
    //        std::cout << " " << std::hex << int(mtx(i, j));
    //    }
    //    std::cout << std::endl;
    //}

    //auto t0 = std::thread([&]() 
    {
        OblvPermutation p;
        auto perm2 = perm;
        p.program(chl02, chl01, perm2, prng, s1);
    }//);
    //std::cout << std::endl;

    //auto t1 = std::thread([&]() 
    {
        OblvPermutation p;
        p.send(chl10, chl12, mtx);
    }//);
    //std::cout << std::endl;

    OblvPermutation p;
    p.recv(chl20, chl21, s2);

    //t0.join();
    //t1.join();
    //std::cout << std::endl;

    //for (u32 i = 0; i < mtx.rows(); ++i)
    //{
    //    std::cout << i << " : ";
    //    for (u32 j = 0; j < mtx.cols(); ++j)
    //    {
    //        std::cout << " " << std::hex << int(s1(i, j) ^ s2(i, j));
    //    }
    //    std::cout << std::endl;
    //}

    bool failed = false;
    for (u32 i = 0; i < mtx.rows(); ++i)
    {
        for (u32 j = 0; j < mtx.cols(); ++j)
        {
            auto val = s1(perm[i], j) ^ s2(perm[i], j);
            if (mtx(i, j) != val)
            {
                std::cout << "mtx(" << i << ", " << j << ") != s1(" << perm[i] << ", " << j << ") ^ s2(" << perm[i] << ", " << j << ")" << std::endl;
                std::cout << "   " << int(mtx(i, j)) << " != " << int(s1(perm[i], j)) << " ^ " << int(s2(perm[i], j)) << std::endl;
                std::cout << "   " << int(mtx(i, j)) << " != " << int(s1(perm[i], j) ^ s2(perm[i], j)) << std::endl;
                failed = true;
            }
        }
    }
    if (failed)
        throw std::runtime_error(LOCATION);
}




void Perm3p_additive_Test()
{
    IOService ios;
    Session s01(ios, "127.0.0.1", SessionMode::Server, "01");
    Session s10(ios, "127.0.0.1", SessionMode::Client, "01");
    Session s02(ios, "127.0.0.1", SessionMode::Server, "02");
    Session s20(ios, "127.0.0.1", SessionMode::Client, "02");
    Session s12(ios, "127.0.0.1", SessionMode::Server, "12");
    Session s21(ios, "127.0.0.1", SessionMode::Client, "12");

    Channel chl01 = s01.addChannel();
    Channel chl10 = s10.addChannel();
    Channel chl02 = s02.addChannel();
    Channel chl20 = s20.addChannel();
    Channel chl12 = s12.addChannel();
    Channel chl21 = s21.addChannel();


    int rows = 100;
    int bytes = 16;

    std::vector<u32> perm(rows);
    for (u32 i = 0; i < perm.size(); ++i)
        perm[i] = i;


    PRNG prng(OneBlock);
    std::random_shuffle(perm.begin(), perm.end(), prng);

    Matrix<u8> mtx(rows, bytes);
    Matrix<u8> s1(rows, bytes);
    Matrix<u8> s2(rows, bytes);

    prng.get(mtx.data(), mtx.size());
    prng.get(s1.data(), s1.size());
    s2 = s1;
    //std::cout << std::endl;
    //for (u32 i = 0; i < mtx.rows(); ++i)
    //{
    //    std::cout << i << " -> " << perm[i] << " : ";
    //    for (u32 j = 0; j < mtx.cols(); ++j)
    //    {
    //        std::cout << " " << std::hex << int(mtx(i, j));
    //    }
    //    std::cout << std::endl;
    //}

    //auto t0 = std::thread([&]() 
    {
        OblvPermutation p;
        auto perm2 = perm;
        p.program(chl02, chl01, perm2, prng, s1, OutputType::Additive);
    }//);
     //std::cout << std::endl;

     //auto t1 = std::thread([&]() 
    {
        OblvPermutation p;
        p.send(chl10, chl12, mtx);
    }//);
     //std::cout << std::endl;

    OblvPermutation p;
    p.recv(chl20, chl21, s2, OutputType::Additive);

    //t0.join();
    //t1.join();
    //std::cout << std::endl;

    //for (u32 i = 0; i < mtx.rows(); ++i)
    //{
    //    std::cout << i << " : ";
    //    for (u32 j = 0; j < mtx.cols(); ++j)
    //    {
    //        std::cout << " " << std::hex << int(s1(i, j) ^ s2(i, j));
    //    }
    //    std::cout << std::endl;
    //}

    bool failed = false;
    for (u32 i = 0; i < mtx.rows(); ++i)
    {
        for (u32 j = 0; j < mtx.cols(); ++j)
        {
            auto val = s1(perm[i], j) ^ s2(perm[i], j);
            if (mtx(i, j) != val)
            {
                std::cout << "mtx(" << i << ", " << j << ") != s1(" << perm[i] << ", " << j << ") ^ s2(" << perm[i] << ", " << j << ")" << std::endl;
                std::cout << "   " << int(mtx(i, j)) << " != " << int(s1(perm[i], j)) << " ^ " << int(s2(perm[i], j)) << std::endl;
                std::cout << "   " << int(mtx(i, j)) << " != " << int(s1(perm[i], j) ^ s2(perm[i], j)) << std::endl;
                failed = true;
            }
        }
    }
    if (failed)
        throw std::runtime_error(LOCATION);
}


void Perm3p_subset_Test()
{
    IOService ios;
    Session s01(ios, "127.0.0.1", SessionMode::Server, "01");
    Session s10(ios, "127.0.0.1", SessionMode::Client, "01");
    Session s02(ios, "127.0.0.1", SessionMode::Server, "02");
    Session s20(ios, "127.0.0.1", SessionMode::Client, "02");
    Session s12(ios, "127.0.0.1", SessionMode::Server, "12");
    Session s21(ios, "127.0.0.1", SessionMode::Client, "12");

    Channel chl01 = s01.addChannel();
    Channel chl10 = s10.addChannel();
    Channel chl02 = s02.addChannel();
    Channel chl20 = s20.addChannel();
    Channel chl12 = s12.addChannel();
    Channel chl21 = s21.addChannel();


    int rows = 100;
    int destRows = 50;
    int bytes = 2;

    std::vector<u32> perm(rows, -1);
    for (u32 i = 0; i < destRows; ++i)
        perm[i] = i;


    PRNG prng(OneBlock);
    std::random_shuffle(perm.begin(), perm.end(), prng);

    Matrix<u8> mtx(rows, bytes);
    Matrix<u8> s1(destRows, bytes);
    Matrix<u8> s2(destRows, bytes);

    prng.get(mtx.data(), mtx.size());
    //std::cout << std::endl;
    //for (u32 i = 0; i < mtx.rows(); ++i)
    //{
    //    std::cout << i << " -> " << perm[i] << " : ";
    //    for (u32 j = 0; j < mtx.cols(); ++j)
    //    {
    //        std::cout << " " << std::hex << int(mtx(i, j));
    //    }
    //    std::cout << std::endl;
    //}

    //auto t0 = std::thread([&]() 
    {
        OblvPermutation p;
        auto perm2 = perm;
        p.program(chl02, chl01, perm2, prng, s1);
    }//);
     //std::cout << std::endl;

     //auto t1 = std::thread([&]() 
    {
        OblvPermutation p;
        p.send(chl10, chl12, mtx);
    }//);
     //std::cout << std::endl;

    OblvPermutation p;
    p.recv(chl20, chl21, s2);

    //t0.join();
    //t1.join();
    //std::cout << std::endl;

    //for (u32 i = 0; i < mtx.rows(); ++i)
    //{
    //    std::cout << i << " : ";
    //    for (u32 j = 0; j < mtx.cols(); ++j)
    //    {
    //        std::cout << " " << std::hex << int(s1(i, j) ^ s2(i, j));
    //    }
    //    std::cout << std::endl;
    //}

    bool failed = false;
    for (u32 i = 0; i < rows; ++i)
    {
        if (perm[i] != -1)
        {
            auto s = perm[i];
            for (u32 j = 0; j < bytes; ++j)
            {
                auto val = s1(s, j) ^ s2(s, j);
                if (mtx(i, j) != val)
                {
                    std::cout << "mtx(" << i << ", " << j << ") != s1(" << s << ", " << j << ") ^ s2(" << s << ", " << j << ")" << std::endl;
                    std::cout << "   " << int(mtx(i, j)) << " != " << int(s1(s, j)) << " ^ " << int(s2(s, j)) << std::endl;
                    std::cout << "   " << int(mtx(i, j)) << " != " << int(s1(s, j) ^ s2(s, j)) << std::endl;
                    failed = true;
                }
            }
        }
    }
    if (failed)
        throw std::runtime_error(LOCATION);
}





void switch_select_test()
{
    IOService ios;
    Session s01(ios, "127.0.0.1", SessionMode::Server, "01");
    Session s10(ios, "127.0.0.1", SessionMode::Client, "01");
    Session s02(ios, "127.0.0.1", SessionMode::Server, "02");
    Session s20(ios, "127.0.0.1", SessionMode::Client, "02");
    Session s12(ios, "127.0.0.1", SessionMode::Server, "12");
    Session s21(ios, "127.0.0.1", SessionMode::Client, "12");

    Channel chl01 = s01.addChannel();
    Channel chl10 = s10.addChannel();
    Channel chl02 = s02.addChannel();
    Channel chl20 = s20.addChannel();
    Channel chl12 = s12.addChannel();
    Channel chl21 = s21.addChannel();


    int trials = 100;
    int srcSize = 40;
    int destSize = 20;
    int bytes = 1;

    Matrix<u8> src(srcSize, bytes);
    Matrix<u8> dest0(destSize, bytes), dest1(destSize, bytes);


    for (auto t = 9; t < trials; ++t)
    {
        PRNG prng(toBlock(t));

        prng.get(src.data(), src.size());

        //for (auto i = 0; i < src.rows(); ++i)
        //{
        //    std::cout << "s[" << i << "] = ";
        //    for (auto j = 0; j < src.cols(); ++j)
        //    {
        //        std::cout << " " << std::setw(2) << std::hex << int(src(i, j));
        //    }
        //    std::cout << std::endl << std::dec;
        //}

        OblvSwitchNet::Program prog;
        prog.init(srcSize, destSize);

        for (u64 i = 0; i < destSize; ++i)
        {
            prog.addSwitch(prng.get<u32>() % srcSize, i);
            //std::cout << "switch[" << i << "] = " << prog.mSrcDests[i][0] << " -> " << prog.mSrcDests[i][1] << std::endl;
        }


        auto t0 = std::thread([&]() {
            OblvSwitchNet snet;
            snet.programSelect(chl02, chl01, prog, prng, dest0);
        });

        auto t1 = std::thread([&]() {
            OblvSwitchNet snet;
            snet.sendSelect(chl10, chl12, src);
        });

        auto t2 = std::thread([&]() {
            OblvSwitchNet snet;
            snet.recvSelect(chl20, chl21, dest1);
        });

        t0.join();
        t1.join();
        t2.join();

        auto last = -1;

        bool print = false;
        if (print)
            std::cout << std::endl;
        bool failed = false;
        for (u64 i = 0; i < prog.mSrcDests.size(); ++i)
        {
            auto s = prog.mSrcDests[i].mSrc;

            if (s != last)
            {
                for (auto j = 0; j < bytes; ++j)
                {
                    if ((dest0(i, j) ^ dest1(i, j)) != src(s, j))
                    {
                        failed = true;
                    }
                }

                if (print)
                {
                    std::cout << "d[" << i << "] = ";
                    for (auto j = 0; j < bytes; ++j)
                    {
                        if ((dest0(i, j) ^ dest1(i, j)) != src(s, j))
                            std::cout << Color::Red;
                        std::cout << ' ' << std::setw(2) << std::hex << int(dest0(i, j) ^ dest1(i, j)) << ColorDefault;
                    }
                    std::cout << "\ns[" << i << "] = ";
                    for (auto j = 0; j < bytes; ++j)
                        std::cout << ' ' << std::setw(2) << std::hex << int(src(s, j));

                    std::cout << std::endl << std::dec;
                }

                last = s;
            }

        }

        if (failed)
            throw std::runtime_error("");

    }
}



void switch_duplicate_test()
{
    IOService ios;
    Session s01(ios, "127.0.0.1", SessionMode::Server, "01");
    Session s10(ios, "127.0.0.1", SessionMode::Client, "01");
    Session s02(ios, "127.0.0.1", SessionMode::Server, "02");
    Session s20(ios, "127.0.0.1", SessionMode::Client, "02");
    Session s12(ios, "127.0.0.1", SessionMode::Server, "12");
    Session s21(ios, "127.0.0.1", SessionMode::Client, "12");

    Channel chl01 = s01.addChannel();
    Channel chl10 = s10.addChannel();
    Channel chl02 = s02.addChannel();
    Channel chl20 = s20.addChannel();
    Channel chl12 = s12.addChannel();
    Channel chl21 = s21.addChannel();


    int trials = 100;
    int srcSize = 50;
    int destSize = srcSize /2;
    int bytes = 1;

    Matrix<u8> src(srcSize, bytes);
    Matrix<u8> dest0(destSize, bytes), dest1(destSize, bytes);


    for (auto t = 0; t < trials; ++t)
    {
        PRNG prng(toBlock(t));

        prng.get(src.data(), src.size());

        //for (auto i = 0; i < src.rows(); ++i)
        //{
        //    std::cout << "s[" << i << "] = ";
        //    for (auto j = 0; j < src.cols(); ++j)
        //    {
        //        std::cout << " " << std::setw(2) << std::hex << int(src(i, j));
        //    }
        //    std::cout << std::endl << std::dec;
        //}

        OblvSwitchNet::Program prog;
        prog.init(srcSize, destSize);

        for (u64 i = 0; i < destSize; ++i)
        {
            prog.addSwitch(prng.get<u32>() % srcSize, i);
            //prog.addSwitch(0, i);
            //std::cout << "switch[" << i << "] = " << prog.mSrcDests[i][0] << " -> " << prog.mSrcDests[i][1] << std::endl;
        }


        auto t0 = std::thread([&]() {
            setThreadName("t0");
            OblvSwitchNet snet;
            snet.programSelect(chl02, chl01, prog, prng, dest0);
            snet.programDuplicate(chl02, chl01, prog, prng, dest0);
        });

        auto t1 = std::thread([&]() {
            setThreadName("t1");
            OblvSwitchNet snet;
            snet.sendSelect(chl10, chl12, src);
            snet.helpDuplicate(chl10, destSize, bytes);
        });

        auto t2 = std::thread([&]() {
            setThreadName("t2");
            OblvSwitchNet snet;
            snet.recvSelect(chl20, chl21, dest1);
            PRNG prng2(toBlock(585643));
            snet.sendDuplicate(chl20, prng2, dest1);
        });

        t0.join();
        t1.join();
        t2.join();


        bool print = false;
        if (print)
            std::cout << std::endl;
        bool failed = false;
        for (u64 i = 0; i < prog.mSrcDests.size(); ++i)
        {
            auto s = prog.mSrcDests[i].mSrc;

            for (auto j = 0; j < bytes; ++j)
            {
                if ((dest0(i, j) ^ dest1(i, j)) != src(s, j))
                {
                    failed = true;
                }
            }

            if (print)
            {
                std::cout << "d[" << i << "] = ";
                for (auto j = 0; j < bytes; ++j)
                {
                    if ((dest0(i, j) ^ dest1(i, j)) != src(s, j))
                        std::cout << Color::Red;
                    std::cout << ' ' << std::setw(2) << std::hex << int(dest0(i, j) ^ dest1(i, j)) << ColorDefault;
                }
                std::cout << "\ns[" << i << "] = ";
                for (auto j = 0; j < bytes; ++j)
                    std::cout << ' ' << std::setw(2) << std::hex << int(src(s, j));

                std::cout << std::endl << std::dec;
            }


        }

        if (failed)
            throw std::runtime_error("");

    }
}



void switch_full_test()
{
    IOService ios;
    Session s01(ios, "127.0.0.1", SessionMode::Server, "01");
    Session s10(ios, "127.0.0.1", SessionMode::Client, "01");
    Session s02(ios, "127.0.0.1", SessionMode::Server, "02");
    Session s20(ios, "127.0.0.1", SessionMode::Client, "02");
    Session s12(ios, "127.0.0.1", SessionMode::Server, "12");
    Session s21(ios, "127.0.0.1", SessionMode::Client, "12");

    Channel chl01 = s01.addChannel();
    Channel chl10 = s10.addChannel();
    Channel chl02 = s02.addChannel();
    Channel chl20 = s20.addChannel();
    Channel chl12 = s12.addChannel();
    Channel chl21 = s21.addChannel();


    int trials = 100;
    int srcSize = 50;
    int destSize = srcSize / 2;
    int bytes = 1;

    Matrix<u8> src(srcSize, bytes);
    Matrix<u8> dest0(destSize, bytes), dest1(destSize, bytes);


    for (auto t = 0; t < trials; ++t)
    {
        PRNG prng(toBlock(t));

        prng.get(src.data(), src.size());

        //for (auto i = 0; i < src.rows(); ++i)
        //{
        //    std::cout << "s[" << i << "] = ";
        //    for (auto j = 0; j < src.cols(); ++j)
        //    {
        //        std::cout << " " << std::setw(2) << std::hex << int(src(i, j));
        //    }
        //    std::cout << std::endl << std::dec;
        //}

        OblvSwitchNet::Program prog;
        prog.init(srcSize, destSize);

        for (u64 i = 0; i < destSize; ++i)
        {
            prog.addSwitch(prng.get<u32>() % srcSize, i);
            //prog.addSwitch(0, i);
            //std::cout << "switch[" << i << "] = " << prog.mSrcDests[i][0] << " -> " << prog.mSrcDests[i][1] << std::endl;
        }


        auto t0 = std::thread([&]() {
            setThreadName("t0");
            OblvSwitchNet snet;
            snet.program(chl02, chl01, prog, prng, dest0);
        });

        auto t1 = std::thread([&]() {
            setThreadName("t1");
            OblvSwitchNet snet;
            snet.sendRecv(chl10, chl12, src, dest1);
        });

        auto t2 = std::thread([&]() {
            setThreadName("t2");
            OblvSwitchNet snet;
            PRNG prng2(toBlock(44444));
            snet.help(chl20, chl21, prng2, destSize, bytes);
        });

        t0.join();
        t1.join();
        t2.join();


        bool print = false;
        if (print)
            std::cout << std::endl;
        bool failed = false;
        for (u64 i = 0; i < prog.mSrcDests.size(); ++i)
        {
            auto s = prog.mSrcDests[i].mSrc;
            auto d = prog.mSrcDests[i].mDest;

            for (auto j = 0; j < bytes; ++j)
            {
                if ((dest0(d, j) ^ dest1(d, j)) != src(s, j))
                {
                    failed = true;
                }
            }

            if (print)
            {
                std::cout << "d[" << d << "] = ";
                for (auto j = 0; j < bytes; ++j)
                {
                    if ((dest0(d, j) ^ dest1(d, j)) != src(s, j))
                        std::cout << Color::Red;
                    std::cout << ' ' << std::setw(2) << std::hex << int(dest0(i, j) ^ dest1(i, j)) << ColorDefault;
                }
                std::cout << "\ns[" << s << "] = ";
                for (auto j = 0; j < bytes; ++j)
                    std::cout << ' ' << std::setw(2) << std::hex << int(src(s, j));

                std::cout << std::endl << std::dec;
            }


        }

        if (failed)
            throw std::runtime_error("");

    }
}
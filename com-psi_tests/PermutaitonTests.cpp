

#include <cryptoTools/Network/IOService.h>
#include <cryptoTools/Network/Session.h>
#include <cryptoTools/Network/Channel.h>
#include <cryptoTools/Common/Matrix.h>
#include <com-psi/OblvPermutation.h>
#include <cryptoTools/Crypto/PRNG.h>

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
            if (mtx(i,j) != val)
            {
                std::cout << "mtx(" << i << ", " << j << ") != s1(" << perm[i] << ", " << j << ") ^ s2(" << perm[i] << ", " << j << ")"<< std::endl;
                std::cout << "   " << int(mtx(i, j)) << " != " << int(s1(perm[i], j)) << " ^ " << int(s2(perm[i], j)) << std::endl;
                std::cout << "   " << int(mtx(i, j)) << " != " << int(s1(perm[i], j)  ^  s2(perm[i], j)) << std::endl;
                failed = true;
            }
        }
    }
    if(failed)
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
        p.program(chl02, chl01, perm2, prng, s1, OblvPermutation::Additive);
    }//);
     //std::cout << std::endl;

     //auto t1 = std::thread([&]() 
    {
        OblvPermutation p;
        p.send(chl10, chl12, mtx);
    }//);
     //std::cout << std::endl;

    OblvPermutation p;
    p.recv(chl20, chl21, s2, OblvPermutation::Additive);

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
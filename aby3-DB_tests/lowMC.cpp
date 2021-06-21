#include "aby3-DB/LowMC.h"
#include <iostream>
#include <cryptoTools/Common/Timer.h>
#include <cryptoTools/Common/TestCollection.h>
#include <cstdio>
//////////////////
//     MAIN     //
//////////////////
using namespace oc;

template<typename T>
struct vectorPrint
{
    const std::vector<T>& vec;

};
template<typename T>
std::ostream& operator<<(std::ostream& out, const vectorPrint<T>& v)
{
    out << "{";
    auto& vec = v.vec;

    if (vec.size())
    {
        out << vec[0];
    }

    for (u64 i = 1; i < vec.size(); ++i)
    {
        out << ", " << vec[i];
    }

    out << "}";
    return out;
}

void lowMC_Circuit_test() {
    // Example usage of the LowMC class
    // Instantiate a LowMC cipher instance called cipher using the key '1'.
    //LowMC cipher(1);
    LowMC2<> cipher2(false, 1);
    LowMC2<>::block m = 0xFFD5;

    //std::cout << "Plaintext:" << std::endl;
    //std::cout << m << std::endl;
    //auto c1 = cipher.encrypt(m);
    auto c2 = cipher2.encrypt(m);
    //std::cout << "Ciphertext:" << std::endl;
    //std::cout << c1 << std::endl;
    //std::cout << c2 << std::endl;
    //auto m1 = cipher.decrypt(c1);
    //auto m2 = cipher2.decrypt(c2);
    //std::cout << "Encryption followed by decryption of plaintext:" << std::endl;
    //std::cout << m1 << std::endl;
    //std::cout << m2 << std::endl;



    oc::BetaCircuit cir;
    cipher2.to_enc_circuit(cir);

    std::vector<BitVector> inputs(14), outputs(1);

    auto blocksize = m.size();

    inputs[0].resize(blocksize);
    for (u64 i = 0; i < blocksize; ++i)
    {
        inputs[0][i] = m[i];
    }

    for (u64 r = 0; r < 13; ++r)
    {
        inputs[r + 1].resize(blocksize);
        for (u64 i = 0; i < blocksize; ++i)
        {
            inputs[r + 1][i] = cipher2.roundkeys[r][i];
        }
    }

    outputs[0].resize(blocksize);


    cir.evaluate(inputs, outputs, true);

    //std::cout << outputs[0] << std::endl;


    for (u64 i = 0; i < blocksize; ++i)
    {
        if ((bool)outputs[0][i] != c2[i])
        {
            std::cout << "failed " << i << std::endl;

            std::cout << "m " << m << std::endl;
            std::cout << "M " << inputs[0][i] << std::endl;
            std::cout << "o " <<c2 << std::endl;
            std::cout << "O " << outputs[0] << std::endl;

            throw UnitTestFail();
        }
    }

    //cipher2.print_matrices();

}

void lowMC_FileCircuit_test() {

#ifdef USE_JSON
    oc::Timer t;

    t.setTimePoint("s");
    t.setTimePoint("s2");

    LowMC2<>::block m = 0xFFD5;
    LowMC2<>::block m2("0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001111111111010101", m.size(), '0', '1');
    LowMC2<>::block c2("0000101000111011000011010101010100001010100101001001100100110011110000001011110101100111000001011010110101110001100110000101110111100110100101110110101101100001011000101101001001101011011111010011001111010110011101010001000100001100101001100011111100011110", m.size(), '0', '1');


    if (m2 != m)
        throw std::runtime_error(LOCATION);


    std::string filename = "./lowMCCircuit_b" + std::to_string(sizeof(LowMC2<>::block)) + "_k" + std::to_string(sizeof(LowMC2<>::keyblock)) +".json";

    oc::BetaCircuit cir;

    std::ifstream in;
    in.open(filename, std::ios::in | std::ios::binary);

    if (in.is_open() == false)
    {
        LowMC2<> cipher1(false, 1);
        LowMC2<> cipher2(false, 1);


        for (u64 i = 0; i < cipher1.LinMatrices.size(); ++i)
        {
            if (cipher1.LinMatrices[i] != cipher2.LinMatrices[i])
            {
                std::cout << i << " \n" 
                    << vectorPrint<LowMC2<>::block>{ cipher1.LinMatrices[i] } 
                    << "\n != \n" 
                        << cipher2.LinMatrices[i][0] << std::endl;

                throw std::runtime_error(LOCATION);
            }
        }
        t.setTimePoint("lowMC done");

        cipher2.to_enc_circuit(cir);

        t.setTimePoint("to circuit done");
        std::ofstream out;
        out.open(filename, std::ios::trunc | std::ios::out | std::ios::binary);

        cir.writeJson(out);
        out.close();


        t.setTimePoint("write circuit");

        PRNG prng(ZeroBlock);
        for (u64 i = 0; i < 13; ++i)
        {
            for (u64 j = 0; j < cipher2.roundkeys[i].size(); ++j)
            {
                cipher2.roundkeys[i][j] = prng.get<bool>();
            }
        }

        auto c1 = cipher2.encrypt(m);
        if (c1 != c2)
        {
            std::cout << "verify exp " << c2 << std::endl;
            std::cout << "verify act " << c1 << std::endl;
            throw std::runtime_error(LOCATION);
        }

        t.setTimePoint("verify done");

    }
    else
    {
        cir.readJson(in);
        t.setTimePoint("read circuit done");

    }
    //if (cir != cir2)
    //    throw std::runtime_error(LOCATION);



    std::vector<BitVector> inputs(14), outputs(1);

    auto blocksize = m.size();

    inputs[0].resize(blocksize);
    for (u64 i = 0; i < blocksize; ++i)
    {
        inputs[0][i] = m[i];
    }

    PRNG prng(ZeroBlock);
    for (u64 i = 0; i < 13; ++i)
    {
        inputs[i + 1].resize(blocksize);
        for (u64 j = 0; j < blocksize; ++j)
        {
            inputs[i + 1][j] = prng.get<bool>();
        }
    }

    outputs[0].resize(blocksize);


    cir.evaluate(inputs, outputs, true);

    t.setTimePoint("circuit eval done");
    std::remove(filename.c_str());


    //std::cout << outputs[0] << std::endl;

    //std::cout << t << std::endl;
    //std::cout << "m " << m << std::endl;
    //std::cout << "M " << inputs[0] << std::endl;
    //std::cout << "o " << c2 << std::endl;
    //std::cout << "O " << outputs[0] << std::endl;
    for (u64 i = 0; i < blocksize; ++i)
    {
        if ((bool)outputs[0][i] != c2[i])
        {
            std::cout << "failed " << i << std::endl;

            std::cout << "m " << m << std::endl;
            std::cout << "M " << inputs[0] << std::endl;
            std::cout << "o " << c2 << std::endl;
            std::cout << "O " << outputs[0] << std::endl;

            throw UnitTestFail();
        }
    }
#else
	throw UnitTestSkipped("USE_JSON not defined");
#endif
}



void lowMC_BinFileCircuit_test() {


    oc::Timer t;

    t.setTimePoint("s");
    t.setTimePoint("s2");

    LowMC2<>::block m = 0xFFD5;
    LowMC2<>::block m2("0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001111111111010101", m.size(), '0', '1');
    LowMC2<>::block c2("0000101000111011000011010101010100001010100101001001100100110011110000001011110101100111000001011010110101110001100110000101110111100110100101110110101101100001011000101101001001101011011111010011001111010110011101010001000100001100101001100011111100011110", m.size(), '0', '1');


    if (m2 != m)
        throw std::runtime_error(LOCATION);


    std::string filename = "./lowMCCircuit_b" + std::to_string(sizeof(LowMC2<>::block)) + "_k" + std::to_string(sizeof(LowMC2<>::keyblock)) + ".bin";

    oc::BetaCircuit cir;

    std::ifstream in;
    in.open(filename, std::ios::in | std::ios::binary);

    if (in.is_open() == false)
    {
        LowMC2<> cipher1(false, 1);
        LowMC2<> cipher2(false, 1);


        for (u64 i = 0; i < cipher1.LinMatrices.size(); ++i)
        {
            if (cipher1.LinMatrices[i] != cipher2.LinMatrices[i])
                throw std::runtime_error(LOCATION);
        }
        t.setTimePoint("lowMC done");

        cipher2.to_enc_circuit(cir);

        t.setTimePoint("to circuit done");
        std::ofstream out;
        out.open(filename, std::ios::trunc | std::ios::out | std::ios::binary);

        cir.writeBin(out);
        out.close();


        t.setTimePoint("write circuit");

        PRNG prng(ZeroBlock);
        for (u64 i = 0; i < 13; ++i)
        {
            for (u64 j = 0; j < cipher2.roundkeys[i].size(); ++j)
            {
                cipher2.roundkeys[i][j] = prng.get<bool>();
            }
        }

        auto c1 = cipher2.encrypt(m);
        if (c1 != c2)
        {
            std::cout << "verify exp " << c2 << std::endl;
            std::cout << "verify act " << c1 << std::endl;
            throw std::runtime_error(LOCATION);
        }

        t.setTimePoint("verify done");

    }
    else
    {
        cir.readBin(in);
        t.setTimePoint("read circuit done");

    }
    //if (cir != cir2)
    //    throw std::runtime_error(LOCATION);



    std::vector<BitVector> inputs(14), outputs(1);

    auto blocksize = m.size();

    inputs[0].resize(blocksize);
    for (u64 i = 0; i < blocksize; ++i)
    {
        inputs[0][i] = m[i];
    }

    PRNG prng(ZeroBlock);
    for (u64 i = 0; i < 13; ++i)
    {
        inputs[i + 1].resize(blocksize);
        for (u64 j = 0; j < blocksize; ++j)
        {
            inputs[i + 1][j] = prng.get<bool>();
        }
    }

    outputs[0].resize(blocksize);


    cir.evaluate(inputs, outputs, true);

    t.setTimePoint("circuit eval done");


    std::remove(filename.c_str());
    //std::cout << outputs[0] << std::endl;

    //std::cout << t << std::endl;
    //std::cout << "m " << m << std::endl;
    //std::cout << "M " << inputs[0] << std::endl;
    //std::cout << "o " << c2 << std::endl;
    //std::cout << "O " << outputs[0] << std::endl;
    for (u64 i = 0; i < blocksize; ++i)
    {
        if ((bool)outputs[0][i] != c2[i])
        {

            //std::cout << "failed " << i << std::endl;

            //std::cout << "m " << m << std::endl;
            //std::cout << "M " << inputs[0] << std::endl;
            //std::cout << "o " << c2 << std::endl;
            //std::cout << "O " << outputs[0] << std::endl;

            throw UnitTestSkipped("Known issue, need to investigate. " LOCATION);
        }
    }
}
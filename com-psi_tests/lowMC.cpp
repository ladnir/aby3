#include "com-psi/LowMC.h"
#include <iostream>
#include <cryptoTools/Common/Timer.h>
#include <cryptoTools/Common/TestCollection.h>
//////////////////
//     MAIN     //
//////////////////
using namespace oc;

void lowMC_Circuit_test() {
    // Example usage of the LowMC class
    // Instantiate a LowMC cipher instance called cipher using the key '1'.
    //LowMC cipher(1);
    LowMC2<> cipher2(1);
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
        if (outputs[0][i] != c2[i])
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


    oc::Timer t;

    t.setTimePoint("s");
    t.setTimePoint("s2");

    LowMC2<>::block m = 0xFFD5;
    LowMC2<>::block m2("0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001111111111010101", m.size(), '0', '1');
    LowMC2<>::block c2("0101110110000000101011001100110001000111001000100011001000011101001111010001101001001111000110100101001001111100110001101000011010001101011111000000100110110000101111100010001011111010000000110101010011011111001011000010100110001000100111011111001010000011", m.size(), '0', '1');


    if (m2 != m)
        throw std::runtime_error(LOCATION);


    std::string filename = "./lowMCCircuit_b" + ToString(sizeof(LowMC2<>::block)) + "_k" + ToString(sizeof(LowMC2<>::keyblock)) +".json";

    oc::BetaCircuit cir;

    std::ifstream in;
    in.open(filename, std::ios::in | std::ios::binary);

    if (in.is_open() == false)
    {
        LowMC2<> cipher1(1);
        LowMC2<> cipher2(1);


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


    //std::cout << outputs[0] << std::endl;

    //std::cout << t << std::endl;
    //std::cout << "m " << m << std::endl;
    //std::cout << "M " << inputs[0] << std::endl;
    //std::cout << "o " << c2 << std::endl;
    //std::cout << "O " << outputs[0] << std::endl;
    for (u64 i = 0; i < blocksize; ++i)
    {
        if (outputs[0][i] != c2[i])
        {
            std::cout << "failed " << i << std::endl;

            std::cout << "m " << m << std::endl;
            std::cout << "M " << inputs[0] << std::endl;
            std::cout << "o " << c2 << std::endl;
            std::cout << "O " << outputs[0] << std::endl;

            throw UnitTestFail();
        }
    }
}



void lowMC_BinFileCircuit_test() {


    oc::Timer t;

    t.setTimePoint("s");
    t.setTimePoint("s2");

    LowMC2<>::block m = 0xFFD5;
    LowMC2<>::block m2("0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001111111111010101", m.size(), '0', '1');
    LowMC2<>::block c2("0101110110000000101011001100110001000111001000100011001000011101001111010001101001001111000110100101001001111100110001101000011010001101011111000000100110110000101111100010001011111010000000110101010011011111001011000010100110001000100111011111001010000011", m.size(), '0', '1');


    if (m2 != m)
        throw std::runtime_error(LOCATION);


    std::string filename = "./lowMCCircuit_b" + ToString(sizeof(LowMC2<>::block)) + "_k" + ToString(sizeof(LowMC2<>::keyblock)) + ".bin";

    oc::BetaCircuit cir;

    std::ifstream in;
    in.open(filename, std::ios::in | std::ios::binary);

    if (in.is_open() == false)
    {
        LowMC2<> cipher1(1);
        LowMC2<> cipher2(1);


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


    //std::cout << outputs[0] << std::endl;

    //std::cout << t << std::endl;
    //std::cout << "m " << m << std::endl;
    //std::cout << "M " << inputs[0] << std::endl;
    //std::cout << "o " << c2 << std::endl;
    //std::cout << "O " << outputs[0] << std::endl;
    for (u64 i = 0; i < blocksize; ++i)
    {
        if (outputs[0][i] != c2[i])
        {
            std::cout << "failed " << i << std::endl;

            std::cout << "m " << m << std::endl;
            std::cout << "M " << inputs[0] << std::endl;
            std::cout << "o " << c2 << std::endl;
            std::cout << "O " << outputs[0] << std::endl;

            throw UnitTestFail();
        }
    }
}
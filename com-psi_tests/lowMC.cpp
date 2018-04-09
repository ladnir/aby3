#include "com-psi/LowMC.h"
#include <iostream>

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
        if (outputs[0][i] != m[i])
        {
            std::cout << "failed " << i << std::endl;
            throw UnitTestFail();
        }
    }

    //cipher2.print_matrices();

}

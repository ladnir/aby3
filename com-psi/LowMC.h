#pragma once

#include <bitset>
#include <vector>
#include <array>
#include <string>
#include <random>

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <algorithm>


#include <aby3/Circuit/BetaCircuit.h>


namespace osuCrypto
{
    template<typename block_type>
    unsigned rank_of_Matrix(const std::vector<block_type>& matrix);

    template<typename block_type>
    std::vector<block_type> invert_Matrix(const std::vector<block_type> matrix);

    template<
        size_t  numofboxes = 49,
        size_t  blocksize = 256,
        size_t  keysize = 80,
        size_t  rounds = 12
    >
        class LowMC2 {
        public:


            size_t getBlockSize() { return blocksize; };

            using block = std::bitset<blocksize>; // Store messages and states
            using keyblock = std::bitset<keysize>;

            // Size of the identity part in the Sbox layer
            const unsigned identitysize = blocksize - 3 * numofboxes;

            LowMC2(keyblock k = 0) {
                key = k;
                instantiate_LowMC();
                keyschedule();
            };

            block encrypt(const block& message)
            {
                //std::cout << "message        " << message << std::endl;
                //std::cout << "key 0          " << roundkeys[0] << std::endl;

                block c = message ^ roundkeys[0];

                //std::cout << "state[0]       " << c << std::endl;

                for (unsigned r = 1; r <= rounds; ++r) {
                    c = Substitution(c);

                    //std::cout << "state[" << r << "].sbox  " << c << std::endl;

                    c = MultiplyWithGF2Matrix(LinMatrices[r - 1], c);

                    //std::cout << "state[" << r << "].mul   " << c << std::endl;
                    c ^= roundconstants[r - 1];

                    //std::cout << "state[" << r << "].const " << c << std::endl;

                    c ^= roundkeys[r];

                    //std::cout << "state[" << r << "]       " << c << std::endl;
                }
                return c;
            }


            block decrypt(const block message)
            {
                block c = message;
                for (unsigned r = rounds; r > 0; --r) {
                    c ^= roundkeys[r];
                    c ^= roundconstants[r - 1];
                    c = MultiplyWithGF2Matrix(invLinMatrices[r - 1], c);
                    c = invSubstitution(c);
                }
                c ^= roundkeys[0];
                return c;
            }

            void set_key(keyblock k)
            {
                key = k;
                keyschedule();
            }


            void to_enc_circuit(BetaCircuit& cir)
            {
                BetaBundle message(blocksize), state(blocksize), temp(blocksize), temp2(1);
                std::array<BetaBundle, rounds + 1> roundKeys;

                cir.addInputBundle(message);
                for (auto& key : roundKeys)
                {
                    key.mWires.resize(blocksize);
                    cir.addInputBundle(key);
                }

                cir.addOutputBundle(state);

                cir.addTempWireBundle(temp);
                cir.addTempWireBundle(temp2);

                for (int i = 0; i < blocksize; ++i)
                {
                    cir.addGate(message[i], roundKeys[0][i], GateType::Xor, state[i]);
                }

                //cir.addPrint("message        ");
                //cir.addPrint(message);
                //cir.addPrint("\n");

                //cir.addPrint("key 0          ");
                //cir.addPrint(roundKeys[0]);
                //cir.addPrint("\n");

                //cir.addPrint("state[0]       ");
                //cir.addPrint(state);
                //cir.addPrint("\n");


                for (unsigned r = 0; r < rounds; ++r)
                {
                    // SBOX
                    for (int i = 0; i < numofboxes; ++i)
                    {
                        auto& c = state[i * 3 + 0];
                        auto& b = state[i * 3 + 1];
                        auto& a = state[i * 3 + 2];
                        auto& aa = temp[i * 3 + 0];
                        auto& bb = temp[i * 3 + 1];
                        auto& cc = temp[i * 3 + 2];
                        auto& a_b = temp2[0];

                        // a_b = a + b
                        cir.addGate(a, b, GateType::Xor, a_b);

                        cir.addGate(b, c, GateType::And, aa);

                        cir.addGate(a, c, GateType::And, bb);

                        cir.addGate(a, b, GateType::And, cc);
                        cir.addGate(cc, c, GateType::Xor, cc);

                        // a = a + bc
                        cir.addGate(a, aa, GateType::Xor, a);
                        // b = a + b + ac
                        cir.addGate(a_b, bb, GateType::Xor, b);
                        // c = a + b + c + ab
                        cir.addGate(cc, a_b, GateType::Xor, c);
                    }
                    //cir.addPrint("state[" + ToString(r) + "].sbox  ");
                    //cir.addPrint(state);
                    //cir.addPrint("\n");


                    // multiply with linear matrix and add const
                    auto& matrix = LinMatrices[r];
                    for (int i = 0; i < blocksize; ++i)
                    {
                        auto& t = temp[i];
                        int j = -1, firstIdx, secondIdx;

                        auto& row = matrix[i];

                        while (row[++j] == 0);
                        firstIdx = j;

                        while (row[++j] == 0);
                        secondIdx = j++;

                        cir.addGate(state[firstIdx], state[secondIdx], GateType::Xor, t);

                        for (; j < blocksize; ++j)
                        {
                            if (row[j])
                            {
                                cir.addGate(t, state[j], GateType::Xor, t);
                            }
                        }
                    }
                    //cir.addPrint("state[" + ToString(r) + "].mul   ");
                    //cir.addPrint(temp);
                    //cir.addPrint("\n");

                    for (int i = 0; i < blocksize; ++i)
                    {

                        if (roundconstants[r][i])
                        {
                            cir.addInvert(temp[i]);
                        }
                    }
                    //cir.addPrint("state[" + ToString(r) + "].const ");
                    //cir.addPrint(temp);
                    //cir.addPrint("\n");

                    // add key
                    for (int i = 0; i < blocksize; ++i)
                    {
                        cir.addGate(temp[i], roundKeys[r + 1][i], GateType::Xor, state[i]);
                    }


                    //cir.addPrint("state[" + ToString(r) + "].      ");
                    //cir.addPrint(state);
                    //cir.addPrint("\n");
                }
            }

            void print_matrices()
            {
                std::cout << "LowMC2 matrices and constants" << std::endl;
                std::cout << "============================" << std::endl;
                std::cout << "Block size: " << blocksize << std::endl;
                std::cout << "Key size: " << keysize << std::endl;
                std::cout << "Rounds: " << rounds << std::endl;
                std::cout << std::endl;

                std::cout << "Linear layer matrices" << std::endl;
                std::cout << "---------------------" << std::endl;
                for (unsigned r = 1; r <= rounds; ++r) {
                    std::cout << "Linear layer " << r << ":" << std::endl;
                    for (auto row : LinMatrices[r - 1]) {
                        std::cout << "[";
                        for (unsigned i = 0; i < blocksize; ++i) {
                            std::cout << row[i];
                            if (i != blocksize - 1) {
                                std::cout << ", ";
                            }
                        }
                        std::cout << "]" << std::endl;
                    }
                    std::cout << std::endl;
                }

                std::cout << "Round constants" << std::endl;
                std::cout << "---------------------" << std::endl;
                for (unsigned r = 1; r <= rounds; ++r) {
                    std::cout << "Round constant " << r << ":" << std::endl;
                    std::cout << "[";
                    for (unsigned i = 0; i < blocksize; ++i) {
                        std::cout << roundconstants[r - 1][i];
                        if (i != blocksize - 1) {
                            std::cout << ", ";
                        }
                    }
                    std::cout << "]" << std::endl;
                    std::cout << std::endl;
                }

                std::cout << "Round key matrices" << std::endl;
                std::cout << "---------------------" << std::endl;
                for (unsigned r = 0; r <= rounds; ++r) {
                    std::cout << "Round key matrix " << r << ":" << std::endl;
                    for (auto row : KeyMatrices[r]) {
                        std::cout << "[";
                        for (unsigned i = 0; i < keysize; ++i) {
                            std::cout << row[i];
                            if (i != keysize - 1) {
                                std::cout << ", ";
                            }
                        }
                        std::cout << "]" << std::endl;
                    }
                    if (r != rounds) {
                        std::cout << std::endl;
                    }
                }
            }

            //private:
                // LowMC2 private data members //

                // The Sbox and its inverse    
            const std::array<unsigned, 8> Sbox =
            { 0x00, 0x01, 0x03, 0x06, 0x07, 0x04, 0x05, 0x02 };
            const std::array<unsigned, 8> invSbox =
            { 0x00, 0x01, 0x07, 0x02, 0x05, 0x06, 0x03, 0x04 };

            // Stores the binary matrices for each round
            std::vector<std::vector<block>> LinMatrices;
            // Stores the inverses of LinMatrices
            std::vector<std::vector<block>> invLinMatrices;
            // Stores the round constants
            std::vector<block> roundconstants;
            //Stores the master key
            keyblock key = 0;
            // Stores the matrices that generate the round keys
            std::vector<std::vector<keyblock>> KeyMatrices;
            // Stores the round keys
            std::vector<block> roundkeys;

            // LowMC2 private functions //

            // The substitution layer
            block Substitution(const block message)
            {
                block temp = 0;
                //Get the identity part of the message
                temp ^= (message >> 3 * numofboxes);
                //Get the rest through the Sboxes
                for (unsigned i = 1; i <= numofboxes; ++i) {
                    temp <<= 3;
                    temp ^= Sbox[((message >> 3 * (numofboxes - i))
                        & block(0x7)).to_ulong()];
                }
                return temp;
            }

            // The inverse substitution layer
            block invSubstitution(const block message)
            {
                block temp = 0;
                //Get the identity part of the message
                temp ^= (message >> 3 * numofboxes);
                //Get the rest through the invSboxes
                for (unsigned i = 1; i <= numofboxes; ++i) {
                    temp <<= 3;
                    temp ^= invSbox[((message >> 3 * (numofboxes - i))
                        & block(0x7)).to_ulong()];
                }
                return temp;
            }

            // For the linear layer
            template<typename block_type>
            block MultiplyWithGF2Matrix(const std::vector<block_type> matrix, const block_type message)
            {
                block temp = 0;
                for (unsigned i = 0; i < blocksize; ++i) {
                    temp[i] = (message & matrix[i]).count() & 1;
                }
                return temp;
            }

            //Creates the round keys from the master key
            void keyschedule()
            {
                roundkeys.clear();
                for (unsigned r = 0; r <= rounds; ++r) {
                    roundkeys.push_back(MultiplyWithGF2Matrix(KeyMatrices[r], key));
                }
                return;
            }


            template<typename block_type>
            std::vector<block_type> loadMatrix(std::istream& in)
            {
                std::vector<block_type> mat(blocksize);
                char c;

                for (int i = 0; i < mat.size(); ++i)
                {
                    for (int j = 0; j < mat[i].size(); ++j)
                    {
                        in.read(&c, 1);
                        if (c == '0')
                            mat[i][j] = 0;
                        else if (c == '1')
                            mat[i][j] = 1;
                        else
                            throw std::runtime_error(LOCATION);
                    }

                    in.read(&c, 1);
                    if(c != '\n')
                        throw std::runtime_error(LOCATION);
                }
                return mat;
            }

            template<typename block_type>
            void writeMatrix(std::ostream& out, const std::vector<block_type>& mat)
            {
                char c;
                for (int i = 0; i < mat.size(); ++i)
                {
                    for (int j = 0; j < mat[i].size(); ++j)
                    {
                        c = '0' + mat[i][j];
                        out.write(&c, 1);
                    }

                    c = '\n';
                    out.write(&c, 1);
                }
            }



            //Fills the matrices and roundconstants with pseudorandom bits 
            void instantiate_LowMC()
            {

                // Create LinMatrices and invLinMatrices
                LinMatrices.clear();
                invLinMatrices.clear();
                for (unsigned r = 0; r < rounds; ++r) {

                    std::vector<block> mat;

                    std::string fileName("./linMtx_" + ToString(r) + ".txt");
                    std::ifstream in;
                    in.open(fileName, std::ios::in);

                    if (in.is_open() == false)
                    {

                        // Create matrix
                        // Fill matrix with random bits
                        do {
                            mat.clear();
                            for (unsigned i = 0; i < blocksize; ++i) {
                                mat.push_back(getrandblock());
                            }
                            // Repeat if matrix is not invertible
                        } while (rank_of_Matrix(mat) != blocksize);

                        //std::ofstream out;
                        //out.open(fileName, std::ios::out | std::ios::binary | std::ios::trunc);
                        //writeMatrix(out, mat);
                    }
                    else
                    {
                        mat = loadMatrix<block>(in);

                        if (rank_of_Matrix(mat) != blocksize)
                            throw std::runtime_error(LOCATION);
                    }

                    LinMatrices.push_back(mat);
                    invLinMatrices.push_back(invert_Matrix(LinMatrices.back()));
                }



                // Create roundconstants
                roundconstants.clear();
                for (unsigned r = 0; r < rounds; ++r) {
                    roundconstants.push_back(getrandblock());
                }

                // Create KeyMatrices
                KeyMatrices.clear();
                for (unsigned r = 0; r <= rounds; ++r) {
                    // Create matrix
                    std::vector<keyblock> mat;


                    std::string fileName("./keyMtx_" + ToString(r) + ".txt");
                    std::ifstream in;
                    in.open(fileName, std::ios::in);

                    if (in.is_open() == false)
                    {

                        // Fill matrix with random bits
                        do {
                            mat.clear();
                            for (unsigned i = 0; i < blocksize; ++i) {
                                mat.push_back(getrandkeyblock());
                            }
                            // Repeat if matrix is not of maximal rank
                        } while (rank_of_Matrix(mat) < std::min(blocksize, keysize));

                        //std::ofstream out;
                        //out.open(fileName, std::ios::out | std::ios::binary | std::ios::trunc);
                        //writeMatrix(out, mat);
                    }
                    else
                    {
                        mat = loadMatrix<keyblock>(in);

                        if (rank_of_Matrix(mat) < std::min(blocksize, keysize))
                            throw std::runtime_error(LOCATION);
                    }

                    KeyMatrices.push_back(mat);
                }

                return;
            }


            // Random bits functions //
            block getrandblock()
            {
                block tmp = 0;
                for (unsigned i = 0; i < blocksize; ++i) tmp[i] = getrandbit();
                return tmp;
            }


            keyblock getrandkeyblock()
            {
                keyblock tmp = 0;
                for (unsigned i = 0; i < keysize; ++i) tmp[i] = getrandbit();
                return tmp;
            }

            bool  getrandbit()
            {
                //static std::mt19937 gen(0); //Standard mersenne_twister_engine seeded with zero
                //std::uniform_int_distribution<> dis(0, 1);
                //return dis(gen);

                static std::bitset<80> state; //Keeps the 80 bit LSFR state
                bool tmp = 0;
                //If state has not been initialized yet
                if (state.none()) {
                    state.set(); //Initialize with all bits set
                                 //Throw the first 160 bits away
                    for (unsigned i = 0; i < 160; ++i) {
                        //Update the state
                        tmp = state[0] ^ state[13] ^ state[23]
                            ^ state[38] ^ state[51] ^ state[62];
                        state >>= 1;
                        state[79] = tmp;
                    }
                }
                //choice records whether the first bit is 1 or 0.
                //The second bit is produced if the first bit is 1.
                bool choice = false;
                do {
                    //Update the state
                    tmp = state[0] ^ state[13] ^ state[23]
                        ^ state[38] ^ state[51] ^ state[62];
                    state >>= 1;
                    state[79] = tmp;
                    choice = tmp;
                    tmp = state[0] ^ state[13] ^ state[23]
                        ^ state[38] ^ state[51] ^ state[62];
                    state >>= 1;
                    state[79] = tmp;
                } while (!choice);
                return tmp;
            }

    };


    // Binary matrix functions //   

    template<typename block_type>
    std::vector<block_type> invert_Matrix(const std::vector<block_type> matrix)
    {
        std::vector<block_type> mat = matrix;
        auto blocksize = mat[0].size();

        std::vector<block_type> invmat(blocksize, 0); //To hold the inverted matrix
        for (unsigned i = 0; i < blocksize; ++i) {
            invmat[i][i] = 1;
        }

        unsigned size = mat[0].size();
        //Transform to upper triangular matrix
        unsigned row = 0;
        for (unsigned col = 0; col < size; ++col) {
            if (!mat[row][col]) {
                unsigned r = row + 1;
                while (r < mat.size() && !mat[r][col]) {
                    ++r;
                }
                if (r >= mat.size()) {
                    continue;
                }
                else {
                    auto temp = mat[row];
                    mat[row] = mat[r];
                    mat[r] = temp;
                    temp = invmat[row];
                    invmat[row] = invmat[r];
                    invmat[r] = temp;
                }
            }
            for (unsigned i = row + 1; i < mat.size(); ++i) {
                if (mat[i][col]) {
                    mat[i] ^= mat[row];
                    invmat[i] ^= invmat[row];
                }
            }
            ++row;
        }

        //Transform to identity matrix
        for (unsigned col = size; col > 0; --col) {
            for (unsigned r = 0; r < col - 1; ++r) {
                if (mat[r][col - 1]) {
                    mat[r] ^= mat[col - 1];
                    invmat[r] ^= invmat[col - 1];
                }
            }
        }

        return invmat;
    }

    template<typename block_type>
    unsigned rank_of_Matrix(const std::vector<block_type>& matrix)
    {
        std::vector<block_type> mat = matrix;

        unsigned size = mat[0].size();
        //Transform to upper triangular matrix
        unsigned row = 0;
        for (unsigned col = 1; col <= size; ++col) {
            if (!mat[row][size - col]) {
                unsigned r = row;
                while (r < mat.size() && !mat[r][size - col]) {
                    ++r;
                }
                if (r >= mat.size()) {
                    continue;
                }
                else {
                    auto temp = mat[row];
                    mat[row] = mat[r];
                    mat[r] = temp;
                }
            }
            for (unsigned i = row + 1; i < mat.size(); ++i) {
                if (mat[i][size - col]) mat[i] ^= mat[row];
            }
            ++row;
            if (row == size) break;
        }
        return row;
    }

}
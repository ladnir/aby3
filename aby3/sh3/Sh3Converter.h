#pragma once

#include "Sh3Types.h"
#include "Sh3Runtime.h"
#include "aby3/Circuit/CircuitLibrary.h"
namespace aby3
{


    class Sh3Converter
    {
    public:
        CircuitLibrary mLib;

        void toPackedBin(const sbMatrix& in, sPackedBin& dest);

        void toBinaryMatrix(const sPackedBin& in, sbMatrix& dest);

        Sh3Task toPackedBin(Sh3Task dep, const si64Matrix& in, sPackedBin& dest);
        Sh3Task toBinaryMatrix(Sh3Task dep, const si64Matrix& in, sbMatrix& dest);

        Sh3Task toSi64Matrix(Sh3Task dep, const sbMatrix& in, si64Matrix& dest);
        Sh3Task toSi64Matrix(Sh3Task dep, const sPackedBin& in, si64Matrix& dest);
    };
}
#pragma once

#include "Sh3Types.h"

namespace aby3
{


    class Sh3Converter
    {
    public:
        void toPackedBin(const sbMatrix& in, sPackedBin& dest);

        void toBinaryMatrix(const sPackedBin& in, sbMatrix& dest);
    };
}
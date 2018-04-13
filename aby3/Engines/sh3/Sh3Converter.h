#pragma once

#include "Sh3Defines.h"

namespace aby3
{


    class Sh3Converter
    {
    public:
        void toPackedBin(const Sh3::sbMatrix& in, Sh3::sPackedBin& dest);

        void toBinaryMatrix(const Sh3::sPackedBin& in, Sh3::sbMatrix& dest);
    };
}
#pragma once
#include "aby3/Common/Defines.h"
#include <cryptoTools/Network/Session.h>

namespace aby3
{


    class Sh3Runtime
    {

        void init(u64 partyIdx, oc::Session& prev, oc::Session& next);


    };

}
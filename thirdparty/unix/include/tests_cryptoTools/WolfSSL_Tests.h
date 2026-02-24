#pragma once
#include <cryptoTools/Common/CLP.h>


void wolfSSL_echoServer_test(const osuCrypto::CLP& cmd);
void wolfSSL_mutualAuth_test(const osuCrypto::CLP& cmd); 
void wolfSSL_channel_test(const osuCrypto::CLP& cmd);
void wolfSSL_CancelChannel_Test();
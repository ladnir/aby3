#pragma once

#include <cryptoTools/Common/config.h>

#include <cryptoTools/Common/CLP.h>
#include <cryptoTools/Common/TestCollection.h>

namespace tests_cryptoTools
{

#ifdef ENABLE_BOOST

    void BtNetwork_Connect1_Test(const osuCrypto::CLP& cmd);
    void BtNetwork_BadConnect_Test(const osuCrypto::CLP& cmd);
    void BtNetwork_PartialConnect_Test(const osuCrypto::CLP& cmd);
    
    void BtNetwork_shutdown_test(const osuCrypto::CLP& cmd);


    void BtNetwork_RapidConnect_Test(const osuCrypto::CLP& cmd);
    void BtNetwork_OneMegabyteSend_Test(const osuCrypto::CLP& cmd);
    void BtNetwork_ConnectMany_Test(const osuCrypto::CLP& cmd);
    void BtNetwork_CrossConnect_Test(const osuCrypto::CLP& cmd);
    void BtNetwork_ManySessions_Test(const osuCrypto::CLP& cmd);


    void BtNetwork_AsyncConnect_Test(const osuCrypto::CLP& cmd);
    void BtNetwork_std_Containers_Test(const osuCrypto::CLP& cmd);
    void BtNetwork_bitVector_Test(const osuCrypto::CLP& cmd);


    void BtNetwork_recvErrorHandler_Test(const osuCrypto::CLP& cmd);
    void BtNetwork_closeOnError_Test(const osuCrypto::CLP& cmd);
    void BtNetwork_clientClose_Test(const osuCrypto::CLP& cmd);


	void BtNetwork_SocketInterface_Test(const osuCrypto::CLP& cmd);

	void BtNetwork_AnonymousMode_Test(const osuCrypto::CLP& cmd);
	void BtNetwork_ServerMode_Test(const osuCrypto::CLP& cmd);
	void BtNetwork_CancelChannel_Test(const osuCrypto::CLP& cmd);


    void BtNetwork_oneWorker_Test(const osuCrypto::CLP& cmd);
    void BtNetwork_useAfterCancel_test(const osuCrypto::CLP& cmd);
    void BtNetwork_fastCancel(const osuCrypto::CLP& cmd);

    void BtNetwork_socketAdapter_test(const osuCrypto::CLP& cmd);
    void BtNetwork_BasicSocket_test(const osuCrypto::CLP&cmd);

    void SBO_ptr_test();
    void BtNetwork_queue_Test(const osuCrypto::CLP& cmd);
#else
    inline void np() { throw oc::UnitTestSkipped("ENABLE_BOOST not defined."); }
    inline void BtNetwork_Connect1_Test(const osuCrypto::CLP& cmd) { np(); }
    inline void BtNetwork_BadConnect_Test(const osuCrypto::CLP& cmd) { np(); }
    inline void BtNetwork_PartialConnect_Test(const osuCrypto::CLP& cmd) { np(); }
    inline void BtNetwork_shutdown_test(const osuCrypto::CLP& cmd) { np(); }
    inline void BtNetwork_RapidConnect_Test(const osuCrypto::CLP& cmd) { np(); }
    inline void BtNetwork_OneMegabyteSend_Test(const osuCrypto::CLP& cmd) { np(); }
    inline void BtNetwork_ConnectMany_Test(const osuCrypto::CLP& cmd) { np(); }
    inline void BtNetwork_CrossConnect_Test(const osuCrypto::CLP& cmd) { np(); }
    inline void BtNetwork_ManySessions_Test(const osuCrypto::CLP& cmd) { np(); }
    inline void BtNetwork_AsyncConnect_Test(const osuCrypto::CLP& cmd) { np(); }
    inline void BtNetwork_std_Containers_Test(const osuCrypto::CLP& cmd) { np(); }
    inline void BtNetwork_bitVector_Test(const osuCrypto::CLP& cmd) { np(); }
    inline void BtNetwork_recvErrorHandler_Test(const osuCrypto::CLP& cmd) { np(); }
    inline void BtNetwork_closeOnError_Test(const osuCrypto::CLP& cmd) { np(); }
    inline void BtNetwork_clientClose_Test(const osuCrypto::CLP& cmd) { np(); }
    inline void BtNetwork_SocketInterface_Test(const osuCrypto::CLP& cmd) { np(); }
    inline void BtNetwork_AnonymousMode_Test(const osuCrypto::CLP& cmd) { np(); }
    inline void BtNetwork_ServerMode_Test(const osuCrypto::CLP& cmd) { np(); }
    inline void BtNetwork_CancelChannel_Test(const osuCrypto::CLP& cmd) { np(); }
    inline void BtNetwork_oneWorker_Test(const osuCrypto::CLP& cmd) { np(); }
    inline void BtNetwork_useAfterCancel_test(const osuCrypto::CLP& cmd) { np(); }
    inline void BtNetwork_fastCancel(const osuCrypto::CLP& cmd) { np(); }
    inline void SBO_ptr_test() { np(); }
    inline void BtNetwork_queue_Test(const osuCrypto::CLP& cmd) { np(); }
    inline void BtNetwork_socketAdapter_test(const osuCrypto::CLP& cmd) { np(); }
    inline void BtNetwork_BasicSocket_test(const osuCrypto::CLP& cmd) { np(); };
    
#endif
}
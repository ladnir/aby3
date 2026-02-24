#pragma once
// This file and the associated implementation has been placed in the public domain, waiving all copyright. No restrictions are placed on its use.  
#include <cryptoTools/Common/config.h>
#ifdef ENABLE_BOOST

#include "cryptoTools/Common/Defines.h"
#include <cryptoTools/Network/Channel.h>

#include <string>
#include <list>
#include <mutex>
#include <memory>
#include "TLS.h"
#include "util.h"

namespace osuCrypto {

	class IOService;
	class Acceptor;
	namespace details { struct SessionGroup; }
    class ChannelBase;
	struct SessionBase;

	typedef SessionMode EpMode;

    class Session
    {
    public:

		// Start a session for the given IP and port in either Client or Server mode.
		// The server should use their local address on which the socket should bind.
		// The client should use the address of the server.
		// The same name should be used by both sessions. Multiple Sessions can be bound to the same
		// address if the same IOService is used but with different name.
        void start(IOService& ioService, std::string remoteIp, u32 port, SessionMode type, std::string name = "");

		// Start a session for the given address in either Client or Server mode.
		// The server should use their local address on which the socket should bind.
		// The client should use the address of the server.
		// The same name should be used by both sessions. Multiple Sessions can be bound to the same
		// address if the same IOService is used but with different name.
        void start(IOService& ioService, std::string address, SessionMode type, std::string name = "");

        void start(IOService& ioService, std::string ip, u64 port, SessionMode type, TLSContext& tls, std::string name = "");


		// See start(...)
		Session(IOService & ioService, std::string address, SessionMode type, std::string name = "");

		// See start(...)
        Session(IOService& ioService, std::string remoteIP, u32 port, SessionMode type, std::string name = "");

        Session(IOService & ioService, std::string remoteIP, u32 port, SessionMode type, TLSContext& tls, std::string name = "");

		// Default constructor
		Session();

		Session(const Session&);
		Session(Session&&) = default;

		Session(const std::shared_ptr<SessionBase>& c);

        ~Session();

        std::string getName() const;

		u64 getSessionID() const;

		IOService& getIOService();

        // Adds a new channel (data pipe) between this endpoint and the remote. The channel is named at each end.
        Channel addChannel(std::string localName = "", std::string remoteName = "");

        // Stops this Session.
		void stop(/*const std::optional<std::chrono::milliseconds>& waitTime = {}*/);

        // returns whether the endpoint has been stopped (or never isConnected).
        bool stopped() const;

		u32 port() const;

		std::string IP() const;

		bool isHost() const;

		std::shared_ptr<SessionBase> mBase;
    };

	typedef Session Endpoint;
	class IOService;
	
	struct SessionBase
	{
		SessionBase(IOService& ios);
		~SessionBase();

		void stop();
		//  Removes the channel with chlName. 
		//void removeChannel(ChannelBase* chl);

		// if this channnel is waiting on a socket, cancel that 
		// operation and set the future to contain an exception
		//void cancelPendingConnection(ChannelBase* chl);

		std::string mIP;
		u32 mPort = 0, mAnonymousChannelIdx = 0;
		SessionMode mMode = SessionMode::Client;
		bool mStopped = true;
		IOService* mIOService = nullptr;
		Acceptor* mAcceptor = nullptr;


		std::atomic<u32> mRealRefCount;

		Work mWorker;

		//bool mHasGroup = false;
		std::shared_ptr<details::SessionGroup> mGroup;

        TLSContext mTLSContext;

		std::mutex mAddChannelMtx;
		std::string mName;

		u64 mSessionID = 0;

#ifdef ENABLE_WOLFSSL
		std::mutex mTLSSessionIDMtx;
		bool mTLSSessionIDIsSet = false;
		block mTLSSessionID;
#endif

		boost::asio::ip::tcp::endpoint mRemoteAddr;
	};


}
#endif
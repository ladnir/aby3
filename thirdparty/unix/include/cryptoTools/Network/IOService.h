#pragma once
// This file and the associated implementation has been placed in the public domain, waiving all copyright. No restrictions are placed on its use. 
#include <cryptoTools/Common/config.h>
#ifdef ENABLE_BOOST

#include <cryptoTools/Common/Defines.h>
#include <cryptoTools/Network/SocketAdapter.h>
#include <cryptoTools/Network/Session.h>
#include <cryptoTools/Network/IoBuffer.h>
#include <cryptoTools/Common/Log.h>
#include <unordered_set>

# if defined(_WINSOCKAPI_) && !defined(_WINSOCK2API_)
#  error WinSock.h has already been included. Please move the boost headers above the WinNet*.h headers
# endif // defined(_WINSOCKAPI_) && !defined(_WINSOCK2API_)

#ifndef _MSC_VER
#pragma GCC diagnostic push 
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#include <boost/asio.hpp>
#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif

#include <thread> 
#include <mutex>
#include <list> 
#include <future>
#include <string>
#include <unordered_map>
#include <functional>


namespace osuCrypto
{

	class Acceptor;
	class IOOperation;

    std::vector<std::string> split(const std::string &s, char delim);

    class IOService
    {
        friend class Channel;
        friend class Session;
    public:
#ifdef ENABLE_NET_LOG
        Log mLog;
        std::mutex mWorkerMtx;
        std::unordered_map<void*, std::string> mWorkerLog;
#endif
        //std::unordered_set<ChannelBase*> mChannels;

        block mRandSeed;
        std::atomic<u64> mSeedIndex;

        // returns a unformly random block.
        block getRandom();

        IOService(const IOService&) = delete;

        // Constructor for the IO service that services network IO operations.
        // threadCount is The number of threads that should be used to service IO operations. 0 = use # of CPU cores.
        IOService(u64 threadCount = 0);
        ~IOService();

        boost::asio::io_service mIoService;
		boost::asio::strand<boost::asio::io_service::executor_type> mStrand;

        Work mWorker;

        std::list<std::pair<std::thread, std::promise<void>>> mWorkerThrds;
        
        // The list of acceptor objects that hold state about the ports that are being listened to. 
        std::list<Acceptor> mAcceptors;

        // indicates whether stop() has been called already.
		bool mStopped = false;

        // Gives a new endpoint which is a host endpoint the acceptor which provides sockets. 
        void aquireAcceptor(std::shared_ptr<SessionBase>& session);

        // Shut down the IO service. WARNING: blocks until all Channels and Sessions are stopped.
        void stop();

        void showErrorMessages(bool v);

        void printError(std::string msg);

        void workUntil(std::future<void>& fut);

        operator boost::asio::io_context&() {
            return mIoService;
        }

        bool mPrint = true;
    }; 


	namespace details
	{
        // A socket which has not been named yet. The Acceptor will create these
        // objects and then call listen on them. When a new connection comes in, 
        // the Accept will receive the socket's name. At this point it will
        // be converted to a NamedSocket and matched with a Channel.
        struct PendingSocket {
            PendingSocket(boost::asio::io_service& ios) : mSock(ios) {}
            boost::asio::ip::tcp::socket mSock;
            std::string mBuff;
//#ifdef ENABLE_NET_LOG
            u64 mIdx = 0;
//#endif
        };

        // A single socket that has been named by client. Next it will be paired with 
        // a Channel at which point the object will be destructed.
		struct NamedSocket {
			NamedSocket() = default;
			NamedSocket(NamedSocket&&) = default;

			std::string mLocalName, mRemoteName;
			std::unique_ptr<BoostSocketInterface> mSocket;
		};

        // A group of sockets from a single remote session which 
        // has not been paired with a local session/Channel. 
		struct SocketGroup
		{
			SocketGroup() = default;
			SocketGroup(SocketGroup&&) = default;

            ~SocketGroup();

            // returns whether this socket group has a socket with a matching 
            // name session name and channel name.
			bool hasMatchingSocket(const std::shared_ptr<ChannelBase>& chl) const;

            // Removes this SocketGroup from the containing Acceptor once this
            // group has been merged with a SessionGroup or on cleanup.
			std::function<void()> removeMapping;

            // The human readable name associated with the remote session.
            // Can be empty.
			std::string mName;

            // The unique session identifier chosen by the client
			u64 mSessionID = 0;

            // The unclaimed sockets associated with this remote session.
			std::list<NamedSocket> mSockets;
		};
        
        // A local session that pairs up incoming sockets with local sessions.
        // A SessionGroup is created when a local Session is created. 
		struct SessionGroup
		{
			SessionGroup() = default;
            ~SessionGroup();

            // return true if there are active channels waiting for a socket
            // or if there is an active session associated with this group.
			bool hasSubscriptions() const {
                return mChannels.size() || mBase.expired() == false;// || mPendingChls;
			}

            // Add a newly accepted socket to this group. If there is a matching
            // ChannelBase, this socket will be directly given to it. Otherwise
            // the socket will be stored locally and wait for a matching ChannelBase
            // or until the local session is destructed.
			void add(NamedSocket sock, Acceptor* a);

            // Add a local ChannelBase to this group. If there is a matching socket,
            // this socket will be handed to the ChannelBase. Otherwise the a shared_ptr
            // to the ChannelBase will be stored here until the associated socket is 
            // connected or the channel is canceled.
			void add(const std::shared_ptr<ChannelBase>& chl, Acceptor* a);

            // returns true if there is a ChannelBase in this group that has the same 
            // channel name as this socket. The session name and id are not considered.
			bool hasMatchingChannel(const NamedSocket& sock) const;

            // Takes all the sockets in the SocketGroup and pairs them up with 
            // ChannelBase's in this group. Any socket without a matching ChannelBase
            // will be stored here until it is matched or the session is destructed.
			void merge(SocketGroup& merge, Acceptor* a);

            // A weak pointer to the associated session. Has to be non-owning so that the 
            // session is destructed when it has no more channel/references.
			std::weak_ptr<SessionBase> mBase;

            // removes this session group from the associated Acceptor. Should be called when 
            // hasSubscription returns false.
			std::function<void()> removeMapping;

            // The list of unmatched sockets that we know are associated with this session.
			std::list<NamedSocket> mSockets;

            // The list of unmatched Channels what are associated with this session.
			std::list<std::shared_ptr<ChannelBase>> mChannels;

            std::list<std::shared_ptr<details::SessionGroup>>::iterator mSelfIter;
		};
	}

	class Acceptor
	{

	public:
		Acceptor() = delete;
		Acceptor(const Acceptor&) = delete;

		Acceptor(IOService& ioService);
		~Acceptor();

		std::promise<void> mPendingSocketsEmptyProm, mStoppedPromise;
		std::future<void> mPendingSocketsEmptyFuture, mStoppedFuture;

		IOService& mIOService;

		boost::asio::strand<boost::asio::io_service::executor_type> mStrand;
		boost::asio::ip::tcp::acceptor mHandle;

		std::atomic<bool> mStopped;

        
        bool mListening = false;

        u64 mPendingSocketIdx = 0;

        // The list of sockets which have connected but which have not 
        // completed the initial handshake which allows them to be named.
		std::list<details::PendingSocket> mPendingSockets;
		
		typedef std::list<std::shared_ptr<details::SessionGroup>> GroupList;
		typedef std::list<details::SocketGroup> SocketGroupList;

        // The full list of unmatched named sockets groups associated with this Acceptor. 
        // Each group has a session name (hint) and a unqiue session ID. 
		SocketGroupList mSockets;

        // The full list of matched and unmatched SessionGroups. These all have active Sessions
        // or active Channels which are waiting on matching sockets.
		GroupList mGroups;

		// A map: SessionName -> { socketGroup1, ..., socketGroupN } which all have the 
        // same session name (hint) but a unique ID. These groups of sockets have not been 
        // matched with a local Session/Channel. Then this happens the SocketGroup is merged 
        // into a SessionGroup
		std::unordered_map<std::string, std::list<SocketGroupList::iterator>> mUnclaimedSockets;

		// A map: SessionName -> { sessionGroup1, ..., sessionGroupN }. Each session has not 
        // been paired up with sockets. The key is the session name. For any given session name, 
        // there may be several sessions. When this happens, the first matching socket name gives 
        // that group a Session ID.
		std::unordered_map<std::string, std::list<GroupList::iterator>> mUnclaimedGroups;

        // A map: SessionName || SessionID -> sessionGroup. The list of sessions what have been 
        // matched with a remote session. At least one channel must have been matched and this 
        // SessionGroup has a session ID.
		std::unordered_map<std::string, GroupList::iterator> mClaimedGroups;

        // Takes the connection string name="SessionName`SessionID`remoteName`localName"
        // and matches the name with a compatable ChannelBase. SessionName is not unique,
        // the remote and local name of the channel itself will be used. Note SessionID
        // will always be unique.
		void asyncSetSocket(std::string name,std::unique_ptr<BoostSocketInterface> handel);

        // Let the acceptor know that this channel is looking for a socket
        // with a matching name.
		void asyncGetSocket(std::shared_ptr<ChannelBase> chl);

        // Remove this channel from the list of channels awaiting
        // a matching socket. effectively undoes asyncGetSocket(...);
		void cancelPendingChannel(std::shared_ptr<ChannelBase> chl);

        // return true if any sessions are still accepting connections or
        // if there are still some channels with unmatched sockets.
		bool hasSubscriptions() const;

        // Make this session are no longer accepting any *new* connections. Channels
        // that have previously required sockets will still be matched.
		void unsubscribe(SessionBase* ep);

        // Make this session as interested in accepting connecctions. Will create
        // a SessionGroup.
		void asyncSubscribe(std::shared_ptr<SessionBase>& session, completion_handle ch);

        //void subscribe(std::shared_ptr<ChannelBase>& chl);

        // returns a pointer to the session group that has the provided name and 
        // seesion ID. If no such socket group exists, one is created and returned.
		SocketGroupList::iterator getSocketGroup(const std::string& name, u64 id);

		void stopListening();
		u64 mPort;
		boost::asio::ip::tcp::endpoint mAddress;

		void bind(u32 port, std::string ip, boost::system::error_code& ec);
		void start();
		void stop();
		bool stopped() const;
		bool isListening() const { return mListening; };


        void sendServerMessage(std::list<details::PendingSocket>::iterator iter);

        void recvConnectionString(std::list<details::PendingSocket>::iterator iter);
        void erasePendingSocket(std::list<details::PendingSocket>::iterator iter);


		std::string print() const;
#ifdef ENABLE_NET_LOG
        Log mLog;
#endif

	};

}
#endif
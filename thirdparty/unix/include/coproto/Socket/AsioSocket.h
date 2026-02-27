#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "coproto/config.h"
#ifdef COPROTO_ENABLE_BOOST
#include "coproto/Socket/Socket.h"
#include "coproto/Common/Optional.h"
#include "coproto/Common/macoro.h"
#include <boost/asio.hpp>
#ifdef COPROTO_ENABLE_OPENSSL
#include <boost/asio/ssl.hpp>
#endif

#include <mutex>
#include <string>
#include <vector>

#define COPROTO_ASIO_LOG
#ifndef NDEBUG
	#define COPROTO_ASIO_DEBUG
#endif

namespace coproto
{
	namespace detail
	{
		struct GlobalIOContext
		{
			boost::asio::io_context mIoc;
			optional<boost::asio::io_context::work> mWork;
			std::vector<std::thread> mThreads;

			GlobalIOContext()
				:mWork(mIoc)
				, mThreads(2)
			{
				for (auto& thrd : mThreads)
					thrd = std::thread([this] {mIoc.run(); });
			}

			~GlobalIOContext()
			{
				mWork.reset();
				for (auto& thrd : mThreads)
					thrd.join();
			}
		};
		
		extern optional<GlobalIOContext> global_asio_io_context;
		extern std::mutex global_asio_io_context_mutex;
		inline void init_global_asio_io_context()
		{
			std::lock_guard<std::mutex> lock(global_asio_io_context_mutex);
			if (!global_asio_io_context)
			{
				global_asio_io_context.emplace();
			}
		}

		inline void destroy_global_asio_io_context()
		{
			std::lock_guard<std::mutex> lock(global_asio_io_context_mutex);
			global_asio_io_context.reset();
		}

		struct AsioLifetime
		{
#ifdef COPROTO_ASIO_DEBUG
			std::unique_ptr<std::atomic<u64>> mCount;

			AsioLifetime()
				:mCount(new std::atomic<u64>(0))
			{}

			AsioLifetime(AsioLifetime&&) = default;


			~AsioLifetime()
			{
				if (mCount && *mCount != 0)
					std::terminate();
			}

			struct Lock
			{
				AsioLifetime* mL = nullptr;
				AsioLifetime* mBase = nullptr;

				Lock() = default;
				Lock(Lock&& o) : mL(std::exchange(o.mL, nullptr)), mBase(o.mBase) {}
				Lock& operator=(Lock&& o)
				{
					reset();
					mBase = o.mBase;
					mL = std::exchange(o.mL, nullptr);
					return *this;
				}

				Lock(AsioLifetime* l)
					: mL(l)
					, mBase(l)
				{
					++* mL->mCount;
				}

				~Lock()
				{
					reset();
				}

				void reset()
				{
					if (mL)
					{
						if ((*mL->mCount)-- == 0)
							std::terminate();
					}
				}
			};

			Lock lock() {
				return this;
			}

			struct LockPtr
			{
				Lock* mPtr = nullptr;

				void reset()
				{
					if (mPtr)
						delete mPtr;
					mPtr = nullptr;
				}
			};

			LockPtr lockPtr()
			{
				return { new Lock(this) };
			}
#else
			struct Lock
			{
				void reset() {}
			};

			struct LockPtr
			{
				void reset() {}
			};
			static LockPtr lockPtr()
			{
				return {};
			}
			static Lock lock()
			{
				return {};
			}
#endif
		};

		template<typename SocketType = boost::asio::ip::tcp::socket>
		struct AsioSocket : public Socket
		{

			AsioSocket(SocketType&& s)
				: Socket(make_socket_tag{}, Sock(std::move(s)))
			{
				mSock = (Sock*)Socket::mImpl->getSocket();
			}


			AsioSocket() = default;
			AsioSocket(const AsioSocket&) = default;
			AsioSocket(AsioSocket&& o) : 
				Socket(std::move(o)), 
				mSock(std::exchange(o.mSock, nullptr))
			{}

			AsioSocket& operator=(const AsioSocket&) = default;
			AsioSocket& operator=(AsioSocket&& o)
			{
				static_cast<Socket&>(*this) = std::move(static_cast<Socket&>(o)),
				mSock = std::exchange(o.mSock, nullptr);
				return *this;
			}


			struct State;
			struct Sock;

			struct Awaiter
			{



				Awaiter(Sock* ss, span<u8> dd, bool send, macoro::stop_token&& t, i64 idx = 0)
					: mSock(ss)
					, mData(dd)
					, mType(send ? Type::send : Type::recv)
					, mToken(t)
					, mCancellationRequested(false)
					, mSynchronousFlag(false)
					, mActiveCount()
#ifdef COPROTO_ASIO_LOG
					, mLogState(ss->mState)
					, mIdx(idx)
#endif
				{
					COPROTO_ASSERT(dd.size());
					if (mToken.stop_possible())
					{
						mReg.emplace(mToken, [this] {
#ifdef COPROTO_ASIO_LOG
							mSock->log("cancel requested");
#endif

							mCancellationRequested = true;
							mCancelSignal.emit(boost::asio::cancellation_type::partial);
							});
					}
				}

				Awaiter(Awaiter&& a)
					: mSock(a.mSock)
					, mData(a.mData)
					, mBt(a.mBt)
					, mType(a.mType)
					, mEc(std::move(a.mEc))
					, mHandle(a.mHandle)
					, mCancellationRequested(a.mCancellationRequested.load())
					, mSynchronousFlag(a.mSynchronousFlag.load())
					, mToken(std::move(a.mToken))
					, mReg(std::move(a.mReg))
					, mActiveCount(std::move(a.mActiveCount))
#ifdef COPROTO_ASIO_LOG
					, mLogState(a.mLogState)
					, mIdx(a.mIdx)
#endif
				{
					if (mHandle)
					{
						std::cout << "awaiter can not move after being started" << std::endl;
						std::terminate();
					}
				}

				Sock* mSock;
				span<u8> mData;
				u64 mBt = 0;
				enum Type { send, recv };
				Type mType;
				optional<error_code> mEc;
				boost::asio::cancellation_signal mCancelSignal;
				std::atomic<bool> mCancellationRequested, mSynchronousFlag;
				AsioLifetime mActiveCount;

				coroutine_handle<> mHandle;
				macoro::stop_token mToken;
				macoro::optional_stop_callback mReg;

#ifdef COPROTO_ASIO_LOG
				std::shared_ptr<State> mLogState;
				i64 mIdx = 0;
#endif


				bool await_ready() { return false; }
				void await_suspend(macoro::coroutine_handle<> h);

#ifdef COPROTO_CPP20
				std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) {
					return await_suspend(macoro::coroutine_handle<>(h)).std_cast();
				}
#endif
				void callback(boost::system::error_code ec, std::size_t bt,
					AsioLifetime::Lock lt0,
					AsioLifetime::Lock lt1);

				std::pair<error_code, u64> await_resume() {
					COPROTO_ASSERT(mEc);
					return { *mEc, mBt };
				}

				error_code& ec();
				unique_function<error_code()>& errFn();
			};


			struct State
			{
				State(SocketType&& s)
					: mSock_(std::move(s))
#ifdef COPROTO_ASIO_DEBUG
					, mSslLock(false)
#endif
				{
#ifdef COPROTO_ASIO_LOG
					log("created");
#endif
					
				}

				~State()
				{
				}

				SocketType mSock_;

#ifdef COPROTO_ASIO_LOG
				std::mutex mLogMutex;
				std::vector<std::string> mLog;
				//std::array<u64, 2> mCounts{ {0, 0} };
				void log(std::string msg)
				{
					msg += " ";
					msg += std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

					std::lock_guard<std::mutex> ll(mLogMutex);
					mLog.push_back(std::move(msg));
				}
#endif
#ifdef COPROTO_ASIO_DEBUG
				std::atomic_bool mSslLock;
#endif
				AsioLifetime mOpCount;
			};


			// the actual implementation of the socket.
			struct Sock
			{
				std::shared_ptr<State> mState;

				Sock(SocketType&& s)
					: mState(std::make_shared<State>(std::move(s)))
				{}

				Sock(Sock&&) = default;

#ifdef COPROTO_ASIO_LOG
				~Sock()
				{
					if(mState)
						log("dstr");
				}
#endif

				void close();

				Awaiter send(span<u8> data, macoro::stop_token token = {}) { return Awaiter(this, data, true, std::move(token)); };
				Awaiter recv(span<u8> data, macoro::stop_token token = {}) { return Awaiter(this, data, false, std::move(token)); };

				Awaiter send(span<u8> data, macoro::stop_token token, u64 idx) { return Awaiter(this, data, true, std::move(token), idx); };
				Awaiter recv(span<u8> data, macoro::stop_token token, u64 idx) { return Awaiter(this, data, false, std::move(token), idx); };

#ifdef COPROTO_ASIO_LOG
				void log(std::string msg)
				{
					mState->log(std::move(msg));
				}
#endif

			};

			Sock* mSock = nullptr;


		};



		template<typename SocketType>
		inline void AsioSocket<SocketType>::Sock::close()
		{

			
			boost::asio::dispatch(mState->mSock_.get_executor(),
				[s = mState,
				lt = mState->mOpCount.lockPtr()
				]()mutable{

#ifdef COPROTO_ASIO_LOG
				s->log("close ");
#endif

				s->mSock_.lowest_layer().close();

				lt.reset();

				});
		}




		template<typename SocketType>
		inline void AsioSocket<SocketType>::Awaiter::callback(
			boost::system::error_code ec, 
			std::size_t bytesTrasfered,
			AsioLifetime::Lock lt0,
			AsioLifetime::Lock lt1
			)
		{
			//if (s.use_count() == 1)
			//{
			//	throw MACORO_RTE_LOC;
			//}
//#ifdef COPROTO_ASIO_LOG
//			if (s != mLogState)
//				throw MACORO_RTE_LOC;
//#endif
			//boost::asio::ssl::detail::openssl_init;
			mBt += bytesTrasfered;
			{
				mEc = (ec == boost::asio::error::operation_aborted)
					? error_code(code::operation_aborted)
					: error_code(ec);

				lt0 = {};
				lt1 = {};


#ifdef COPROTO_ASIO_LOG
				auto ss = mLogState;
				std::stringstream stream;
				stream << "await_suspend " << (mType == Type::send ?
					"send " : "recv ") << mIdx << " callback, msg=" + std::to_string(*(i64*)mData.data());

#endif
				auto f = mSynchronousFlag.exchange(true);

#ifdef COPROTO_ASIO_LOG
				stream << ", f = " << f << ", ec=" << mEc->message();
				ss->log(stream.str());
#endif
				// the caller has already suspended. Lets
				// resume them.
				if (f)
					mHandle.resume();

				// --------- DANGER -----------
				// this is destroyed

			}
		}

		template<typename SocketType>
		inline void AsioSocket<SocketType>::Awaiter::await_suspend(coroutine_handle<> h)
		{
			mHandle = h;
#ifdef COPROTO_ASIO_LOG
			mSock->log(std::string("await_suspend a ") + (mType == Type::send ?
				"send " : "recv ") + std::to_string(mIdx));
#endif
			boost::asio::dispatch(mSock->mState->mSock_.get_executor(),
				[this,
				lt0 = mSock->mState->mOpCount.lockPtr(), 
				lt1 = mActiveCount.lockPtr()
				]() mutable {

				using namespace boost::asio;

				if (!(mToken.stop_possible() && mCancellationRequested))
				{
#ifdef COPROTO_ASIO_LOG
					mSock->log(std::string("await_suspend b ") + (mType == Type::send ?
						"send " : "recv ") + std::to_string(mIdx));
#endif

#ifdef COPROTO_ASIO_DEBUG
					bool exp = false;
					if (!mSock->mState->mSslLock.compare_exchange_strong(exp, true))
						std::terminate();
#endif

					if (mType == Type::send)
					{
						async_write(mSock->mState->mSock_, boost::asio::const_buffer(mData.data(), mData.size()),
							boost::asio::bind_cancellation_slot(
								mCancelSignal.slot(),
								[this,
								lt0 = mSock->mState->mOpCount.lock(),
								lt1 = mActiveCount.lock()
								](boost::system::error_code error, std::size_t n) mutable {
									callback(error, n, std::move(lt0), std::move(lt1));
								}
						));
					}
					else
					{
						async_read(mSock->mState->mSock_, boost::asio::mutable_buffer(mData.data(), mData.size()),
							boost::asio::bind_cancellation_slot(
								mCancelSignal.slot(),
								[this,
								lt0 = mSock->mState->mOpCount.lock(),
								lt1 = mActiveCount.lock()
								](boost::system::error_code error, std::size_t n) mutable {

									callback(error, n, std::move(lt0), std::move(lt1));

								}
						));
					}

#ifdef COPROTO_ASIO_DEBUG
					exp = true;
					if (!mSock->mState->mSslLock.compare_exchange_strong(exp, false))
						std::terminate();
#endif


#ifdef COPROTO_ASIO_LOG
					mSock->log(std::string("await_suspend c ") + (mType == Type::send ?
						"send " : "recv ") + std::to_string(mIdx));
#endif

					if (mToken.stop_possible() && mCancellationRequested)
						mCancelSignal.emit(boost::asio::cancellation_type::partial);

					lt0.reset();
					lt1.reset();

					// After this operation our lifetime could end.
					auto f = mSynchronousFlag.exchange(true);
					// we completed synchronously if f==true;
					// this is needed sure we aren't destroyed
					// before checking if we need to emit
					// the cancellation.
					if (f)
						mHandle.resume();

					// --------- DANGER -----------
					// this is destroyed

				}
				else
				{
#ifdef COPROTO_ASIO_LOG
					mSock->log(std::string("await_suspend ") + (mType == Type::send ?
						"send " : "recv ") + std::to_string(mIdx) + " canceled immediately");
#endif
					mEc = code::operation_aborted;

					lt0.reset();
					lt1.reset();

					mHandle.resume();

					// --------- DANGER -----------
					// this is destroyed

				}

				});
		}
	}

	inline boost::asio::io_context& global_io_context()
	{
		detail::init_global_asio_io_context();
		return detail::global_asio_io_context->mIoc;
	}

	struct AsioSocket : public detail::AsioSocket<boost::asio::ip::tcp::socket>
	{
		AsioSocket(boost::asio::ip::tcp::socket&& s)
			: detail::AsioSocket<boost::asio::ip::tcp::socket>(std::move(s))
		{}

		AsioSocket() = default;
		AsioSocket(AsioSocket&&) = default;
		AsioSocket(const AsioSocket&) = default;
		AsioSocket& operator=(AsioSocket&&) = default;
		AsioSocket& operator=(const AsioSocket&) = default;

		static std::array<AsioSocket, 2> makePair(boost::asio::io_context& ioc);
		static std::array<AsioSocket, 2> makePair()
		{
			detail::init_global_asio_io_context();
			return makePair(detail::global_asio_io_context.value().mIoc);
		}
	};

#ifdef COPROTO_ENABLE_OPENSSL
	using AsioTlsSocket = detail::AsioSocket<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>;
#endif

#ifdef COPROTO_ASIO_LOG
	extern std::mutex ggMtx;
	extern std::vector<std::string> ggLog;
#endif

	struct AsioAcceptor
	{
		boost::asio::ip::tcp::acceptor mAcceptor;
		boost::asio::io_context& mIoc;

		AsioAcceptor(
			std::string address,
			boost::asio::io_context& ioc,
			int numConnection = boost::asio::socket_base::max_connections)
			: mAcceptor(boost::asio::make_strand(ioc))
			, mIoc(ioc)
		{


			boost::asio::ip::tcp::resolver resolver(boost::asio::make_strand(ioc));
			auto i = address.find(":");
			boost::asio::ip::tcp::endpoint endpoint;
			if (i != std::string::npos)
			{
				auto prefix = address.substr(0, i);
				auto posfix = address.substr(i + 1);
				endpoint = *resolver.resolve(prefix, posfix);
			}
			else
			{
				endpoint = *resolver.resolve(address);
			}

			mAcceptor.open(endpoint.protocol());
			mAcceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
			mAcceptor.bind(endpoint);
			mAcceptor.listen(numConnection);
		}



		struct Awaiter
		{
			using SocketType = boost::asio::ip::tcp::socket;
			Awaiter(AsioAcceptor& a, macoro::stop_token token = {})
				: mAcceptor(a)
				, mSocket(boost::asio::make_strand(a.mIoc))
				, mToken(std::move(token))
				, mCancellationRequested(false)
				, mSynchronousFlag(false)
				, mStarted(false)
			{

#ifdef COPROTO_ASIO_LOG
				log("init");
#endif
			}

			Awaiter(const Awaiter&) = delete;
			Awaiter(Awaiter&& a)
				: mAcceptor(a.mAcceptor)
				, mEc(a.mEc)
				, mSocket(std::move(a.mSocket))
				, mToken(std::move(a.mToken))
				, mCancellationRequested(false)
				, mSynchronousFlag(false)
				, mStarted(false)
			{
				if(a.mStarted)
				{
					std::cout << "AsioAcceptor::Awaiter can not be moved after it has started. " << COPROTO_LOCATION << std::endl;
					std::terminate();
				}
			}

			void log(std::string s)
			{
				std::lock_guard<std::mutex> l(::coproto::ggMtx);
				ggLog.push_back("accept::" + s);
			}

			AsioAcceptor& mAcceptor;
			boost::system::error_code mEc;
			SocketType mSocket;
			macoro::stop_token mToken;
			macoro::optional_stop_callback mReg;
			boost::asio::cancellation_signal mCancelSignal;
			std::atomic<bool> mCancellationRequested, mSynchronousFlag, mStarted;

			bool await_ready() { return false; }
#ifdef COPROTO_CPP20
			void await_suspend(std::coroutine_handle<> h)
			{
				await_suspend(coroutine_handle<>(h));
			}
#endif
			void await_suspend(coroutine_handle<> h)
			{

#ifdef COPROTO_ASIO_LOG
				log("await_suspend");
#endif
				if (mToken.stop_possible())
				{
					assert(mCancellationRequested == false);
					mReg.emplace(mToken, [this] {
						
#ifdef COPROTO_ASIO_LOG
						log("cancel requested");
#endif
						mCancellationRequested = true;
						mCancelSignal.emit(boost::asio::cancellation_type::partial);
						});

					mAcceptor.mAcceptor.async_accept(mSocket,
						boost::asio::bind_cancellation_slot(
							mCancelSignal.slot(),
							[this, h](boost::system::error_code ec) {

#ifdef COPROTO_ASIO_LOG
								log("async_accept");
#endif
								mEc = ec;

								auto f = mSynchronousFlag.exchange(true);

								if (f)
								{

#ifdef COPROTO_ASIO_LOG
									log("resume async");
#endif
									h.resume();
								}
							}
					));

					if (mCancellationRequested)
						mCancelSignal.emit(boost::asio::cancellation_type::partial);

					auto f = mSynchronousFlag.exchange(true);
					// we completed synchronously if f==true;
					// this is needed sure we aren't destroyed
					// before checking if we need to emit
					// the cancellation.
					if (f)
					{

#ifdef COPROTO_ASIO_LOG
						log("resume sync");
#endif

						h.resume();
					}
				}
				else
				{
					mAcceptor.mAcceptor.async_accept(mSocket, [this, h](boost::system::error_code ec) {
						mEc = ec;

#ifdef COPROTO_ASIO_LOG
						log("resume **");
#endif
						h.resume();
						});
				}

			}

			AsioSocket await_resume()
			{

#ifdef COPROTO_ASIO_LOG
				log("await_resume");
#endif
				if (mEc)
					throw std::system_error(mEc);

				boost::asio::ip::tcp::no_delay option(true);
				mSocket.set_option(option);
				return { std::move(mSocket) };
			}
		};

		Awaiter MACORO_OPERATOR_COAWAIT()
		{
			return { *this };
		}

		Awaiter accept(macoro::stop_token token = {})
		{
			return { *this, std::move(token) };
		}

	};



	struct AsioIoContextPostAwaiter
	{
		boost::asio::io_context& mIoc;

		bool await_ready() { return false; }
#ifdef COPROTO_CPP20
		void await_suspend(std::coroutine_handle<> h)
		{
			await_suspend(coroutine_handle<>(h));
		}
#endif
		void await_suspend(coroutine_handle<> h)
		{
			mIoc.post([h] {h.resume(); });
		}
		void await_resume() {}
	};


	inline AsioIoContextPostAwaiter transfer_to(boost::asio::io_context& ioc)
	{
		return { ioc };
	}

	struct AsioConnect
	{
		using SocketType = boost::asio::ip::tcp::socket;
		//boost::asio::io_context& mIoc;
		boost::system::error_code mEc;
		SocketType mSocket;
		boost::asio::io_context& mIoc;
		boost::asio::ip::tcp::endpoint mEndpoint;
		macoro::stop_token mToken;
		macoro::optional_stop_callback mReg;

		boost::asio::cancellation_signal mCancelSignal;
		std::atomic<char> mCancellationRequested, mSynchronousFlag, mStarted;
		bool mRetryOnFailure;

		boost::posix_time::time_duration mRetryDelay;
		boost::asio::deadline_timer mTimer;

		coroutine_handle<> mHandle;
		//enum class Status
		//{
		//	Init
		//};

		//std::atomic<Status> mStatus;


		void log(std::string s)
		{
			std::lock_guard<std::mutex> l(::coproto::ggMtx);
			ggLog.push_back("connect::" + s);
		}

		AsioConnect(
			std::string address,
			boost::asio::io_context& ioc,
			macoro::stop_token token = {},
			bool retryOnFailure = true
			)
			: mSocket(boost::asio::make_strand(ioc))
			, mIoc(ioc)
			, mToken(token)
			, mCancellationRequested(0)
			, mSynchronousFlag(0)
			, mStarted(0)
			, mRetryOnFailure(retryOnFailure)
			, mRetryDelay(boost::posix_time::milliseconds(1))
			, mTimer(ioc)
		{

#ifdef COPROTO_ASIO_LOG
			log("init");
#endif

			auto i = address.find(":");
			boost::asio::ip::tcp::resolver resolver(boost::asio::make_strand(ioc));
			if (i != std::string::npos)
			{
				auto prefix = address.substr(0, i);
				auto posfix = address.substr(i + 1);
				mEndpoint = *resolver.resolve(prefix, posfix);
			}
			else
			{
				mEndpoint = *resolver.resolve(address);
			}
		}

		AsioConnect(const AsioConnect&) = delete;
		AsioConnect(AsioConnect&& a)
			: mEc(a.mEc)
			, mIoc(a.mIoc)
			, mSocket(std::move(a.mSocket))
			, mEndpoint(std::move(a.mEndpoint))
			, mToken(std::move(a.mToken))
			, mCancellationRequested(0)
			, mSynchronousFlag(0)
			, mStarted(0)
			, mRetryOnFailure(a.mRetryOnFailure)
			, mRetryDelay(boost::posix_time::milliseconds(1))
			, mTimer(std::move(a.mTimer))
		{
			if (a.mStarted)
			{
				std::cout << "AsioConnect can not be moved after it has started. " << COPROTO_LOCATION << std::endl;
				std::terminate();
			}
		}

		bool await_ready() {
			
			mStarted = 1;
			if (mToken.stop_possible())
			{
				if(!mReg)
				{
					assert(!mCancellationRequested);
					mReg.emplace(mToken, [this] {

#ifdef COPROTO_ASIO_LOG
						log("cancel callback, emit");
#endif
						mCancellationRequested = 1;
						mCancelSignal.emit(boost::asio::cancellation_type::partial);
						});
				}
			}
			return false; 
		}
#ifdef COPROTO_CPP20
		void await_suspend(std::coroutine_handle<> h)
		{
			await_suspend(coroutine_handle<>(h));
		}
#endif
		void await_suspend(coroutine_handle<> h)
		{
			mHandle = h;

#ifdef COPROTO_ASIO_LOG
			log("await_suspend");
#endif

				mSocket.async_connect(mEndpoint,
					boost::asio::bind_cancellation_slot(
						mCancelSignal.slot(),
						[this](boost::system::error_code ec) {

#ifdef COPROTO_ASIO_LOG
							log("async_connect callback");
#endif
							mEc = ec;

							auto f = mSynchronousFlag.exchange(1);

							if (f)
							{
								if (mEc == boost::system::errc::connection_refused && mRetryOnFailure && !mCancellationRequested)
								{
									retry();
								}
								else
								{

#ifdef COPROTO_ASIO_LOG
									log("async_connect callback resume");
#endif
									mHandle.resume();
								}
							}
						}
				));

				if (mCancellationRequested)
				{
					
#ifdef COPROTO_ASIO_LOG
					log("emit cancellation");
#endif
					mCancelSignal.emit(boost::asio::cancellation_type::partial);
				}
				auto f = mSynchronousFlag.exchange(1);
				// we completed synchronously if f==true;
				// this is needed sure we aren't destroyed
				// before checking if we need to emit
				// the cancellation.
				if (f)
				{
					if (mEc == boost::system::errc::connection_refused && mRetryOnFailure && !mCancellationRequested)
					{
						retry();
					}
					else
					{
						
#ifdef COPROTO_ASIO_LOG
						log("async_connect sync resume");
#endif
						mHandle.resume();
					}
				}
// 			}
// 			else
// 			{

// 				mSocket.async_connect(mEndpoint, [this](boost::system::error_code ec) {
// 					if (ec == boost::system::errc::connection_refused && mRetryOnFailure)
// 					{

// #ifdef COPROTO_ASIO_LOG
// 						log(" "+ std::to_string(ec.value())+ " " + ec.message());
// #endif
// 						mTimer.expires_from_now(mRetryDelay);
// 						mTimer.async_wait(
// 							[this](error_code ec) {
// 							mSynchronousFlag = false;
// 							mRetryDelay = std::min<boost::posix_time::time_duration>(
// 								mRetryDelay * 2,
// 								boost::posix_time::milliseconds(1000)
// 								);

// #ifdef COPROTO_ASIO_LOG
// 							log("retry");
// #endif
// 							mSocket.close();
// 							await_suspend(mHandle);
// 							});
// 					}
// 					else
// 					{
// 						mEc = ec;
// #ifdef COPROTO_ASIO_LOG
// 						log("async_connect callback resume: " + ec.message());
// #endif
// 						mHandle.resume();
// 					}
// 					});
// 			}
		}

		AsioSocket await_resume()
		{
			if (mEc)
				throw std::system_error(mEc);

			boost::asio::ip::tcp::no_delay option(true);
			mSocket.set_option(option);
			return { std::move(mSocket) };
		}

		void retry()
		{
			mTimer.expires_from_now(mRetryDelay);
			mTimer.async_wait(
				boost::asio::bind_cancellation_slot(
					mCancelSignal.slot(), 
					[this](error_code ec) {
					mSynchronousFlag = 0;
					mRetryDelay = std::min<boost::posix_time::time_duration>(
						mRetryDelay * 2,
						boost::posix_time::milliseconds(1000)
						);
						
#ifdef COPROTO_ASIO_LOG
					log("retry");
#endif
					mSocket.close();
					await_suspend(mHandle);
				}));

			if (mToken.stop_possible() && mCancellationRequested)
				mCancelSignal.emit(boost::asio::cancellation_type::partial);

		}
	};

#ifdef COPROTO_ENABLE_OPENSSL
	struct AsioTlsAcceptor
	{
		AsioAcceptor mAcceptor;
		boost::asio::ssl::context& mContext;

		AsioTlsAcceptor(
			std::string address,
			boost::asio::io_context& ioc,
			boost::asio::ssl::context& context)
			: mAcceptor(address, ioc)
			, mContext(context)
		{}


		struct AcceptAwaiter
		{
			using SocketType = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;
			AcceptAwaiter(
				AsioTlsAcceptor& a,
				boost::asio::ssl::context& context,
				macoro::stop_token token)
				: mAcceptor(a)
				, mSocket(boost::asio::make_strand(a.mAcceptor.mIoc), context)
				, mToken(std::move(token))
			{}

			AcceptAwaiter(const AcceptAwaiter&) = delete;
			AcceptAwaiter(AcceptAwaiter&& a)
				: mAcceptor(a.mAcceptor)
				, mEc(a.mEc)
				, mSocket(std::move(a.mSocket))
				, mToken(std::move(a.mToken))
			{}

			AsioTlsAcceptor& mAcceptor;
			boost::system::error_code mEc;
			SocketType mSocket;
			macoro::stop_token mToken;
			macoro::optional_stop_callback mReg;

			boost::asio::cancellation_signal mCancelSignal;
			std::atomic<bool> mCancellationRequested;
			std::atomic<int> mSynchronousFlag;

			bool await_ready() { return false; }
#ifdef COPROTO_CPP20
			void await_suspend(std::coroutine_handle<> h)
			{
				await_suspend(coroutine_handle<>(h));
			}
#endif
			void await_suspend(coroutine_handle<> h)
			{

				if (mToken.stop_possible())
				{
					mCancellationRequested = false;
					mReg.emplace(mToken, [this] {
						mCancellationRequested = true;
						mCancelSignal.emit(boost::asio::cancellation_type::terminal);
						});
				}
				mSynchronousFlag = 2;

				mAcceptor.mAcceptor.mAcceptor.async_accept(mSocket.lowest_layer(), boost::asio::bind_cancellation_slot(
					mCancelSignal.slot(),
					[this, h](boost::system::error_code ec) {
						mEc = ec;
						if (mEc)
						{
							auto f = (mSynchronousFlag -= 2);
							// we can call resume if the initiator has already exited.
							// otherwise they will resume it after we unwind.
							if (f < 0)
								h.resume();
						}
						else
						{
							mSocket.async_handshake(boost::asio::ssl::stream_base::server,  boost::asio::bind_cancellation_slot(
								mCancelSignal.slot(),
								[this, h](boost::system::error_code ec) {
									mEc = ec;

									auto f = mSynchronousFlag--;

									// we can call resume if the initiator and the 'accept' handle
									// have already exited. Otherwise one of them will resume it 
									// after we unwind.
									if (f == 0)
										h.resume();
								}));

							if (mToken.stop_possible() && mCancellationRequested)
								mCancelSignal.emit(boost::asio::cancellation_type::terminal);

							auto f = mSynchronousFlag--;
							// we can call resume if the initiator and the 'handshake' handle
							// have already exited. Otherwise one of them will resume it 
							// after we unwind.
							if (f == 0)
								h.resume();
						}
					}));

				if (mToken.stop_possible() && mCancellationRequested)
					mCancelSignal.emit(boost::asio::cancellation_type::terminal);

				auto f = mSynchronousFlag--;
				// we completed synchronously if f==3;
				// this is needed sure we aren't destroyed
				// before checking if we need to emit
				// the cancellation.
				if (f == 0)
					h.resume();
			}

			AsioTlsSocket await_resume()
			{
				if (mEc)
				{
					//std::cout << mEc.message() << std::endl;
					throw std::system_error(mEc);
				}

				return { std::move(mSocket) };
			}
		};

		AcceptAwaiter MACORO_OPERATOR_COAWAIT()
		{
			return accept();
		}

		AcceptAwaiter accept(macoro::stop_token token = {})
		{
			return { *this, mContext, std::move(token) };
		}
	};

	struct AsioTlsConnect
	{
		using SocketType = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;

		AsioConnect mConnector;
		SocketType mSocket;


		AsioTlsConnect(
			std::string address,
			boost::asio::io_context& ioc,
			boost::asio::ssl::context& context, 
			macoro::stop_token token = {},
			bool retryOnFailure = true
			)
			: mConnector(std::move(address), ioc, token, retryOnFailure)
			, mSocket(boost::asio::make_strand(ioc), context)
		{}

		AsioTlsConnect(const AsioTlsConnect&) = delete;
		AsioTlsConnect(AsioTlsConnect&& a)
			: mConnector(std::move(a.mConnector))
			, mSocket(std::move(a.mSocket))
		{}

		bool await_ready() { 
			
			if (mConnector.mToken.stop_possible())
			{
				mConnector.mCancellationRequested = false;
				mConnector.mReg.emplace(mConnector.mToken, [this] {
					mConnector.mCancellationRequested = true;
					mConnector.mCancelSignal.emit(boost::asio::cancellation_type::terminal);
					});
			}
			mConnector.mSynchronousFlag = 2;
			return false; 
		}
#ifdef COPROTO_CPP20
		void await_suspend(std::coroutine_handle<> h)
		{
			await_suspend(coroutine_handle<>(h));
		}
#endif
		void await_suspend(coroutine_handle<> h)
		{
			mConnector.mHandle = h;
			mSocket.lowest_layer().async_connect(mConnector.mEndpoint, boost::asio::bind_cancellation_slot(
				mConnector.mCancelSignal.slot(),
				[this](boost::system::error_code ec) {
					mConnector.mEc = ec;
					if (mConnector.mEc)
					{
					
						auto f = (mConnector.mSynchronousFlag -= 2);
						// we can call resume if the initiator has already exited.
						// otherwise they will resume it after we unwind.
						if (f < 0)
						{
							if(mConnector.mEc == boost::system::errc::connection_refused && mConnector.mRetryOnFailure)
							{
								retry();
							}
							else
							{
								auto h = std::exchange(mConnector.mHandle, nullptr);
								h.resume();
							}
						}
					}
					else
					{
						mSocket.async_handshake(boost::asio::ssl::stream_base::client, boost::asio::bind_cancellation_slot(
							mConnector.mCancelSignal.slot(),
							[this](boost::system::error_code ec)
							{
								mConnector.mEc = ec;

								auto f = mConnector.mSynchronousFlag--;

								// we can call resume if the initiator and the 'accept' handle
								// have already exited. Otherwise one of them will resume it 
								// after we unwind.
								if (f == 0)
								{

									auto h = std::exchange(mConnector.mHandle, nullptr);
									h.resume();
								}
							}));


						if (mConnector.mToken.stop_possible() && mConnector.mCancellationRequested)
							mConnector.mCancelSignal.emit(boost::asio::cancellation_type::terminal);

						auto f = mConnector.mSynchronousFlag--;
						// we can call resume if the initiator and the 'handshake' handle
						// have already exited. Otherwise one of them will resume it 
						// after we unwind.
						if (f == 0)
						{
							if(mConnector.mEc == boost::system::errc::connection_refused && mConnector.mRetryOnFailure)
							{
								retry();
							}
							else
							{
								auto h = std::exchange(mConnector.mHandle, nullptr);
								h.resume();
							}
						}
					}
				}));


			if (mConnector.mToken.stop_possible() && mConnector.mCancellationRequested)
				mConnector.mCancelSignal.emit(boost::asio::cancellation_type::terminal);

			auto f = mConnector.mSynchronousFlag--;
			// we completed synchronously if f==3;
			// this is needed sure we aren't destroyed
			// before checking if we need to emit
			// the cancellation.
			if (f == 0)
			{
				auto h = std::exchange(mConnector.mHandle, nullptr);
				h.resume();
			}
		}


		AsioTlsSocket await_resume()
		{
			if (mConnector.mEc)
			{
				//std::cout << "c, " << mConnector.mEc.message() << std::endl;
				throw std::system_error(mConnector.mEc);
			}

			boost::asio::ip::tcp::no_delay option(true);
			mSocket.lowest_layer().set_option(option);
			return { std::move(mSocket) };
		}


		void retry()
		{
			mConnector.mTimer.expires_from_now(mConnector.mRetryDelay);
			mConnector.mTimer.async_wait(
				boost::asio::bind_cancellation_slot(
					mConnector.mCancelSignal.slot(), 
					[this](error_code ec) {
					mConnector.mSynchronousFlag = 2;
					mConnector.mRetryDelay = std::min<boost::posix_time::time_duration>(
						mConnector.mRetryDelay * 2,
						boost::posix_time::milliseconds(1000)
						);
						
#ifdef COPROTO_ASIO_LOG
					log("retry");
#endif

					mSocket.lowest_layer().close();
					await_suspend(mConnector.mHandle);
				}));

			if (mConnector.mToken.stop_possible() && mConnector.mCancellationRequested)
				mConnector.mCancelSignal.emit(boost::asio::cancellation_type::partial);

		}


		void log(std::string s)
		{
			mConnector.log(s);
		}
	};

	struct OpenSslX509
	{
		X509* mPtr = nullptr;

		OpenSslX509(X509* p)
			: mPtr(p) {}
		OpenSslX509() = default;
		OpenSslX509(const OpenSslX509&) = delete;
		OpenSslX509(OpenSslX509&& o) : mPtr(std::exchange(o.mPtr, nullptr)) {}

		~OpenSslX509()
		{
			if (mPtr)
				X509_free(mPtr);
		}


		std::string oneline()
		{
			std::string str(2048, (char)0);
			auto name = X509_get_subject_name(mPtr);
			X509_NAME_oneline(name, &str[0], str.size());
			str.resize(strlen(str.c_str()));
			return str;
		}
		std::string commonName()
		{
			auto full = oneline();
			return full.substr(full.find_last_of("/CN=") + 1);
		}
	};

	inline OpenSslX509 getX509(AsioTlsSocket& sock)
	{
		return { SSL_get_peer_certificate(sock.mSock->mState->mSock_.native_handle()) };
	}


	inline OpenSslX509 getX509(boost::asio::ssl::context& ctx)
	{
		return { SSL_CTX_get0_certificate(ctx.native_handle()) };
	}
#endif

	inline std::array<AsioSocket, 2> AsioSocket::makePair(boost::asio::io_context& ioc)
	{
		std::string address("localhost:1212");

		auto r = macoro::sync_wait(macoro::when_all_ready(
			macoro::make_task(AsioAcceptor(address, ioc)),
			macoro::make_task(AsioConnect(address, ioc))
		));

		return { std::get<0>(r).result(), std::get<1>(r).result() };
	}

	inline AsioSocket asioConnect(std::string address, bool server, boost::asio::io_context& ioc)
	{
		if (server)
		{
			return macoro::sync_wait(
				macoro::make_task(AsioAcceptor(address, ioc, 1))
			);
		}
		else
		{
			return macoro::sync_wait(
				macoro::make_task(AsioConnect(address, ioc))
			);
		}
	}

	inline AsioSocket asioConnect(std::string ip, bool server)
	{
		return asioConnect(ip, server, global_io_context());
	}

}

#endif


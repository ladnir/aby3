#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.



#include "coproto/Common/Defines.h"
#include "coproto/Common/span.h"
#include "coproto/Common/error_code.h"
#include "coproto/Common/TypeTraits.h"
#include "coproto/Common/Function.h"
#include "coproto/Common/macoro.h"

#include "coproto/Proto/SessionID.h"
#include "coproto/Socket/SocketScheduler.h"
#include "coproto/Proto/Buffers.h"

#include "macoro/take_until.h"
#include "macoro/timeout.h"

#include <cstring>

namespace coproto
{
	struct make_socket_tag
	{};


	// Socket represents a type erased socket-like object. It has three main functions
	// 
	// * auto send(Container&& data)
	//	
	//   This function should be awaited to send data.
	// 
	// * auto recv(Container& data)
	//	
	//   This function should be awaited to receive data.
	// 
	// * Socket fork() 
	// 
	//   This function creates a new socket that data can be sent over. Messages
	//   sent/received on the fork will not be mixed with the messages on the original 
	//   socket. This is used as a mechanism to perform several protocols on one socket
	//   while ensuring that each protocol only receives their own messages.
	// 
	// For additional usage, see the tutorial.
	// 
	// 
	// 
	// The actual implementation is provided by a user defined SocketImpl 
	// class. SocketImpl should implement the following interface:
	// 
	// * SendAwaiter SocketImpl::send(span<u8> data, macoro::stop_token token = {})
	//	
	//   Return some user defined class `SendAwaiter` that implements the Awaiter concept. 
	//   send(...) takes as input a span<u8> which is the data to be sent. It optionally 
	//   takes a stop_token as input which is used to cancel the async send.
	// 
	//   SendAwaiter should implement the following functions:
	//     * bool SendAwaiter::await_ready() 
	//       
	//       It is suggested that this always returns false. Return true if the operation 
	//       has somehow already completed and await_suspend should not be called.
	// 
	//     * macoro::coroutine_handle<> SendAwaiter::await_suspend(macoro::coroutine_handle<> h) 
	//       
	//       We suggest in this function you start the async send. If the send completes 
	//       immediately/synchronously, then await_suspend should return h. Otherwise, await_suspend
	//       should somehow store h and return macoro::noop_coroutine(). When the send
	//       completes asynchronously, call h.resume().
	// 
	//     * std::pair<error_code, u64> SendAwaiter::await_resume()
	// 
	//       This function is called after the send has completed to get the result. That is,
	//       its called when await_ready() returns true, or await_suspend(...) returns h, or 
	//       h.resume() is called. This function should return the error_code indicating the 
	//       success or failure of the operation and how many bytes were sent.
	//	
	// * RecvAwaiter SocketImpl::recv(span<u8> data, macoro::stop_token token = {})
	// 
	//   This is basically the same as send(...) but should receive data.
	//
	// For example implementations see the socket tutorial or LocalAsyncSocket, AsioSocket or BufferingSocket.
	//
	class Socket
	{
	public:

		// The session ID that this fork is communicating on. The first fork
		// will have value SessionID::root(). 
		SessionID mId;

		// The scheduler that performs all the Socket logic and interacts with 
		// the user defined SocketImpl.
		std::shared_ptr<internal::SockScheduler> mImpl;

		Socket() = default;
		Socket(const Socket& s) = default;
		Socket(Socket&& s) = default;


		// Construct a new socket using SocketImpl&& s as the underlaying socket. Use
		// getNative() to get a pointer to s.
		template<typename SocketImpl>
		Socket(make_socket_tag, SocketImpl&& s, SessionID sid = SessionID::root())
			: mId(sid)
			, mImpl(std::make_shared<internal::SockScheduler>(std::forward<SocketImpl>(s), mId))
		{}


		// Construct a new socket using std::unique_ptr<SocketImpl>&& s as the underlaying socket.
		template<typename SocketImpl>
		Socket(make_socket_tag, std::unique_ptr<SocketImpl>&& s, SessionID sid = SessionID::root())
			: mId(sid)
			, mImpl(std::make_shared<internal::SockScheduler>(std::move(s), mId))
		{}


		Socket& operator=(Socket&& s) = default;
		Socket& operator=(const Socket& s) = default;

		// Send the Container `t` with a timeout `to`. After the timeout the operation is canceled.
		// The return value must be awaited for the data to be scheduled. 
		//
		// WARNING: `t` will be buffered internally and the await operation will complete 
		// possibly before `t` has actually been sent. To ensure that `t` has been sent, 
		// await Socket::flush().
		template<typename Container>
		auto send(Container&& t, macoro::timeout&& to)
		{
			return macoro::take_until(send(std::forward<Container>(t), to.get_token()), std::move(to));
		}

		// Send the Container `t`. A stop_token can be provided to request that the send be 
		// canceled at some point. The return value must be awaited for the data to be sent. 
		template<typename Container>
		auto send(Container& t, macoro::stop_token token = {})
		{
			return internal::RefSendProto<Container>(mImpl.get(), mId, t, std::move(token));
		}

		// Send the Container `t`. A stop_token can be provided to request that the send be 
		// canceled at some point. The return value must be awaited for the data to be scheduled.
		//
		// WARNING: t will be buffered internally and the await operation will complete 
		// possibly before t has actually been sent. To ensure that t has been sent, 
		// await Socket::flush().
		template<typename Container>
		auto send(Container&& t, macoro::stop_token token = {})
		{
			return internal::MoveSendProto<Container>(mImpl.get(), mId, std::forward<Container>(t), std::move(token));
		}

		// Receive the next message into the container `r` with a timeout `to`. After the timeout 
		// the operation is canceled. The return value must be awaited for the data to be received.
		template<typename Container>
		auto recv(Container&& r, macoro::timeout&& to)
		{
			return macoro::take_until(recv(std::forward<Container>(r), to.get_token()), std::move(to));
		}

		// Receive the next message into the container `r`. An optional stop_token can be provided
		// to cancel the operation. The return value must be awaited for the data to be received.
		template<typename Container>
		auto recv(Container& t, macoro::stop_token token = {})
		{
			return internal::RefRecvProto<Container, false>(mImpl.get(), mId, t, std::move(token));
		}

		// Receive the next message into the container `r`. An optional stop_token can be provided
		// to cancel the operation. The return value must be awaited for the data to be received.
		template<typename Container>
		auto recv(Container&& t, macoro::stop_token token = {})
		{
			return internal::MoveRecvProto<Container, false>(mImpl.get(), mId, std::forward<Container>(t), std::move(token));
		}

		// Receive the next message and store the message in a class of type Container. An optional 
		// stop_token can be provided to cancel the operation. The return value must be awaited for 
		// the data to be received. 
		template<typename Container>
		auto recv(macoro::stop_token token = {})
		{
			return internal::MoveRecvProto<Container, true>(mImpl.get(), mId, std::move(token));
		}

		// Receive the next message into the container `r` with a timeout `to`. After the timeout 
		// the operation is canceled. `r` will be resized to the correct size of possible. The return 
		// value must be awaited for the data to be received.
		template<typename Container>
		auto recvResize(Container& r, macoro::timeout&& to)
		{
			return macoro::take_until(recvResize(r, to.get_token()), std::move(to));
		}

		// Receive the next message into the container `r`. An optional stop_token can be provided
		// to cancel the operation. `r` will be resized to the correct size of possible. The return 
		// value must be awaited for the data to be received.
		template<typename Container>
		auto recvResize(Container& t, macoro::stop_token token = {})
		{
			return internal::RefRecvProto<Container, true>(mImpl.get(), mId, t, std::move(token));
		}

		// returns the number of bytes sent.
		std::size_t bytesSent() {
			return mImpl->mBytesSent;
		}

		// returns the number of bytes received.
		std::size_t bytesReceived() {
			return mImpl->mBytesReceived;
		}

		// Cancel all operations that are currently being performed.
		// Future operations will complete an error.
		void close()
		{
			mImpl->close();
		}


		// returns an awaitable that completes when all current operations complete.
		// This is typically used to make sure async send operations complete
		// before the socket is destroyed.
		internal::Flush flush()
		{
			return internal::Flush(mImpl.get());
		}

		// Returns a new socket that can be used semi-independently.
		// The intended use of this is for each end of the socket to 
		// call fork. Messages on each fork will then remain separate.
		// However, the underlying socket will be sending all messages.
		// The messages are distinguished by sending a per fork tag.
		Socket fork()
		{
			Socket ret = *this;
			ret.mId = mImpl->fork(mId);
			return ret;
		}

		// return the underlaying socket.
		void* getNative() { return mImpl->getSocket(); }



		// Unstable function to enable logging.
		void enableLogging()
		{
			mImpl->enableLogging();
		}

		// Unstable function to disable logging.
		void disableLogging()
		{
			mImpl->disableLogging();
		}

		// Set the completion executor for this socket/fork. When
		// an operation completes, the initiating coroutine will be 
		// resumed by the scheduler. Then scheduler should implement
		// a member function called with signature
		//
		// void schedule(macoro::coroutine_handle<void>);
		//
		// schedule(...) will be called with the coroutine to be resumed.
		// The return value should be a coroutine that should be 
		// resumed within the current execution context. If unsure,
		// return macoro::noop_coroutine().
		template<typename Scheduler>
		void setExecutor(Scheduler& scheduler)
		{
			mImpl->setExecutor(scheduler, mId);
		}

	};

	template<typename T>
	Socket makeSocket(T&& t)
	{
		return Socket(make_socket_tag{}, std::forward<T>(t));
	}
}


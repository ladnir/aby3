#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "coproto/Socket/Socket.h"
#include <sstream>

namespace coproto
{

	inline std::string hex(span<u8> d)
	{
		std::stringstream ss;
		for (auto dd : d)
			ss << std::hex << std::setw(2) << std::setfill('0') << (int)dd;
		return ss.str();
	}

	// The class implements a socket that internally buffers all messages.
	// These messages can be obtained or set by some external code. This 
	// socket type is useful for when you want access to the messages in 
	// a somewhat generic way. 
	// 
	// An example use case of this type is to make a protocol run in "rounds."
	// Here we want to gather all messages that can be sent together, combine
	// them into one large message and then send it. Then we want to
	// receive a single large message from the other party, and set that. In
	// each round we should send one message & complete or send one message
	// & then receive one message.
	// 
	// Messages are obtained by calling getOutbound(). This returns a 
	// Buffer. The size of the buffer can be obtained by calling Buffer::size().
	// You can then get the contained data by calling Buffer::pop(span<u8> dest)
	// which will copy the buffered data into dest. Alternatively, you can
	// call Buffer::pop() to get returned a std::vector<u8> with all the 
	// buffered data in it.
	// 
	// You can set as incoming message by calling processInbound(span<u8> src).
	// This will buffer the data in src and resume the protocol if possible. The
	// protocol will not be resumed if the protocol is requesting more bytes than
	// has been buffered. Assuming the protocol was resumed, new outbound messages
	// might have been generated. In this case the user should send these message.
	// If no outbound messages have been generated the protocol might have completed
	// or is waiting on some async operation, e.g. receive another message or 
	// something else...
	// 
	// Due to the ambiguity between distinguishing between the protocol completing
	// and simply waiting on some other async operation, it can be difficult to 
	// decide what to do when no new outbound messages have been produced. Possible
	// solutions is to have the protocol set a flag when it completes. Then if the 
	// flag is not set, the user can try to receive more data. However, this strategy 
	// may not work well if the protocol can suspend on some other async operation
	// or if its run on some (multi-threaded) executor since processInbound(...)
	// might return even though the protocol is still making progress on the executor.
	//
	struct BufferingSocket : public Socket
	{
		struct SendRecvAwaiter;
		struct SockImpl;

		// Constructs the "actual socket" SockImpl and pass that to Socket.
		// This somewhat unusual pattern is used so that the Socket 
		// class itself owns the actual socket implementation and therefore
		// it is safe to pass a Socket by value. BufferingSocket is then simply
		// a reference type to the underlaying socket, ie SockImpl.
		BufferingSocket()
			: Socket(make_socket_tag{}, std::make_unique<SockImpl>())
			, mSock((SockImpl*)mImpl->getSocket())
		{
		}


		BufferingSocket(const BufferingSocket&) = default;
		BufferingSocket(BufferingSocket&&) = default;
		BufferingSocket& operator=(const BufferingSocket&) = default;
		BufferingSocket& operator=(BufferingSocket&&) = default;

		// The awaiter type for performing a send or a receive operation.
		// This is returned by SockImpl::send(...) and SockImpl::recv(...).
		// A send or receive operation involves a buffer mData and optionally
		// a stop_token that is used to cancel the async operation.
		struct SendRecvAwaiter
		{

			SendRecvAwaiter(SockImpl* ss, span<u8> dd, bool send, macoro::stop_token&& t);

			// The socket that the operation is performed on.
			SockImpl* mSock;

			// The data to be sent or received.
			span<u8> mData;

			enum Type { send, recv };

			// the type of operation.
			Type mType;

			// an error code that is set when the operation completes.
			optional<error_code> mEc;

			// the callback that is used when the operation completes.
			coroutine_handle<> mHandle;

			// an operation stop token.
			macoro::stop_token mToken;

			// the stop token callback that is called if the user gave a stop token
			// and then requested a stop (canceled the operation).
			macoro::optional_stop_callback mReg;

			// always have the user call await suspend to start the operation.
			bool await_ready() { return false; }

			// start the operation. If it completes synchronously the caller h will be 
			// returned and resumed.  
			macoro::coroutine_handle<> await_suspend(macoro::coroutine_handle<> h);

#ifdef COPROTO_CPP20
			std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) {
				return await_suspend(macoro::coroutine_handle<>(h)).std_cast();
			}
#endif

			// once the operation competes this returns the result. Its an
			// error code encoding how the operation completed and the number
			// of bytes that were sent or received.
			std::pair<error_code, u64> await_resume() {
				COPROTO_ASSERT(mEc);
				auto bt = (!*mEc) ? mData.size() : 0;
				return { *mEc, bt };
			}

		};


		struct Buffer
		{
			// the offset into the first buffer where the data starts;
			u64 mFront = 0;

			// total number of bytes contained in the buffer.
			u64 mSize = 0;
			std::deque<std::vector<u8>> mData;

			~Buffer() { size(); }
			Buffer() = default;
			Buffer(const Buffer&) = delete;
			Buffer(Buffer&&) = delete;

//			Buffer(const Buffer&) = default;
			//Buffer(Buffer&& o) 
			//	: mFront(std::exchange(o.mFront, 0))
			//	, mSize(std::exchange(o.mSize, 0))
			//	, mData(std::move(o.mData))
			//{};

			//Buffer& operator=(Buffer&& o)
			//{
			//	mFront = (std::exchange(o.mFront, 0));
			//	mSize  = (std::exchange(o.mSize, 0)) ;
			//	mData  = (std::move(o.mData))		 ;
			//	return *this;
			//}

			u64 size() {
				auto ss = 0ull;
				for (auto& b : mData)
					ss += b.size();

				ss -= mFront;
				COPROTO_ASSERT(ss == mSize);
				return mSize;
			}

			// get the next data.size() bytes from the buffer.
			void pop(span<u8> data)
			{
				COPROTO_ASSERT(mSize >= data.size());

				while (data.size())
				{
					auto m = std::min<u64>(data.size(), mData.front().size() - mFront);
					auto f = mData.front().begin() + mFront;
					std::copy(f, f + m, data.begin());
					mFront += m;
					mSize -= m;

					if (mFront == mData.front().size())
					{
						mData.pop_front();
						mFront = 0;
					}

					data = data.subspan(m);
				}

				size();
			}

			// add the data to the buffer.
			void push(span<u8> data)
			{
				mData.emplace_back(data.begin(), data.end());
				mSize += data.size();
				size();
			}


			// add the data to the buffer.
			void push(Buffer&& data)
			{
				mSize += data.size();
				data.mSize = 0;

				while (data.mData.size())
				{
					COPROTO_ASSERT(data.mFront == 0);
					mData.emplace_back(std::move(data.mData.front()));
					data.mData.pop_front();
				}
				size();
			}

			std::vector<u8> pop()
			{
				std::vector<u8> buff(size());
				auto iter = buff.begin();
				while (mData.size())
				{
					COPROTO_ASSERT(mFront == 0);
					std::copy(mData.front().begin(), mData.front().end(), iter);
					iter += mData.front().size();
					mData.pop_front();
				}
				mSize = 0;
				size();
				return buff;
			}
		};

		// the actual implementation of the socket.
		struct SockImpl
		{
			std::mutex mMtx;

			error_code mEc_ = code::success;

			//the awaiter that wants to receive data.
			SendRecvAwaiter* mInbound_ = nullptr;

			// data that needs to be sent.
			Buffer mOutboundBuffer_;

			// data that has been received and can consumes.
			Buffer mInboundBuffer_;

			//std::vector<std::string> mLog;

			void close();

			SendRecvAwaiter send(span<u8> data, macoro::stop_token token = {}) { return SendRecvAwaiter(this, data, true, std::move(token)); };
			SendRecvAwaiter recv(span<u8> data, macoro::stop_token token = {}) { return SendRecvAwaiter(this, data, false, std::move(token)); };
		};


		bool processInbound(span<u8> data) {

			macoro::coroutine_handle<> cb;
			{
				std::lock_guard<std::mutex> lock(mSock->mMtx);

				//mSock->mLog.push_back("processInbound\n inbound "+std::to_string((u64)mSock->mInbound_)+"\ndata " + hex(data));

				mSock->mInboundBuffer_.push(data);

				if (mSock->mInbound_ && (mSock->mInbound_->mData.size() <= mSock->mInboundBuffer_.size()))
				{
					mSock->mInboundBuffer_.pop(mSock->mInbound_->mData);
					mSock->mInbound_->mEc = code::success;
					cb = mSock->mInbound_->mHandle;
					mSock->mInbound_ = nullptr;
					COPROTO_ASSERT(cb);
				}

			}

			if (cb)
				cb.resume();

			return !!cb;
		}

		void setError(error_code ec)
		{
			COPROTO_ASSERT(ec);
			macoro::coroutine_handle<> cb;
			{
				std::lock_guard<std::mutex> lock(mSock->mMtx);

				if (!mSock->mEc_)
				{
					mSock->mEc_ = ec;

					if (mSock->mInbound_)
					{
						mSock->mInbound_->mEc = ec;
						cb = mSock->mInbound_->mHandle;
						mSock->mInbound_ = nullptr;
						COPROTO_ASSERT(cb);
					}
				}
			}

			if (cb)
				cb.resume();
		}

		optional<std::vector<u8>> getOutbound() {
			std::lock_guard<std::mutex> lock(mSock->mMtx);
			auto out = mSock->mOutboundBuffer_.pop();

			if (out.size() == 0 && mSock->mEc_)
				return {};

			return out;
		}

		static void exchangeMessages(BufferingSocket& s0, BufferingSocket& s1)
		{
			std::array<BufferingSocket, 2> s{ { s0,s1 } };

			bool progress = true;
			while (progress)
			{
				progress = false;
				for (u64 i = 0; i < 2; ++i)
				{
					auto bb = s[i].getOutbound();

					if (bb)
					{
						if (bb->size())
						{
							s[1 ^ i].processInbound(*bb);
							progress = true;
						}
					}
					else
						s[1 ^ i].setError(code::remoteClosed);
				}
			}
		}

		SockImpl* mSock = nullptr;

	};


	inline BufferingSocket::SendRecvAwaiter::SendRecvAwaiter(SockImpl* ss, span<u8> dd, bool send, macoro::stop_token&& t)
		: mSock(ss)
		, mData(dd)
		, mType(send ? Type::send : Type::recv)
		, mToken(t)
	{
		COPROTO_ASSERT(dd.size());
		if (mToken.stop_possible())
		{
			//register the cancellation callback.
			mReg.emplace(mToken, [this] {
				// cancellation was called. Its possible that
				//  1) the operation has not started, 
				//  2) the operation is currently being processed (different thread)
				//  3) the operation is scheduled asynchronously on the socket
				//  4) the operation has completed.
				// 

				coroutine_handle<> cb;

				{
					// lock the socket so make sure we dont have a race.
					std::unique_lock<std::mutex> lock(mSock->mMtx);

					// if the operation has completed, then mEc will be set.
					if (!mEc)
					{
						// ok we are in option 1 or 3. For 1 mHandle will be null.
						// In both cases we need to set mEc and for 3 we need to do 
						// some cleanup.

						mEc = code::operation_aborted;
						cb = mHandle;

						// case 3.
						if (cb)
						{
							// send operations always synchronous.
							if (mType == Type::recv)
							{
								COPROTO_ASSERT(mSock->mInbound_ == this);
								mSock->mInbound_ = nullptr;
							}
						}

					}
				}

				// call the handle is we have one.
				if (cb)
					cb.resume();
				});
		}


	}

	inline void BufferingSocket::SockImpl::close()
	{
		// close means we should cancel all operations 
		// and cancel all future requires.

		coroutine_handle<> cb;
		{
			// lock to make sure no active request is being performed.
			std::unique_lock<std::mutex> lock(mMtx);

			// if we already have an error, we will just keep that code.
			if (!mEc_)
				mEc_ = code::closed;

			// if we have an active receive, then we need to cancel it
			// and clear the pending receive from the socket.
			auto op = std::exchange(mInbound_, nullptr);

			if (op)
			{
				COPROTO_ASSERT(op->mHandle && !op->mEc);
				cb = op->mHandle;
				if (!op->mEc)
					op->mEc = code::operation_aborted;
			}
		}

		// if we had an active receiver, then call its completion handler
		if (cb)
			cb.resume();
	}

	inline coroutine_handle<> BufferingSocket::SendRecvAwaiter::await_suspend(coroutine_handle<> h)
	{
		// here we are going to try to start the async operation.
		// its possible that the operation has already been canceled

		// the completion handle. If we do not complete synchronously, then
		// we should return noop_coroutine. Otherwise we will return h which
		// will resume the caller.
		coroutine_handle<> c1 = macoro::noop_coroutine();
		macoro::optional_stop_callback* r0 = nullptr;

		{
			std::unique_lock<std::mutex> lock(mSock->mMtx);

			if (mEc)
			{
				// we were canceled before we started.
				c1 = h;
			}
			else
			{
				if (mType == Type::send)
				{
					// send operations always complete synchronously by 
					// simply adding the data to our internal buffer.

					//mSock->mLog.push_back("send " + hex(mData));
					mSock->mOutboundBuffer_.push(mData);
					mEc = code::success;
					c1 = h;

				}
				else
				{
					// receive operations complete synchronously if we have 
					// the requested data in our internal buffer. Otherwise
					// we record the request as mSock->mInbound and suspend.
					if (mSock->mInboundBuffer_.size() >= mData.size())
					{
						//mSock->mLog.push_back("pop " + hex(mData));
						mSock->mInboundBuffer_.pop(mData);
						mEc = code::success;
						c1 = h;
					}
					else
					{
						//mSock->mLog.push_back("pop* " + std::to_string(mData.size()));
						mHandle = h;
						mSock->mInbound_ = this;
					}
				}
			}
		}

		return c1;
	}



}
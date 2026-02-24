#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "coproto/Socket/Socket.h"
#include "coproto/Common/macoro.h"

namespace coproto
{
	// This Socket type implements the logic for communicating
	// within the current process. A socket pair can be created
	// by calling LocalAsyncSocket::makePair(). It is intended
	// to be used for unit tests and benchmarking protocols.
	// It implementations asynchronous communication with full 
	// cancellation support.
	//
	// The implementation comes in several parts. There is an 
	// object SharedState that both sockets will have in common.
	// 
	// As outlined in the socket tutorial, coproto::Socket is
	// expecting our socket type to implement the following
	// function.
	// 
	// * send(...)
	// * recv(...)
	// * close()
	// 
	// However, for easy of use, LocalAsyncSocket will actually
	// not implement any of these. Instead the actual socket
	// implementation will be LocalAsyncSocket::Sock. This
	// nested struct will implement the functions above. This
	// is done to hide some implementation details and allow 
	// LocalAsyncSocket to behave as a coproto::Socket.
	//
	struct LocalAsyncSocket : public Socket
	{
		// The common state that a pair of LocalAsyncSocket will hold.
		struct SharedState;

		// The awaiter that is returned from send(...) and recv(...)
		struct Awaiter;

		// A pair the Awaiters that will be matched up.
		struct OpPair
		{
			Awaiter* mSend = nullptr;
			Awaiter* mRecv = nullptr;
		};

		// The actual socket implementation.
		struct Sock;

		// This is a coroutine awaiter that is returned by Sock::send(...) and Sock::recv(...).
		// It represents the asynchronous operation of sending or receiving data. 
		// We also support cancellation of an asynchronous operation. This is achieved 
		// by implementing the coroutine awaiter interface:
		//
		// * await_ready()  
		// * await_suspend()
		// * await_resume() 
		//
		// See the function below for more details.
		struct Awaiter
		{

			Awaiter(Sock* ss, span<u8> dd, bool send, macoro::stop_token&& t);

			// A pointer to the socket that this io operation belongs to.
			Sock* mSock;

			// The data to be sent or received. This will shrink as we 
			// make progress in sending or receiving data.
			span<u8> mData;

			// The total amount of data to be sent or received.
			u64 mTotalSize;
			enum Type { send, recv };

			// The type of the operation (send or receive).
			Type mType;

			// An error code that is set once the operation has 
			// completed successfully or with an error.
			optional<error_code> mEc;

			// The coroutine callback that should be called when this operation completes.
			coroutine_handle<> mHandle;

			// An optional token that the user can provide to stop an asynchronous operation.
			// If mToken.stop_possible() returns true, then we will register a callback that
			// is called when/if the user decides to cancel this operation.
			macoro::stop_token mToken;

			// the stop callback.
			macoro::optional_stop_callback mReg;

			// We always return false. This means that await_suspend is always called.
			bool await_ready() { return false; }

			// This function starts the asynchronous operation. It performs something
			// called coroutine symmetric transfer. Thats a fancy way of saying it
			// takes the callback/coroutine_handle h as input. h is a callback to the
			// caller/initiator of the operation. If we complete synchronously, then
			// we should return h. This tells the coroutine framework to resume h, as 
			// the result is ready. If we do not complete synchronously, then we will
			// store h as mHandle and return noop_coroutine(). This is a special handle
			// that tells the caller that there is no more work to do at this time. 
			// Later, when the operation does complete we will resume h/mHandle.
			// 
			// In the event of cancellation, we will have registered a callback (mReg)
			// that is called whenever the user cancels the operation. In this callback
			// we will need to check if the operation is still pending and if so cancel 
			// it and then call h/mHandle
			// 
			// When h/mHandle is resumed, the first thing it does is call await_resume().
			// This allows the initiator (ie h) to get the "return value" of the operation.
			// In this case we will return an error_code and the number of bytes transfered.
			//
			macoro::coroutine_handle<> await_suspend(macoro::coroutine_handle<> h);

#ifdef COPROTO_CPP20
			// this version of await_suspend allows c++20 coroutine support.
			std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) {
				return await_suspend(macoro::coroutine_handle<>(h)).std_cast();
			}
#endif

			// once we complete, this function allows the initiator to get the result
			// of the operation. In particular a pair consisting of an error_code and
			// the number of bytes transfered. On success, the error_code should be
			// 0 and bytes transfered should be all of them. Otherwise error_code 
			// should be some other value.
			std::pair<error_code, u64> await_resume() {
				COPROTO_ASSERT(mEc);
				auto bt = mTotalSize - mData.size();
				return { *mEc, bt };
			}

			// helper functions
			error_code& ec();
			unique_function<error_code()>& errFn();
			OpPair& outbound();
			OpPair& inbound();


		};

		// The actual implementation of the socket. It implements
		// the send(...) and recv(...) functions. Both of these 
		// return an Awaiter that tracks & manages the progress of
		// the operation. Additionally we implement the close() function
		// that can be used to cancel any async operation and close
		// the socket.
		//
		struct Sock
		{

			////////////////////////////////////////////////
			// Required interface
			////////////////////////////////////////////////

			// cancel all pending operations and prevent any future operations
			// from succeeding.
			void close();

			// This returns an Awaiter that will be used to start the async send
			// and manage its execution. As discussed above, Awaiter has three 
			// methods: await_ready(), await_suspend(...), await_resume(). These
			// get called in that order. await_suspend will start the actual
			// async operation. await_resume will return the result of the async 
			// operation. send takes as input the data to be sent, and optionally
			// a stop_token. The stop_token allows the user to tell the socket
			// that this operation should be canceled. See Awaiter for more details.
			Awaiter send(span<u8> data, macoro::stop_token token = {}) { return Awaiter(this, data, true, std::move(token)); };

			// This returns an Awaiter that will be used to start the async receive
			// and manage its execution. As discussed above, Awaiter has three 
			// methods: await_ready(), await_suspend(...), await_resume(). These
			// get called in that order. await_suspend will start the actual
			// async operation. await_resume will return the result of the async 
			// operation. recv takes as input the data to be received, and optionally
			// a stop_token. The stop_token allows the user to tell the socket
			// that this operation should be canceled. See Awaiter for more details.
			Awaiter recv(span<u8> data, macoro::stop_token token = {}) { return Awaiter(this, data, false, std::move(token)); };

			////////////////////////////////////////////////
			// internal implementation
			////////////////////////////////////////////////

			// We construct a Sock using the index of this Sock (0/1)
			// and the shared state.
			Sock(u64 i, std::shared_ptr<SharedState> state)
				: mIdx(i)
				, mImpl(state)
			{}

			// a zero or one value indicating which end of the socket we are.
			u64 mIdx = ~0ull;

			// the shared state between the two ends of the socket.
			std::shared_ptr<SharedState> mImpl;

			// helper function to get/set this socket's error_code
			error_code& ec() { return mImpl->mEc[mIdx]; }

			// this returns a function that can be used to artificially inject
			// errors into the socket. This is useful to unit testing to 
			// make sure a protocol behaves well regardless of when a networking
			// error might occur.
			unique_function<error_code()>& errFn() { return mImpl->mDebugErrorInjector; }

			// returns the OpPair that will hold the send operation
			auto& outbound() { return mImpl->mInbound[mIdx ^ 1]; }

			// returns the OpPair that will hold the receive operation
			auto& inbound() { return mImpl->mInbound[mIdx]; }

		};

		// the shared state that the two ends communicate by
		struct SharedState
		{
			// a mutex to ensure no data races
			std::mutex mMtx;

			// An optional function that can be used to inject artificial
			// network errors. If set, this function is called for each
			// operation and if it returns an error, that error will be 
			// propagated to the caller.
			unique_function<error_code()> mDebugErrorInjector;

			// The current error state of the two ends of the socket.
			std::array<error_code, 2> mEc = { code::success , code::success };

			// The two sets of operations that the socket can have. 
			std::array<OpPair, 2> mInbound;

			// The two sockets.
			std::array<Sock*, 2> mSocks;
		};

		// The pointer to the actual socket implementation. 
		// The lifetime of this object is managed by Socket.
		Sock* mSock;

		// A helper function to make a pair of LocalAsyncSocket
		// that can communicate with each other.
		static std::array<LocalAsyncSocket, 2> makePair()
		{
			std::array<LocalAsyncSocket, 2> pair;
			auto state = std::make_shared<SharedState>();
			auto s0 = std::unique_ptr<Sock>(new Sock(0, state));
			auto s1 = std::unique_ptr<Sock>(new Sock(1, state));


			pair[0].mSock = (Sock*)s0.get();
			pair[1].mSock = (Sock*)s1.get();
			state->mSocks[0] = pair[0].mSock;
			state->mSocks[1] = pair[1].mSock;

			*static_cast<Socket*>(&pair[0]) = Socket(make_socket_tag{}, std::move(s0));
			*static_cast<Socket*>(&pair[1]) = Socket(make_socket_tag{}, std::move(s1));

			return pair;
		}
	};


	inline LocalAsyncSocket::Awaiter::Awaiter(Sock* ss, span<u8> dd, bool send, macoro::stop_token&& t)
		: mSock(ss)
		, mData(dd)
		, mTotalSize(dd.size())
		, mType(send ? Type::send : Type::recv)
		, mToken(t)
	{
		COPROTO_ASSERT(dd.size());
	}

	inline error_code& LocalAsyncSocket::Awaiter::ec() { return mSock->ec(); }
	inline unique_function<error_code()>& LocalAsyncSocket::Awaiter::errFn() { return mSock->errFn(); }
	inline LocalAsyncSocket::OpPair& LocalAsyncSocket::Awaiter::outbound() { return mSock->outbound(); }
	inline LocalAsyncSocket::OpPair& LocalAsyncSocket::Awaiter::inbound() { return mSock->inbound(); }

	inline void LocalAsyncSocket::Sock::close()
	{
		std::array<coroutine_handle<>, 4> cbs;

		{
			std::unique_lock<std::mutex> lock(mImpl->mMtx);
			//outLog().emplace_back("close", std::this_thread::get_id(), 0);
			//inLog().emplace_back("close", std::this_thread::get_id(), 0);

			if(!ec())
				ec() = code::closed;

			auto& ec2 = mImpl->mEc[mIdx ^ 1];
			if (!ec2)
				ec2 = code::remoteClosed;

			if (inbound().mRecv)
			{
				auto op = std::exchange(inbound().mRecv, nullptr);
				COPROTO_ASSERT(op && op->mHandle && !op->mEc);
				op->mEc = code::closed;
				cbs[0] = op->mHandle;
			}
			if (inbound().mSend)
			{
				auto op = std::exchange(inbound().mSend, nullptr);
				COPROTO_ASSERT(op && op->mHandle && !op->mEc);
				op->mEc = code::remoteClosed;
				cbs[1] = op->mHandle;
			}

			if (outbound().mRecv)
			{
				auto op = std::exchange(outbound().mRecv, nullptr);
				COPROTO_ASSERT(op && op->mHandle && !op->mEc);
				op->mEc = code::remoteClosed;
				cbs[2] = op->mHandle;
			}

			if (outbound().mSend)
			{
				auto op = std::exchange(outbound().mSend, nullptr);
				COPROTO_ASSERT(op && op->mHandle && !op->mEc);
				op->mEc = code::closed;
				cbs[3] = op->mHandle;
			}
		}

		for (auto cb : cbs)
		{
			if (cb)
				cb.resume();
		}
	}

	inline coroutine_handle<> LocalAsyncSocket::Awaiter::await_suspend(coroutine_handle<> h)
	{
		// if op is set, then we will complete synchronously.
		OpPair* op = nullptr;

		// The two callbacks that need to be "called" when we complete
		// an operation. One for both ends of the socket.
		coroutine_handle<> c0, c1;

		// The two stop_callbacks that need to be cleared in the
		// event that we complete the operation.
		macoro::optional_stop_callback* r0 = nullptr, * r1 = nullptr;

		// The source and destination buffers of the operation.
		span<u8> src, dst;

		// First we need to check it the stop token can be used.
		if (mToken.stop_possible())
		{
			// if so then register the cancellation callback.
			mReg.emplace(mToken, [this] {

				// This might be called synchronously if the operation has 
				// already been canceled.
				coroutine_handle<> cb;
				{
					std::unique_lock<std::mutex> lock(mSock->mImpl->mMtx);
					if (!mEc)
					{
						// If we dont already have an error, then cancel any pending
						// operation and record the callback. If we called this synchronously
						// then mHandle has not yet been set.
						cb = mHandle;
						if (mType == Type::send)
						{
							assert(mSock->outbound().mSend == (cb ? this : nullptr));
							mSock->outbound().mSend = nullptr;
						}
						else
						{
							assert(mSock->inbound().mRecv == (cb ? this : nullptr));
							mSock->inbound().mRecv = nullptr;
						}

						mEc = code::operation_aborted;
					}
				}

				// resume the callback outside the lock...
				if (cb)
					cb.resume();
				});
		}


		// Note that we might have just canceled our operation.
		{

			std::unique_lock<std::mutex> lock(mSock->mImpl->mMtx);

			// record the handle.
			mHandle = h;

			// check if we canceled synchronously, if so we can return 
			// h to tell the caller that the operation has completed
			// and they can resume.
			if (mEc)
			{
				return h;
			}

			// This is an optional feature where if the socket has no
			// error and it has an "error function" errFn, then we
			// will call errFn and inject artificial errors. This
			// is used for unit testing a protocol.
			if (!ec() && errFn())
			{
				ec() = errFn()();
			}

			// If the Socket itself has an error, this operation will
			//complete with that error.
			if (ec())
			{
				mEc = ec();
				return h;
			}
			else
			{
				// Check if we can complete synchronously. That is, if there
				// is a operation on the other end of the socket that will 
				// complete our current operation.
				if (mType == Type::send)
				{
					//log = &outLog();
					//outLog().emplace_back("await_suspend send", std::this_thread::get_id(), *(u64*)mData.data());

					auto& out = outbound();
					assert(!out.mSend);
					out.mSend = this;

					// is there a recv to match our send?
					if (out.mRecv)
						op = &out;
				}
				else
				{
					//log = &inLog();
					//inLog().emplace_back("await_suspend recv", std::this_thread::get_id(), 0);

					auto& in = inbound();
					assert(!in.mRecv);
					in.mRecv = this;

					// is there a send to match our recv?
					if (in.mSend)
						op = &in;;
				}
			}


			// op is set when we can complete the operation.
			// If so we will do some booking to extract
			//
			// * src,dst : the buffers
			// * c0,c1 : the callbacks
			// * r0,r1 : the stop callbacks.
			if (op)
			{
				auto& mRecv = op->mRecv;
				auto& mSend = op->mSend;
				//log->emplace_back("await_suspend complete", std::this_thread::get_id(), *(u64*)mSend->mData.data());

				COPROTO_ASSERT(mSend);
				COPROTO_ASSERT(mRecv);
				COPROTO_ASSERT(mRecv->mType == Awaiter::Type::recv);
				COPROTO_ASSERT(mSend->mType == Awaiter::Type::send);

				auto min = std::min<u64>(mSend->mData.size(), mRecv->mData.size());
				COPROTO_ASSERT(min);

				src = mSend->mData.subspan(0, min);
				dst = mRecv->mData.subspan(0, min);

				mSend->mData = mSend->mData.subspan(min);
				mRecv->mData = mRecv->mData.subspan(min);

				if (mRecv->mData.size() == 0)
				{
					mRecv->mEc = code::success;
					assert(mRecv->mHandle);
					c0 = mRecv->mHandle;
					if (mRecv->mReg)
						r0 = &mRecv->mReg;
					mRecv = nullptr;
				}

				if (mSend->mData.size() == 0)
				{
					mSend->mEc = code::success;
					c1 = mSend->mHandle;
					if (mSend->mReg)
						r1 = &mSend->mReg;
					mSend = nullptr;
				}

			}
			//else
			//	log->emplace_back("await_suspend incomplete", std::this_thread::get_id(), 0);
		}

		// outside the lock we will perform 
		// 
		// * the data copy 
		// * clear the stop_callbacks. This must be done outside the locks as
		//   there might be someone in the stop_callback that is blocked on the
		//   lock. To prevent a race, we will/must block until they are out of 
		//   the stop_callback and therefore we must release out lock before
		//   clearing the stop_callback.
		// * call the callbacks. Clearly this should not be done while holding 
		//   the lock. Note that we explicitly call one of the callbacks
		//   while the other is returned. Returning a callback is equivalent to
		//   calling it ourselves but is more efficient. Google symmetric transfer
		//   for more details.
		// 
		if (op)
		{

			memcpy(dst.data(), src.data(), src.size());

			if (c0 && c1)
			{
				// stop the cancellation callbacks.
				if (r0)
					r0->reset();
				if (r1)
					r1->reset();

				// resume the coroutines
				(h == c0 ? c1 : c0).resume();
				return h;
			}
			else if (c0)
			{
				// stop the cancellation callback.
				if (r0)
					r0->reset();

				// resume the coroutine
				return c0;

			}
			else
			{
				assert(c1);
				if (r1)
					r1->reset();

				return c1;
			}
		}


		// if we dont complete synchronously, then return noop_coroutine 
		// to tell the framework we dont have more work to do.
		return macoro::noop_coroutine();
	}

}
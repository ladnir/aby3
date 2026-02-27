#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "coproto/Common/Defines.h"
#include "coproto/Common/TypeTraits.h"
#include "coproto/Common/span.h"
#include "coproto/Common/InlinePoly.h"
#include "coproto/Common/error_code.h"
#include "coproto/Common/Function.h"
#include "coproto/Common/macoro.h"


#include "coproto/Socket/SocketScheduler.h"

namespace coproto
{


	struct BadReceiveBufferSize : public std::system_error
	{
		u64 mBufferSize, mReceivedSize;

		BadReceiveBufferSize(u64 bufferSize, u64 receivedSize)
			: std::system_error(code::badBufferSize,
				std::string(
					"\nlocal buffer size:   " + std::to_string(bufferSize) +
					" bytes\ntransmitted size: " + std::to_string(receivedSize) +
					" bytes\n"
				))
			, mBufferSize(bufferSize)
			, mReceivedSize(receivedSize)
		{}
	};

	namespace internal
	{


		template<typename Container>
		struct MvSendBuffer : public SendOp
		{
			Container mCont;

			MvSendBuffer(Container&& c)
				:mCont(std::forward<Container>(c))
			{
			}

			span<u8> asSpan() override
			{
				return ::coproto::internal::asSpan(mCont);
			}

			void setError(std::exception_ptr e) override
			{
			}

		};



		struct SendProtoBase 
		{
			SockScheduler* mSock = nullptr;
			SessionID mId;
			std::exception_ptr mExPtr;
			macoro::stop_token mToken;

			SendProtoBase(SockScheduler* s, SessionID sid, macoro::stop_token&& token)
				:mSock(s)
				,mId(sid)
				,mToken(std::move(token))
			{}
			virtual SendBuffer getBuffer() = 0;

			bool await_ready() {
				return false;
			}

#ifdef COPROTO_CPP20

			std::coroutine_handle<> await_suspend(std::coroutine_handle<> h);
#endif
			coroutine_handle<> await_suspend(coroutine_handle<> h);

		};

		class RecvProtoBase :  public RecvBuffer
		{
		public:
			SockScheduler* mSock = nullptr;
			SessionID mId;
			macoro::stop_token mToken;

			RecvProtoBase(SockScheduler* s, SessionID sid, macoro::stop_token&& token)
				: mSock(s)
				, mId(sid)
				, mToken(std::move(token))
			{}

			RecvBuffer* getBuffer() {
				return this;
			}

			bool await_ready() { return false; }
#ifdef COPROTO_CPP20
			std::coroutine_handle<> await_suspend(std::coroutine_handle<> h);
#endif
			coroutine_handle<> await_suspend(coroutine_handle<> h);

		};

		template<typename Container, bool allowResize>
		class MoveRecvProto : public RecvProtoBase
		{
		public:
			Container mContainer;

			MoveRecvProto(SockScheduler* s, SessionID id, Container&& c, macoro::stop_token&& token)
				: RecvProtoBase(s, id, std::move(token))
				, mContainer(std::forward<Container>(c))
			{}
			MoveRecvProto(SockScheduler* s, SessionID id, macoro::stop_token&& token)
				: RecvProtoBase(s, id, std::move(token))
			{}
			MoveRecvProto() = default;
			MoveRecvProto(MoveRecvProto&& m)
				: RecvProtoBase(std::move((RecvProtoBase&)m))
				, mContainer(std::move(m.mContainer))
			{
			}
			MoveRecvProto(const MoveRecvProto&) = delete;
			span<u8> asSpan(u64 size) override
			{

				auto buff = tryResize(size, mContainer, allowResize);
				if (buff.size() != size)
				{
					mExPtr = std::make_exception_ptr(BadReceiveBufferSize(buff.size(), size));
				}
				return buff;
			}

			Container await_resume()
			{
				if (mExPtr)
					std::rethrow_exception(mExPtr);

				return std::move(mContainer);
			}
		};

		template<typename Container, bool allowResize = true>
		class RefRecvProto : public RecvProtoBase
		{
		public:
			Container& mContainer;

			RefRecvProto(SockScheduler* s,SessionID id, Container& t, macoro::stop_token&& token)
				: RecvProtoBase(s, id, std::move(token))
				, mContainer(t)
			{
#ifdef COPROTO_LOGGING
				setName("recv_" + std::to_string(gProtoIdx++));
#endif
			}

			span<u8> asSpan(u64 size) override
			{

				auto buff = tryResize(size, mContainer, allowResize);
				if (buff.size() != size)
				{
					mExPtr = std::make_exception_ptr(BadReceiveBufferSize(buff.size(), size));
				}
				return buff;
			}

			void await_resume() 
			{
				
				if (mExPtr)
					std::rethrow_exception(mExPtr);
			}
		};


		template<typename Container>
		class RefSendProto : public SendProtoBase, public SendOp
		{
		public:
			Container& mContainer;

			span<u8> asSpan() override
			{
				return ::coproto::internal::asSpan(mContainer);
			}

			void setError(std::exception_ptr e) override
			{
				mExPtr = e;
			}

			RefSendProto(SockScheduler* s, SessionID id, Container& t, macoro::stop_token&& token)
				: SendProtoBase(s, id, std::move(token))
				, mContainer(t)
			{
#ifdef COPROTO_LOGGING
				setName("send_" + std::to_string(gProtoIdx++));
#endif
			}

			SendBuffer getBuffer() override final
			{
				SendBuffer ret;
				ret.mStorage.setBorrowed(this);
				//ret.mStorage.emplace<RefSendBuffer<Container>>(mContainer);
				return ret;
			}

			void await_resume()
			{
				if (mExPtr)
					std::rethrow_exception(mExPtr);
			}
		};


		template<typename Container>
		class MoveSendProto : public SendProtoBase
		{
		public:
			Container mContainer;

			MoveSendProto(SockScheduler* s, SessionID id,  Container&& t, macoro::stop_token&& token)
				: SendProtoBase(s, id, std::move(token))
				, mContainer(std::move(t))
			{
#ifdef COPROTO_LOGGING
				setName("send_" + std::to_string(gProtoIdx++));
#endif
			}


			SendBuffer getBuffer() override final
			{
				SendBuffer ret;
				ret.mStorage.emplace<MvSendBuffer<Container>>(std::move(mContainer));
				return ret;
			}

			template<typename P>
			coroutine_handle<> await_suspend(const coroutine_handle<P>&h)
			{
				
				if (internal::asSpan(mContainer).size() == 0)
				{
					mExPtr = std::make_exception_ptr(std::system_error(code::sendLengthZeroMsg));
					return h;
				}
				else
				{
					return mSock->send(mId, getBuffer(), h, std::move(mToken), true);
				}
			}

#ifdef COPROTO_CPP20
			template<typename P>
			std::coroutine_handle<> await_suspend(const std::coroutine_handle<P>& h)
			{
				return await_suspend(static_cast<coroutine_handle<>>(h)).std_cast();
			}
#endif


			void await_resume()
			{
				if (mExPtr)
					std::rethrow_exception(mExPtr);
			}
		};


		class Flush
		{
		public:
			SockScheduler* mSock;
			Flush(SockScheduler* s)
				:mSock(s)
			{}

			bool await_ready() { return false; }
			coroutine_handle<> await_suspend(coroutine_handle<> h)
			{
				return mSock->flush(h);
			}

			void await_resume() {};
		};


#ifdef COPROTO_CPP20
		inline std::coroutine_handle<> SendProtoBase::await_suspend(std::coroutine_handle<> h)
		{
			return mSock->send(mId, getBuffer(), coroutine_handle<>(h), std::move(mToken)).std_cast();
		}
#endif
		inline coroutine_handle<> SendProtoBase::await_suspend(coroutine_handle<> h)
		{
			return mSock->send(mId, getBuffer(), h, std::move(mToken));
		}
#ifdef COPROTO_CPP20
		inline std::coroutine_handle<> RecvProtoBase::await_suspend(std::coroutine_handle<> h)
		{
			return mSock->recv(mId, this, coroutine_handle<>(h), std::move(mToken)).std_cast();
		}
#endif
		inline coroutine_handle<> RecvProtoBase::await_suspend(coroutine_handle<> h)
		{
			return mSock->recv(mId, this, h, std::move(mToken));
		}

	}
}


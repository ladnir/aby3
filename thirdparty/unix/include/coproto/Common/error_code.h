#pragma once
// © 2022 Visa.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include  <system_error>



namespace coproto
{
	enum class code
	{
		//The io operation completed successfully;
		success = 0,
		// The operation has been suspended.
		suspend,
		cancel,
		//remoteCancel,

		closed,
		remoteClosed,

		operation_aborted,

		//The io operation failed and the caller should abort;
		ioError,
		// bad buffer size. The reciever's buffer size does not match the number of bytes sent.
		badBufferSize,
		// sending a zero length message is not allowed.
		sendLengthZeroMsg,
		// The requested operation for not support async sockets.
		noAsyncSupport,
		//An uncaught exception was thrown during the protocol. ";
		uncaughtException,
		invalidArguments,
		endOfRound,
		uninitialized,

		badCoprotoMessageHeader,

		// 
		DEBUG_ERROR

	};
}
namespace std {
	template <>
	struct is_error_code_enum<coproto::code> : true_type {};
}

namespace coproto
{
	using error_code = std::error_code;


	static const std::error_category& coproto_cat = error_code(code{}).category();

	error_code make_error_code(code e);

	inline error_code as_error_code(std::exception_ptr p)
	{
		try
		{
			std::rethrow_exception(p);
		}
		catch (std::system_error& e)
		{
			return e.code();
		}
		catch (...)
		{
			return code::uncaughtException;
		}
	}

}
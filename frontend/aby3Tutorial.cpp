#include <cryptoTools/Network/IOService.h>

#include "aby3/sh3/Sh3Runtime.h"
#include "aby3/sh3/Sh3Encryptor.h"
#include "aby3/sh3/Sh3Evaluator.h"

using namespace oc;
using namespace aby3;

// This function sets up the basic classes that we will 
// use to perform some computation. This mostly consists
// of creating Channels (network sockets) to the other 
// parties and then establishing some shared randomness.
void setup(
	u64 partyIdx,
	IOService& ios,
	Sh3Encryptor& enc,
	Sh3Evaluator& eval,
	Sh3Runtime& runtime)
{
	// A CommPkg is a pair of Channels (network sockets) to the other parties.
	// See cryptoTools\frontend_cryptoTools\Tutorials\Network.cpp 
	// for detials.
	CommPkg comm;
	switch (partyIdx)
	{
	case 0:
		comm.mNext = Session(ios, "127.0.0.1:1313", SessionMode::Server, "01").addChannel();
		comm.mPrev = Session(ios, "127.0.0.1:1313", SessionMode::Server, "02").addChannel();
		break;
	case 1:
		comm.mNext = Session(ios, "127.0.0.1:1313", SessionMode::Server, "12").addChannel();
		comm.mPrev = Session(ios, "127.0.0.1:1313", SessionMode::Client, "01").addChannel();
		break;
	default:
		comm.mNext = Session(ios, "127.0.0.1:1313", SessionMode::Client, "02").addChannel();
		comm.mPrev = Session(ios, "127.0.0.1:1313", SessionMode::Client, "12").addChannel();
		break;
	}

	// in a real work example, where parties 
	// have different IPs, you have to give the 
	// Clients the IP of the server and you give
	// the servers their own IP (to listen to).


	// Establishes some shared randomness needed for the later protocols
	enc.init(partyIdx, comm, sysRandomSeed());

	// Establishes some shared randomness needed for the later protocols
	eval.init(partyIdx, comm, sysRandomSeed());

	// Copies the Channels and will use them for later protcols.
	runtime.init(partyIdx, comm);
}


void integerOperations(u64 partyIdx)
{
	// The first thing we always do is to create and
	// IO Service. This will help us perform networking.
	// We must ensure that this object outlives all of
	// out protocols.
	IOService ios;

	// Next we will create our three main classes. 
	// Sh3Encryptor allows us to generate and reconstruct
	// secret shared values. 
	// Sh3Evaluator will allow us to perform some of the 
	// most common interactive protocols, e.g. multiplication.
	// Sh3Runtime will take care of the networking and help
	// schedule operations in parallel (for free :).
	Sh3Encryptor enc;
	Sh3Evaluator eval;
	Sh3Runtime runtime;

	// Next we will initialize these classes. See setup(...)
	// for details.
	setup(partyIdx, ios, enc, eval, runtime);

	// The main way to use ABY3 requires a basic understanding
	// of asynchonous programming. Lets start with a simple example.

	// Here we want party 0 to input a secret shared integer.
	// This basic type for this is a si64 (secret shared 64-bit 
	// signed integer). 
	si64 sharedInt;

	// At this point it is uninitialized. To put a value into it, 
	// the parties must run a protocol. This is done as follows.

	Sh3Task inputTask;
	if (partyIdx == 0)
	{
		// we will have party 0 provide the value
		inputTask = enc.localInt(runtime, 42, sharedInt);
	}
	else
	{
		// the other parties must help party 0 by calling remoteInt.
		inputTask = enc.remoteInt(runtime, sharedInt);
	}

	// However, at this point nothing has actually happen. sharedInt
	// has not been populated with the value of 42. To actually make 
	// this happen with must call get() on the return value of these
	// functions. That is,

	inputTask.get();

	// It is now the case that sharedInt == 42.

	// For less typing, we can also do the same thing by typing
	if (partyIdx == 0)
		enc.localInt(runtime, 21, sharedInt).get();
	else 
		enc.remoteInt(runtime, sharedInt).get();
		
	// Not that we called .get() on the return type. This will the
	// runtime that we want the value right now.

	// You might be wondering why we would want to wait to get the 
	// value secret shared. This is because we secret sharing a value
	// requires the parties to interact (send messages) which takes
	// time. If we want to secret share many values, e.g.

	std::vector<si64> sharedVec(20);

	// Then it would take several round trips to compute
	for (u64 i = 0; i < sharedVec.size(); ++i)
	{
		if (i % 3 == partyIdx)
			enc.localInt(runtime, i, sharedVec[i]).get();
		else
			enc.remoteInt(runtime, sharedVec[i]).get();
	}
	// This is because (more or less) each one of these 
	// operations must be completed before the next can 
	// start. This mean that who ever is inputting the 
	// value must sends shares of the value to the other
	// who must then wait. This then happens for the next 
	// value.

	// However, if we instead computed it as
	Sh3Task task;
	for (u64 i = 0; i < sharedVec.size(); ++i)
	{
		if (i % 3 == partyIdx)
			task &= enc.localInt(runtime, i, sharedVec[i]);
		else
			task &= enc.remoteInt(runtime, sharedVec[i]);
	}

	task.get();

	// Then all of these operations get performed in parallel.
	// This is critical to understand in order to acheive good
	// performance.

	// Note that we AND'ed together several tasks. This results
	// the task.get() meaning that you want to get the result
	// of all of the tasks.


	// Now that we have some shared values, lets do some computation!

	// We can add shares together with little to no costs.
	si64 sum = sharedVec[0];
	for (u64 i = 1; i < sharedVec.size(); ++i)
		sum = sum + sharedVec[i];

	// However, multiplication is a bit more involved due to it
	// requiring communication. The same type of operations would 
	// look like
	si64 prod = sharedVec[0];
	task = runtime.noDependencies();
	for (u64 i = 1; i < sharedVec.size(); ++i)
		task = eval.asyncMul(task, prod, sharedVec[i], prod);

	task.get();

	// Here we are computing the product of the values in sharedVec.
	// First we computed (sharedVec[0] * sharedVec[1]). We then take
	// the result and multipluply it with sharedVec[2] and so on. Each
	// mutltiplication depends on the previous and therefore we can 
	// express this by passing the dependent task as input to the next
	// multiplication. This tell the runtime to wait until the previous 
	// multiplication is completed before performing the next.

	// Alternatively, we could have done the same thing as
	prod = sharedVec[0];
	for (u64 i = 1; i < sharedVec.size(); ++i)
		eval.asyncMul(runtime, prod, sharedVec[1], prod).get();

	// However, this would not have allowed us to run other operations
	// in "parallel". In this case it does not matter but sometime we 
	// have parallel work that could be performed. 

	// To reconstruct a result, we can do this by calling 
	// Sh3Evaluator::revealAll(...).
	i64 prodVal;
	enc.revealAll(runtime, prod, prodVal).get();

	// This will reveal the value to all of the parties, we can 
	// alternative reveal it to one party by 
	if (partyIdx == 0)
	{
		i64 sumVal;
		enc.reveal(runtime, sum, sumVal).get();
	}
	else
		enc.reveal(runtime, 0, sum).get();

}


void matrixOperations(u64 partyIdx)
{
	IOService ios;
	Sh3Encryptor enc;
	Sh3Evaluator eval;
	Sh3Runtime runtime;
	setup(partyIdx, ios, enc, eval, runtime);

	// In addition to working with individual integers,
	// ABY3 directly supports performing matrix 
	// multiplications. Matrix operations are more efficient
	// for several reasons. First, there is less overhead from 
	// the runtime and second we can use more efficient protocols
	// in many cases. See the ABY3 paper for details.

	// A plaintext matrix can be instantiated as
	u64 rows = 10,
		cols = 4;
	eMatrix<i64> plainMatrix(rows, cols);

	// We can populate is by 
	for (u64 i = 0; i < rows; ++i)
		for (u64 j = 0; j < cols; ++j)
			plainMatrix(i, j) = i + j;

	// To encrypt it, we use 
	si64Matrix sharedMatrix(rows, cols);
	if (partyIdx == 0)
		enc.localIntMatrix(runtime, plainMatrix, sharedMatrix).get();
	else
		enc.remoteIntMatrix(runtime, sharedMatrix).get();


	// We can add locally
	si64Matrix addition = sharedMatrix + sharedMatrix;

	// We can multiply
	si64Matrix prod;
	Sh3Task mulTask = eval.asyncMul(runtime, sharedMatrix, addition, prod);

	// we can reconstruct the secret shares
	enc.revealAll(mulTask, prod, plainMatrix).get();
}

void fixedPointOperations(u64 partyIdx)
{
	IOService ios;
	Sh3Encryptor enc;
	Sh3Evaluator eval;
	Sh3Runtime runtime;
	setup(partyIdx, ios, enc, eval, runtime);

	// The framework also supports the ability to perform 
	// fixed point computation. This is similar to the
	// double or float type in c++. The key difference is 
	// that it is implemented as an integer where a fixed 
	// number of the bits represent decimal/fraction part.

	// This represent a plain 64-bit value where the bottom  
	// 8-bit of the integer represent the fractional part.
	f64<D8> fixedInt = 34.62;

	// We can encrypt this value in the similar way as an integer
	sf64<D8> sharedFixedInt;
	if (partyIdx == 0)
		enc.localFixed(runtime, fixedInt, sharedFixedInt).get();
	else
		enc.remoteFixed(runtime, sharedFixedInt).get();

	// We can add and multiply
	sf64<D8> addition = sharedFixedInt + sharedFixedInt;
	sf64<D8> prod;
	eval.asyncMul(runtime, addition, sharedFixedInt, prod).get();

	// We can also perform matrix operations.
	u64 rows = 10,
		cols = 4;
	f64Matrix<D8> fixedMatrix(rows, cols);

	// We can populate is by 
	for (u64 i = 0; i < rows; ++i)
		for (u64 j = 0; j < cols; ++j)
			fixedMatrix(i, j) = double(i) / j ;

	// To encrypt it, we use 
	sf64Matrix<D8> sharedMatrix(rows, cols);
	if (partyIdx == 0)
		enc.localFixedMatrix(runtime, fixedMatrix, sharedMatrix).get();
	else
		enc.remoteFixedMatrix(runtime, sharedMatrix).get();

	// We can add locally
	sf64Matrix<D8> additionMtx = sharedMatrix + sharedMatrix;

	// We can multiply
	sf64Matrix<D8> prodMtx;
	Sh3Task mulTask = eval.asyncMul(runtime, sharedMatrix, additionMtx, prodMtx);

	// we can reconstruct the secret shares
	enc.revealAll(mulTask, prodMtx, fixedMatrix).get();
}

//////////////////////////////////////////////////////////
// How things are implemented:
//////////////////////////////////////////////////////////



//  Lets take the logistic function as an example https://github.com/ladnir/aby3/blob/master/aby3-ML/Regression.h#L218
//  
//  All three parties will call this function on their own thread or program.It does the following :
//  
//  1) Selects a mini - batch based on a common random number generator.https://github.com/ladnir/aby3/blob/master/aby3-ML/Regression.h#L251
//  2) Multiply the features with the linear model https://github.com/ladnir/aby3/blob/master/aby3-ML/Regression.h#L262
//  3) Apply the activation function(logistic).https://github.com/ladnir/aby3/blob/master/aby3-ML/Regression.h#L263
//  4) Compute the backpropagation.https://github.com/ladnir/aby3/blob/master/aby3-ML/Regression.h#L268
//  5) update the model https://github.com/ladnir/aby3/blob/master/aby3-ML/Regression.h#L279
//  6) go back to #1
//  
//  Each of these tasks is performed right when they are called. 
//  Each of these calls will internally schedule some aby3 tasks 
//  and then perform them.
//  
//  For example, Multiplying the features(#2) requires one round 
//  of interaction which consists of two aby3 tasks : (a)sending 
//  the first multiplication message, (b)receiving this message 
//  from the other partiesand computing the output message.This 
//  class https://github.com/ladnir/aby3/blob/master/aby3-ML/aby3ML.h#L12 
//  is what schedules and executes these tasks. Multiplication is 
//  shown here https://github.com/ladnir/aby3/blob/master/aby3-ML/aby3ML.h#L106. 
//  It schedules the tasks (a,b) by calling asyncMul(...) and then 
//  executes it by calling .get() on the resulting task. 
//  
//  asyncMul(...) can be found here https://github.com/ladnir/aby3/blob/master/aby3/sh3/Sh3Evaluator.cpp#L442 
//  It works as follows: it takes the dependency task and then adds
//  a new task that should be performed once the dependent task is
//  done. This is performed by passing a function to the .then(...) 
//  function. This first task (a) begins on line 449 and on line 520. 
//  When this task is performed, it schedules another task (b) as shown
// on line 502. This task completes the multiplication.
//  
//  Notice that the overall asyncMul task should complete only when 
//  this second the task is done.To achieve this, what we do is return 
//  the 'closure' task of the first task.Since the second task was 
//  scheduled using the first as a dependency, the closure will only 
//  be completed when all `downstream` tasks as done.This closure task 
//  is obtained by calling.getClosure() on line 520.
//  
//  Computing the logistic function(#3) is done in the same basic 
//  way but there are many more steps.See the aby3 paper on the 
//  step. The basics is that you evaluate a piecewise polynomial 
//  function which approximates the logistic function.As shown here
//  https://github.com/ladnir/aby3/blob/master/aby3-ML/aby3ML.h#L137 
//  You first describe the piecewise polynomial https://github.com/ladnir/aby3/blob/master/aby3-ML/aby3ML.h#L125 
//  and then evaluate it. There is a general protocol for doing this 
//  and its found here https://github.com/ladnir/aby3/blob/master/aby3/sh3/Sh3Piecewise.cpp#L184 
//  This is performed in the following steps:
//  
//  1) Determine which region / piece of the piecewise polynomial that the input is in.https://github.com/ladnir/aby3/blob/master/aby3/sh3/Sh3Piecewise.cpp#L184
//  2) At the same time, evaluate the each polynomial on the input https://github.com/ladnir/aby3/blob/master/aby3/sh3/Sh3Piecewise.cpp#L276
//  3) Once done with these, multiply the bit indicating whether the input is in the current region with the current polynomial evaluation https://github.com/ladnir/aby3/blob/master/aby3/sh3/Sh3Piecewise.cpp#L300


//  Computing #1 requires 6=log2(64 bits) rounds. Computing #2 is 1 
//  round, computing #3 requires 1 round. The `critical path` is 
//  #1 + #3 which means computing the piecewise poly requires 7 rounds.

//  You can also take a look at the aby3 paper. It is not easy to 
//  tell just by looking at the code since, as you say, one task
//  will then schedule more tasks. Each of these is all split 
//  across many function calls.

//  If a task performs communication, then it will always be performed
//  in the round that follows the `parent` task(s) that it depends on.
//  If it does not perform communication this its performed in the current round.

//  Each party is performed on a single thread. All scheduling and execution 
// of the protocol is performed on a single thread (per party). All tasks are
// executed inside some call to Sh3Task::get(). 

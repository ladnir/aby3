
#include <cryptoTools/Common/CLP.h>

#include "aby3_tests/aby3_tests.h"
#include "com-psi_tests/UnitTests.h"
#include <tests_cryptoTools/UnitTests.h>
#include "cryptoTools/Common/BitIterator.h"
#include "cryptoTools/Common/Timer.h"
#include "cryptoTools/Crypto/PRNG.h"
#include <cryptoTools/Network/IOService.h>
#include <atomic>
#include "com-psi/ComPsiServer.h"
#include "eric.h"

using namespace oc;
std::vector<std::string> unitTestTag{ "u", "unitTest" };





void ComPsi_Intersect(u32 rows, u32 cols = 0)
{

	IOService ios;
	Session s01(ios, "127.0.0.1", SessionMode::Server, "01");
	Session s10(ios, "127.0.0.1", SessionMode::Client, "01");
	Session s02(ios, "127.0.0.1", SessionMode::Server, "02");
	Session s20(ios, "127.0.0.1", SessionMode::Client, "02");
	Session s12(ios, "127.0.0.1", SessionMode::Server, "12");
	Session s21(ios, "127.0.0.1", SessionMode::Client, "12");


	ComPsiServer srvs[3];
	srvs[0].init(0, s02, s01);
	srvs[1].init(1, s10, s12);
	srvs[2].init(2, s21, s20);


	// 80 bits;
	u32 hashSize = 80;


	PRNG prng(ZeroBlock);

	auto keyBitCount = srvs[0].mKeyBitCount;
	std::vector<ColumnInfo>
		aCols = { ColumnInfo{ "key", TypeID::IntID, keyBitCount } },
		bCols = { ColumnInfo{ "key", TypeID::IntID, keyBitCount } };

	for (u32 i = 0; i < cols; ++i)
	{
		//aCols.emplace_back("a" + std::to_string(i), TypeID::IntID, 32);
		bCols.emplace_back("b" + std::to_string(i), TypeID::IntID, 32);
	}

	Table a(rows, aCols)
		, b(rows, bCols);
	auto intersectionSize = (rows + 1) / 2;


	for (u64 i = 0; i < rows; ++i)
	{
		auto out = (i >= intersectionSize);
		for (u64 j = 0; j < a.mColumns[0].mData.cols(); ++j)
		{
			a.mColumns[0].mData(i, j) = i + 1;
			b.mColumns[0].mData(i, j) = i + 1 + (rows * out);
		}
	}

	Timer t;

	bool failed = false;
	auto routine = [&](int i) {
		setThreadName("t0");
		t.setTimePoint("start");


		auto A = (i == 0) ? srvs[i].localInput(a) : srvs[i].remoteInput(0);
		auto B = (i == 0) ? srvs[i].localInput(b) : srvs[i].remoteInput(0);

		if (i == 0)
			t.setTimePoint("inputs");

		if (i == 0)
			srvs[i].setTimer(t);



		SelectQuery query;
		query.noReveal("r");
		auto aKey = query.joinOn(A["key"], B["key"]);
		query.addOutput("key", aKey);

		for (u32 i = 0; i < cols; ++i)
		{
			//query.addOutput("a" + std::to_string(i), query.addInput(A["a" + std::to_string(i)]));
			query.addOutput("b" + std::to_string(i), query.addInput(B["b" + std::to_string(i)]));
		}

		auto C = srvs[i].joinImpl(query);

		if (i == 0)
			t.setTimePoint("intersect");


		if (C.rows())
		{
			aby3::Sh3::i64Matrix c(C.mColumns[0].rows(), C.mColumns[0].i64Cols());
			srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[0], c);

			if (i == 0)
				t.setTimePoint("reveal");
		}
		else
		{
			failed = true;
		}
		//std::cout << t << std::endl << srvs[i].getTimer() << std::endl;
	};

	auto t0 = std::thread(routine, 0);
	auto t1 = std::thread(routine, 1);
	routine(2);

	t0.join();
	t1.join();

	std::cout << "n = " << rows << "\n" << t << std::endl;
	std::cout << "    " << (srvs[0].mComm.mNext.getTotalDataSent() + srvs[0].mRt.mComm.mNext.getTotalDataSent()) << std::endl;
	std::cout << "    " << (srvs[1].mComm.mNext.getTotalDataSent() + srvs[1].mRt.mComm.mNext.getTotalDataSent()) << std::endl;
	std::cout << "    " << (srvs[2].mComm.mNext.getTotalDataSent() + srvs[2].mRt.mComm.mNext.getTotalDataSent()) << std::endl;

	if (failed)
	{
		std::cout << "bad intersection" << std::endl;
		throw std::runtime_error("");
	}
}



void ComPsi_threat(u32 rows, u32 setCount)
{

	IOService ios;
	Session s01(ios, "127.0.0.1", SessionMode::Server, "01");
	Session s10(ios, "127.0.0.1", SessionMode::Client, "01");
	Session s02(ios, "127.0.0.1", SessionMode::Server, "02");
	Session s20(ios, "127.0.0.1", SessionMode::Client, "02");
	Session s12(ios, "127.0.0.1", SessionMode::Server, "12");
	Session s21(ios, "127.0.0.1", SessionMode::Client, "12");


	ComPsiServer srvs[3];
	srvs[0].init(0, s02, s01);
	srvs[1].init(1, s10, s12);
	srvs[2].init(2, s21, s20);


	// 80 bits;
	u32 hashSize = 80;


	PRNG prng(ZeroBlock);

	auto keyBitCount = srvs[0].mKeyBitCount;
	std::vector<ColumnInfo>
		aCols = { ColumnInfo{ "key", TypeID::IntID, keyBitCount } },
		bCols = { ColumnInfo{ "key", TypeID::IntID, keyBitCount } };

	Table a(rows, aCols)
		, b(rows, bCols);
	auto intersectionSize = (rows + 1) / 2;


	for (u64 i = 0; i < rows; ++i)
	{
		auto out = (i >= intersectionSize);
		for (u64 j = 0; j < a.mColumns[0].mData.cols(); ++j)
		{
			a.mColumns[0].mData(i, j) = i + 1;
			b.mColumns[0].mData(i, j) = i + 1 + (rows * out);
		}
	}

	Timer t;

	bool failed = false;
	auto routine = [&](int i) {
		setThreadName("t0");
		if (i == 0)
			t.setTimePoint("start");


		auto A = (i == 0) ? srvs[i].localInput(a) : srvs[i].remoteInput(0);
		auto B = (i == 0) ? srvs[i].localInput(b) : srvs[i].remoteInput(0);
		SharedTable C;

		if (i == 0)
			t.setTimePoint("inputs");

		//if (i == 0)
		//	srvs[i].setTimer(t);

		auto ss = setCount;
		while (ss > 1)
		{

			int loops = ss / 2;

			for (u64 j = 0; j < loops; ++j)
			{
				C = srvs[i].rightUnion(A["key"], B["key"], { A["key"] }, { B["key"] });
			}

			A = C;
			B = C;
			ss -= loops;
		}

		if (i == 0)
			t.setTimePoint("done");

		if (C.rows())
		{
			aby3::Sh3::i64Matrix c(C.mColumns[0].rows(), C.mColumns[0].i64Cols());
			srvs[i].mEnc.revealAll(srvs[i].mRt.mComm, C.mColumns[0], c);

			if (i == 0)
				t.setTimePoint("reveal");
		}
		else
		{
			failed = true;
		}
		//std::cout << t << std::endl << srvs[i].getTimer() << std::endl;
	};

	auto t0 = std::thread(routine, 0);
	auto t1 = std::thread(routine, 1);
	routine(2);

	t0.join();
	t1.join();

	std::cout << "n = " << rows << "\n" << t << std::endl;
	std::cout << "    " << (srvs[0].mComm.mNext.getTotalDataSent() + srvs[0].mRt.mComm.mNext.getTotalDataSent()) << std::endl;
	std::cout << "    " << (srvs[1].mComm.mNext.getTotalDataSent() + srvs[1].mRt.mComm.mNext.getTotalDataSent()) << std::endl;
	std::cout << "    " << (srvs[2].mComm.mNext.getTotalDataSent() + srvs[2].mRt.mComm.mNext.getTotalDataSent()) << std::endl;

	if (failed)
	{
		std::cout << "bad intersection" << std::endl;
		throw std::runtime_error("");
	}
}



i64 Sh3_add_test(u64 n);



void circuit()
{
	oc::BetaLibrary lib;


	auto size = lib.int_int_mult(64, 64, 96);
	auto depth = lib.int_int_mult(64, 64, 96, oc::BetaLibrary::Optimized::Depth);
	size->levelByAndDepth();
	depth->levelByAndDepth();

	std::cout << "size: " << size->mNonlinearGateCount << " " << size->mLevelAndCounts.size() << std::endl;
	std::cout << "size: " << depth->mNonlinearGateCount << " " << depth->mLevelAndCounts.size() << std::endl;
}

void help()
{

	std::cout << "-u                  ~~ to run all tests" << std::endl;
	std::cout << "-u n1 [n2 ...]      ~~ to run test n1, n2, ..." << std::endl;
	std::cout << "-u -list            ~~ to list all tests" << std::endl;
	std::cout << "-intersect -nn NN   ~~ to run the intersection benchmark with 2^NN set sizes" << std::endl;
	std::cout << "-eric -nn NN        ~~ to run the eric benchmark with 2^NN set sizes" << std::endl;
	std::cout << "-threat -nn NN -s S ~~ to run the threat log benchmark with 2^NN set sizes and S sets" << std::endl;
}

int main(int argc, char** argv)
{
	try {


		bool set = false;
		oc::CLP cmd(argc, argv);


		if (cmd.isSet(unitTestTag))
		{
			set = true;
			auto tests = aby3_tests;
			tests += ComPsi_tests;
			//tests += tests_cryptoTools::Tests;

			if (cmd.isSet("list"))
			{
				tests.list();
			}
			else
			{
				cmd.setDefault("loop", 1);
				auto loop = cmd.get<u64>("loop");

				if (cmd.hasValue(unitTestTag))
					tests.run(cmd.getMany<u64>(unitTestTag), loop);
				else
					tests.runAll(loop);
			}
		}
		if (cmd.isSet("eric"))
		{
			set = true;

			auto nn = cmd.getMany<int>("nn");
			if (nn.size() == 0)
				nn.push_back(16);

			for (auto n : nn)
			{
				eric(1 << n);
			}
		}


		if (cmd.isSet("intersect"))
		{
			set = true;

			auto nn = cmd.getMany<int>("nn");
			auto c = cmd.getOr("c", 0);
			if (nn.size() == 0)
				nn.push_back(1 << 16);

			for (auto n : nn)
			{
				auto size = 1 << n;
				ComPsi_Intersect(size, c);
			}
		}


		if (cmd.isSet("threat"))
		{
			set = true;

			auto nn = cmd.getMany<int>("nn");
			auto c = cmd.getOr("s", 2);
			if (nn.size() == 0)
				nn.push_back(1 << 16);

			for (auto n : nn)
			{
				auto size = 1 << n;
				ComPsi_threat(size, c);
			}
		}

		if (cmd.isSet("add"))
		{
			set = true;

			auto nn = cmd.getMany<int>("nn");
			if (nn.size() == 0)
				nn.push_back(1 << 16);

			for (auto n : nn)
			{
				auto size = 1 << n;
				Sh3_add_test(size);
			}
		}

		if (set == false)
		{
			help();
		}

	}
	catch (std::exception& e)
	{
		std::cout << e.what() << std::endl;
	}

	return 0;
}



i64 Sh3_add_test(u64 n)
{
	BetaLibrary lib;
	BetaCircuit* cir = lib.int_int_add(32, 32, 32);


	IOService ios;
	Session s01(ios, "127.0.0.1", SessionMode::Server, "01");
	Session s10(ios, "127.0.0.1", SessionMode::Client, "01");
	Session s02(ios, "127.0.0.1", SessionMode::Server, "02");
	Session s20(ios, "127.0.0.1", SessionMode::Client, "02");
	Session s12(ios, "127.0.0.1", SessionMode::Server, "12");
	Session s21(ios, "127.0.0.1", SessionMode::Client, "12");

	Channel chl01 = s01.addChannel("c");
	Channel chl10 = s10.addChannel("c");
	Channel chl02 = s02.addChannel("c");
	Channel chl20 = s20.addChannel("c");
	Channel chl12 = s12.addChannel("c");
	Channel chl21 = s21.addChannel("c");


	CommPkg comms[3], debugComm[3];
	comms[0] = { chl02, chl01 };
	comms[1] = { chl10, chl12 };
	comms[2] = { chl21, chl20 };

	debugComm[0] = { comms[0].mPrev.getSession().addChannel(), comms[0].mNext.getSession().addChannel() };
	debugComm[1] = { comms[1].mPrev.getSession().addChannel(), comms[1].mNext.getSession().addChannel() };
	debugComm[2] = { comms[2].mPrev.getSession().addChannel(), comms[2].mNext.getSession().addChannel() };

	cir->levelByAndDepth();
	u64 width = n;
	bool failed = false;
	//bool manual = false;
	using namespace aby3;

	enum Mode { Manual, Auto, Replicated };


	auto aSize = cir->mInputs[0].size();
	auto bSize = cir->mInputs[1].size();
	auto cSize = cir->mOutputs[0].size();
	Timer t;
	auto t0 = std::thread([&]() {
		auto i = 0;
		Sh3::i64Matrix a(width, 1), b(width, 1), c(width, 1);
		Sh3::i64Matrix aa(width, 1), bb(width, 1);

		PRNG prng(ZeroBlock);
		for (u64 i = 0; i < a.size(); ++i)
		{
			a(i) = prng.get<i64>();
			b(i) = prng.get<i64>();
		}

		Sh3Runtime rt(i, comms[i]);

		Sh3::sbMatrix A(width, aSize), B(width, bSize), C(width, cSize);

		Sh3Encryptor enc;
		enc.init(i, toBlock(i), toBlock(i + 1));

		auto task = rt.noDependencies();
		t.setTimePoint("start");
		enc.localBinMatrix(rt.noDependencies(), a, A).get();


		task = enc.localBinMatrix(rt.noDependencies(), b, B);

		Sh3BinaryEvaluator eval;

		t.setTimePoint("eval");

		task = eval.asyncEvaluate(task, cir, { &A, &B }, { &C });
		task.get();
		t.setTimePoint("done");

	});

	auto routine = [&](int i)
	{
		PRNG prng;

		Sh3Runtime rt(i, comms[i]);

		Sh3::sbMatrix A(width, aSize), B(width, bSize), C(width, cSize);

		Sh3Encryptor enc;
		enc.init(i, toBlock(i), toBlock((i + 1) % 3));


		auto task = rt.noDependencies();
		// queue up the operations
		enc.remoteBinMatrix(rt.noDependencies(), A).get();
		task = enc.remoteBinMatrix(rt.noDependencies(), B);

		Sh3BinaryEvaluator eval;

		task = eval.asyncEvaluate(task, cir, { &A, &B }, { &C });

		// actually execute the computation
		task.get();

	};

	auto t1 = std::thread(routine, 1);
	auto t2 = std::thread(routine, 2);

	t0.join();
	t1.join();
	t2.join();


	std::cout << "n " << n << "\n" << t << std::endl;

	if (failed)
		throw std::runtime_error(LOCATION);

	return 0;
}
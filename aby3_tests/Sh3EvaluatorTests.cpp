#include "Sh3EvaluatorTests.h"
#include "aby3/sh3/Sh3Evaluator.h"
#include "aby3/sh3/Sh3Encryptor.h"
#include <cryptoTools/Network/Channel.h>
#include <cryptoTools/Network/IOService.h>
#include <cryptoTools/Crypto/PRNG.h>
#include <cryptoTools/Common/BitVector.h>
#include <cryptoTools/Common/TestCollection.h>
#include "aby3/sh3/Sh3FixedPoint.h"
#include <iomanip>
#include "testUtils.h"

using namespace aby3;
using namespace oc;


void Sh3_Evaluator_asyncMul_test()
{

	IOService ios;
	auto chl01 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "01").addChannel();
	auto chl10 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "01").addChannel();
	auto chl02 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "02").addChannel();
	auto chl20 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "02").addChannel();
	auto chl12 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "12").addChannel();
	auto chl21 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "12").addChannel();


	int trials = 10;
	CommPkg comms[3];
	comms[0] = { chl02, chl01 };
	comms[1] = { chl10, chl12 };
	comms[2] = { chl21, chl20 };

	Sh3Encryptor encs[3];
	Sh3Evaluator evals[3];

	encs[0].init(0, toBlock(0, 0), toBlock(0, 1));
	encs[1].init(1, toBlock(0, 1), toBlock(0, 2));
	encs[2].init(2, toBlock(0, 2), toBlock(0, 0));

	evals[0].init(0, toBlock(1, 0), toBlock(1, 1));
	evals[1].init(1, toBlock(1, 1), toBlock(1, 2));
	evals[2].init(2, toBlock(1, 2), toBlock(1, 0));

	bool failed = false;
	auto t0 = std::thread([&]() {
		auto& enc = encs[0];
		auto& eval = evals[0];
		auto& comm = comms[0];
		Sh3Runtime rt;
		rt.init(0, comm);

		PRNG prng(ZeroBlock);

		for (u64 i = 0; i < trials; ++i)
		{
			i64Matrix a(trials, trials), b(trials, trials), c(trials, trials), cc(trials, trials);
			rand(a, prng);
			rand(b, prng);
			si64Matrix A(trials, trials), B(trials, trials), C(trials, trials);

			enc.localIntMatrix(comm, a, A);
			enc.localIntMatrix(comm, b, B);

			auto task = rt.noDependencies();

			for (u64 j = 0; j < trials; ++j)
			{
				task = eval.asyncMul(task, A, B, C);
				task = task.then([&](CommPkg & comm, Sh3Task & self) {
					A = C + A;
					});
			}

			task.get();

			enc.reveal(comm, C, cc);

			for (u64 j = 0; j < trials; ++j)
			{
				c = a * b;
				a = c + a;
			}
			if (c != cc)
			{
				failed = true;
				std::cout << c << std::endl;
				std::cout << cc << std::endl;
			}
		}
		});


	auto rr = [&](int i)
	{
		auto& enc = encs[i];
		auto& eval = evals[i];
		auto& comm = comms[i];
		Sh3Runtime rt;
		rt.init(i, comm);

		for (u64 i = 0; i < trials; ++i)
		{
			si64Matrix A(trials, trials), B(trials, trials), C(trials, trials);
			enc.remoteIntMatrix(comm, A);
			enc.remoteIntMatrix(comm, B);

			auto task = rt.noDependencies();
			for (u64 j = 0; j < trials; ++j)
			{
				task = eval.asyncMul(task, A, B, C);
				task = task.then([&](CommPkg & comm, Sh3Task & self) {
					A = C + A;
					});
			}
			task.get();

			enc.reveal(comm, 0, C);
		}
	};

	auto t1 = std::thread(rr, 1);
	auto t2 = std::thread(rr, 2);

	t0.join();
	t1.join();
	t2.join();

	if (failed)
		throw std::runtime_error(LOCATION);
}





void Sh3_Evaluator_asyncMul_fixed_test()
{

	IOService ios;
	auto chl01 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "01").addChannel();
	auto chl10 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "01").addChannel();
	auto chl02 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "02").addChannel();
	auto chl20 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "02").addChannel();
	auto chl12 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "12").addChannel();
	auto chl21 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "12").addChannel();


	int trials = 1;
	CommPkg comms[3];
	comms[0] = { chl02, chl01 };
	comms[1] = { chl10, chl12 };
	comms[2] = { chl21, chl20 };

	Sh3Encryptor encs[3];
	Sh3Evaluator evals[3];

	encs[0].init(0, toBlock(0, 0), toBlock(0, 1));
	encs[1].init(1, toBlock(0, 1), toBlock(0, 2));
	encs[2].init(2, toBlock(0, 2), toBlock(0, 0));

	evals[0].init(0, toBlock(1, 0), toBlock(1, 1));
	evals[1].init(1, toBlock(1, 1), toBlock(1, 2));
	evals[2].init(2, toBlock(1, 2), toBlock(1, 0));

	bool failed = false;
	auto t0 = std::thread([&]() {
		auto& enc = encs[0];
		auto& eval = evals[0];
		auto& comm = comms[0];
		Sh3Runtime rt;
		rt.init(0, comm);

		PRNG prng(ZeroBlock);

		for (u64 i = 0; i < trials; ++i)
		{

			f64<D8> a = (prng.get<i32>() >> 8) / 100.0;
			f64<D8> b = (prng.get<i32>() >> 8) / 100.0;
			f64<D8> c;

			//std::cout << a.mValue << " * " << b.mValue << " -> " << (a * b).mValue << std::endl;
			//std::cout << a << " * " << b << " -> " << (a * b) << std::endl;

			sf64<D8> A; enc.localFixed(rt, a, A);
			sf64<D8> B; enc.localFixed(rt, b, B).get();
			sf64<D8> C;
			/*
						ostreamLock(std::cout) <<"p" << rt.mPartyIdx <<": " << "a   " << prettyShare(rt.mPartyIdx, A.mShare) << " ~ " << a<< std::endl;
						ostreamLock(std::cout) <<"p" << rt.mPartyIdx <<": " << "b   " << prettyShare(rt.mPartyIdx, B.mShare) << " ~ " << b<< std::endl;
			*/
			auto task = rt.noDependencies();
			//for (u64 j = 0; j < trials; ++j)
			{
				task = eval.asyncMul(task, A, B, C);
				//task = task.then([&](CommPkg& comm, Sh3Task& self) {
				//    A = C + A;
				//});
			}

			f64<D8> cc, aa;
			enc.reveal(task, A, aa).get();
			enc.reveal(task, C, cc).get();


			//for (u64 j = 0; j < trials; ++j)
			{
				c = a * b;
				//a = c + a;
			}
			auto diff0 = (c - cc);
			double diff1 = diff0.mValue / double(1ull << c.mDecimal);
			double diff2 = static_cast<double>(diff0);
			//ostreamLock(std::cout)
			//	<< "p" << rt.mPartyIdx << ": " << "a   " << prettyShare(rt.mPartyIdx, A.mShare) << " ~ " << a << std::endl
			//	<< "p" << rt.mPartyIdx << ": " << "b   " << prettyShare(rt.mPartyIdx, B.mShare) << " ~ " << b << std::endl
			//	<< c << std::endl
			//	<< cc << " (" << cc.mValue << ")" << std::endl
			//	<< diff0.mValue << std::endl
			//	<< diff1 << std::endl
			//	<< diff2 << std::endl;

			if (std::abs(diff1) > 0.01)
			{
				failed = true;
			}

			//ostreamLock(std::cout) << "p" << rt.mPartyIdx << ": c " << prettyShare(rt.mPartyIdx, C.mShare) << " ~ " << cc << " " << c<< std::endl;

		}
		});


	auto rr = [&](int i)
	{
		auto& enc = encs[i];
		auto& eval = evals[i];
		auto& comm = comms[i];
		Sh3Runtime rt;
		rt.init(i, comm);

		for (u64 i = 0; i < trials; ++i)
		{

			sf64<D8> A; enc.remoteFixed(rt, A);
			sf64<D8> B; enc.remoteFixed(rt, B).get();
			sf64<D8> C;

			//ostreamLock(std::cout) <<"p" << rt.mPartyIdx <<": " << "a   " << prettyShare(rt.mPartyIdx, A.mShare) << std::endl;
			//ostreamLock(std::cout) <<"p" << rt.mPartyIdx <<": " << "b   " << prettyShare(rt.mPartyIdx, B.mShare) << std::endl;

			auto task = rt.noDependencies();
			//for (u64 j = 0; j < trials; ++j)
			{
				task = eval.asyncMul(task, A, B, C);
				//task = task.then([&](CommPkg& comm, Sh3Task& self) {
				//    A = C + A;
				//});

			}

			enc.reveal(task, 0, A).get();
			enc.reveal(task, 0, C).get();

			//ostreamLock(std::cout) << "p" << rt.mPartyIdx << ": c " << prettyShare(rt.mPartyIdx, C.mShare)  << std::endl;

		}
	};

	auto t1 = std::thread(rr, 1);
	auto t2 = std::thread(rr, 2);

	t0.join();
	t1.join();
	t2.join();

	if (failed)
		throw std::runtime_error(LOCATION);
}



void Sh3_Evaluator_truncationPai_test(const oc::CLP & cmd)
{


	IOService ios;
	auto chl01 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "01").addChannel();
	auto chl10 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "01").addChannel();
	auto chl02 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "02").addChannel();
	auto chl20 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "02").addChannel();
	auto chl12 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "12").addChannel();
	auto chl21 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "12").addChannel();

	int size = cmd.getOr("s", 4);
	int trials = cmd.getOr("t", 4);
	CommPkg comms[3];
	comms[0] = { chl02, chl01 };
	comms[1] = { chl10, chl12 };
	comms[2] = { chl21, chl20 };

	Sh3Encryptor encs[3];
	Sh3Evaluator evals[3];

	encs[0].init(0, toBlock(0, 0), toBlock(0, 1));
	encs[1].init(1, toBlock(0, 1), toBlock(0, 2));
	encs[2].init(2, toBlock(0, 2), toBlock(0, 0));

	evals[0].init(0, toBlock(1, 0), toBlock(1, 1));
	evals[1].init(1, toBlock(1, 1), toBlock(1, 2));
	evals[2].init(2, toBlock(1, 2), toBlock(1, 0));

	auto dec = Decimal::D8;

	for (u64 t = 0; t < trials; ++t)
	{

		auto t0 = evals[0].getTruncationTuple(size, size, dec);
		auto t1 = evals[1].getTruncationTuple(size, size, dec);
		auto t2 = evals[2].getTruncationTuple(size, size, dec);


		auto small = t0.mRTrunc.mShares[0] + t1.mRTrunc.mShares[0] + t2.mRTrunc.mShares[0];
		auto large = t0.mR + t1.mR + t2.mR;

		for (u64 i = 0; i < small.size(); ++i)
		{
			auto exp = large(i) >> dec;
			if (small(i) != exp)
			{
				std::cout << small(i) << " " << exp << std::endl;
				throw RTE_LOC;
			}
		}

	}
}

void Sh3_Evaluator_asyncMul_matrixFixed_test(const oc::CLP & cmd)
{


	IOService ios;
	auto chl01 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "01").addChannel();
	auto chl10 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "01").addChannel();
	auto chl02 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "02").addChannel();
	auto chl20 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "02").addChannel();
	auto chl12 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "12").addChannel();
	auto chl21 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "12").addChannel();

	int size = cmd.getOr("s", 4);
	int trials = cmd.getOr("t", 4);
	CommPkg comms[3];
	comms[0] = { chl02, chl01 };
	comms[1] = { chl10, chl12 };
	comms[2] = { chl21, chl20 };

	Sh3Encryptor encs[3];
	Sh3Evaluator evals[3];

	encs[0].init(0, toBlock(0, 0), toBlock(0, 1));
	encs[1].init(1, toBlock(0, 1), toBlock(0, 2));
	encs[2].init(2, toBlock(0, 2), toBlock(0, 0));

	evals[0].init(0, toBlock(1, 0), toBlock(1, 1));
	evals[1].init(1, toBlock(1, 1), toBlock(1, 2));
	evals[2].init(2, toBlock(1, 2), toBlock(1, 0));

	bool failed = false;

	auto routine = [&](int idx) {
		auto& enc = encs[idx];
		auto& eval = evals[idx];
		auto& comm = comms[idx];
		Sh3Runtime rt;
		rt.init(idx, comm);




		for (u64 i = 0; i < trials; ++i)
		{
			PRNG prng(toBlock(i));

			f64Matrix<D8> a(size, size);
			f64Matrix<D8> b(size, size);
			f64Matrix<D8> c;

			sf64Matrix<D8> A(size, size);
			sf64Matrix<D8> B(size, size);
			sf64Matrix<D8> C;
			for (u64 i = 0; i < a.size(); ++i) a(i) = (prng.get<u32>() >> 8) / 100.0;
			for (u64 i = 0; i < b.size(); ++i) b(i) = (prng.get<u32>() >> 8) / 100.0;
			c = a * b;
			auto c64 = a.i64Cast() * b.i64Cast();

			A[0].setZero();
			A[1].setZero();
			B[0].setZero();
			B[1].setZero();

			if (idx == 0)
			{
				//lout << "a \n" << a.i64Cast() << std::endl;
				//lout << "b \n" << b.i64Cast() << std::endl;
				//lout << "c \n" << c.i64Cast() << std::endl;
				//lout << "c64 \n" << c64 << std::endl;

				//auto vv = c64(0,1);
				//auto vv2 = a(0, 0).mValue * b(0, 1).mValue +
				//	 	   a(0, 1).mValue * b(1, 1).mValue;

				//lout << a(0,0).mValue << " * " << b(1,0).mValue << "+\n"
				//	<< a(0,1).mValue << " * " << b(1,1).mValue << "\n -> " 
				//	<< c(0,1).mValue << "\n ~ " <<vv << " " << vv2<< " " << (vv >> 8)<< std::endl;
				//lout << a(1) << " * " << b(1) << " -> " << c(1) << std::endl;

				enc.localFixedMatrix(rt, a, A);
				enc.localFixedMatrix(rt, b, B).get();
			}
			else
			{
				enc.remoteFixedMatrix(rt, A);
				enc.remoteFixedMatrix(rt, B).get();
			}

			auto task = rt.noDependencies();
			task = eval.asyncMul(task, A, B, C);

			f64Matrix<D8> cc, aa;
			enc.revealAll(task, A, aa).get();
			enc.revealAll(task, C, cc).get();


			if (idx == 0)
			{
				f64Matrix<D8> diffMat = (c - cc);

				for (u64 i = 0; i < diffMat.size(); ++i)
				{
					//fp<i64,D8>::ref r = diffMat(i);
					f64<D8> diff = diffMat(i);
					//double diff2 = diff / double(1ull << D8);

					if (diff.mValue > 1 || diff.mValue < -1)
					{
						failed = true;
						oc::lout << Color::Red << "======= FAILED =======" << std::endl << Color::Default;
						sf64<D8> aa = A(0);
						sf64<D8> bb = B(0);
						oc::lout << "\n" << i << "\n"
							<< "p" << rt.mPartyIdx << ": " << "a   " << prettyShare(rt.mPartyIdx, aa.mShare) << " ~ " << std::endl
							<< "p" << rt.mPartyIdx << ": " << "b   " << prettyShare(rt.mPartyIdx, bb.mShare) << " ~ " << std::endl
							<< c(i) << " " << c(i).mValue << std::endl
							<< cc(i) << " " << cc(i).mValue << std::endl
							<< "diff.v\n" << diff.mValue << std::endl
							<< "diff\n" << diff << std::endl;
					}
				}


				if (failed)
				{
					oc::lout << "a " << a << std::endl;
					oc::lout << "b " << b << std::endl;
					oc::lout << "c " << c << std::endl;
					oc::lout << "C " << cc << std::endl;
				}
				//ostreamLock(std::cout) << "p" << rt.mPartyIdx << ": c " << prettyShare(rt.mPartyIdx, C.mShare) << " ~ " << cc << " " << c<< std::endl;
			}
		}
	};

	auto t0 = std::thread(routine, 0);
	auto t1 = std::thread(routine, 1);
	auto t2 = std::thread(routine, 2);

	t0.join();
	t1.join();
	t2.join();

	if (failed)
		throw std::runtime_error(LOCATION);
}




void Sh3_Evaluator_mul_test()
{
    
    throw UnitTestFail("known issue. " LOCATION);

	IOService ios;
	auto chl01 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "01").addChannel();
	auto chl10 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "01").addChannel();
	auto chl02 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "02").addChannel();
	auto chl20 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "02").addChannel();
	auto chl12 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "12").addChannel();
	auto chl21 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "12").addChannel();


	int trials = 10;
	CommPkg comms[3];
	comms[0] = { chl02, chl01 };
	comms[1] = { chl10, chl12 };
	comms[2] = { chl21, chl20 };

	Sh3Encryptor encs[3];
	Sh3Evaluator evals[3];

	encs[0].init(0, toBlock(0, 0), toBlock(0, 1));
	encs[1].init(1, toBlock(0, 1), toBlock(0, 2));
	encs[2].init(2, toBlock(0, 2), toBlock(0, 0));

	evals[0].init(0, toBlock(1, 0), toBlock(1, 1));
	evals[1].init(1, toBlock(1, 1), toBlock(1, 2));
	evals[2].init(2, toBlock(1, 2), toBlock(1, 0));

	bool failed = false;
	auto t0 = std::thread([&]() {
		auto& enc = encs[0];
		auto& eval = evals[0];
		auto& comm = comms[0];
		Sh3Runtime rt;
		rt.init(0, comm);
		PRNG prng(ZeroBlock);

		for (u64 i = 0; i < trials; ++i)
		{
			i64Matrix a(trials, trials), b(trials, trials), c(trials, trials), cc(trials, trials);
			rand(a, prng);
			rand(b, prng);
			si64Matrix A(trials, trials), B(trials, trials), C(trials, trials);

			auto i0 = enc.localIntMatrix(rt, a, A);
			auto i1 = enc.localIntMatrix(rt, b, B);
			auto m = eval.asyncMul(i0 && i1, A, B, C);
			enc.reveal(m, C, cc).get();

			c = a * b;


			if (c != cc)
			{
				failed = true;
				std::cout << c << std::endl;
				std::cout << cc << std::endl;
			}
		}
		});


	auto rr = [&](int i)
	{
		auto& enc = encs[i];
		auto& eval = evals[i];
		auto& comm = comms[i];
		Sh3Runtime rt;
		rt.init(i, comm);

		for (u64 i = 0; i < trials; ++i)
		{
			si64Matrix A(trials, trials), B(trials, trials), C(trials, trials);
			auto i0 = enc.remoteIntMatrix(rt, A);
			auto i1 = enc.remoteIntMatrix(rt, B);

			auto m = eval.asyncMul(i0 && i1, A, B, C);

			auto r = enc.reveal(m, 0, C);

            r.get();
		}
	};

	auto t1 = std::thread(rr, 1);
	auto t2 = std::thread(rr, 2);

	t0.join();
	t1.join();
	t2.join();

	if (failed)
		throw std::runtime_error(LOCATION);
}


void Sh3_f64_basics_test()
{
	f64<D8> v0 = 0.0, v1 = 0.0;
	auto trials = 100;
	PRNG prng(ZeroBlock);

	for (int i = 0; i < trials; ++i)
	{


		auto vv0 = prng.get<i16>() / double(1 << 7);
		auto vv1 = prng.get<i16>() / double(1 << 7);

		v0 = vv0;
		v1 = vv1;


		if (v0 + v1 != f64<D8>(vv0 + vv1))
			throw RTE_LOC;

		if (v0 - v1 != f64<D8>(vv0 - vv1))
			throw RTE_LOC;

		if (v0 + v1 != f64<D8>(vv0 + vv1))
			throw RTE_LOC;

		if (v0 * v1 != f64<D8>(vv0 * vv1))
		{

			std::cout << vv0 << " * " << vv1 << " = " << vv0 * vv1 << std::endl;
			std::cout << v0 << " * " << v1 << " = " << v0 * v1 << std::endl;

			throw RTE_LOC;
		}

	}
}

void createSharing(
	PRNG & prng,
	i64Matrix & value,
	si64Matrix & s0,
	si64Matrix & s1,
	si64Matrix & s2);


void createSharing(
	PRNG & prng,
	i64Matrix & value,
	sbMatrix & s0,
	sbMatrix & s1,
	sbMatrix & s2)
{

	//s0.mShares[0] = value;

	s0.mShares[0].resize(value.rows(), value.cols());
	s1.mShares[0].resize(value.rows(), value.cols());
	s2.mShares[0].resize(value.rows(), value.cols());

	for (auto i = 0ull; i < s0.mShares[0].size(); ++i)
		s0.mShares[0](i) = value(i);

	//if (zeroshare) {
	//	s1.mShares[0].setZero();
	//	s2.mShares[0].setZero();
	//}
	//else 
	{
		prng.get(s1.mShares[0].data(), s1.mShares[0].size());
		prng.get(s2.mShares[0].data(), s2.mShares[0].size());

		for (u64 i = 0; i < s0.mShares[0].size(); ++i)
			s0.mShares[0](i) ^= s1.mShares[0](i) ^ s2.mShares[0](i);
	}

	s0.mShares[1] = s2.mShares[0];
	s1.mShares[1] = s0.mShares[0];
	s2.mShares[1] = s1.mShares[0];
}

void sh3_asyncArithBinMul_test(const oc::CLP& cmd)
{
	IOService ios;
	auto chl01 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "01").addChannel();
	auto chl10 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "01").addChannel();
	auto chl02 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "02").addChannel();
	auto chl20 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "02").addChannel();
	auto chl12 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "12").addChannel();
	auto chl21 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "12").addChannel();


	PRNG prng(ZeroBlock);	
	u64 size = cmd.getOr("size", 100);
	u64 trials = cmd.getOr("t", 10);;
	u64 dec = 16;

	CommPkg comms[3];
	comms[0] = { chl02, chl01 };
	comms[1] = { chl10, chl12 };
	comms[2] = { chl21, chl20 };

	Sh3Runtime p0, p1, p2;

	p0.init(0, comms[0]);
	p1.init(1, comms[1]);
	p2.init(2, comms[2]);

	Sh3Encryptor encs[3];
	Sh3Evaluator evals[3];
	encs[0].init(0, toBlock(0, 0), toBlock(0, 1));
	encs[1].init(1, toBlock(0, 1), toBlock(0, 2));
	encs[2].init(2, toBlock(0, 2), toBlock(0, 0));
	evals[0].init(0, toBlock(1, 0), toBlock(1, 1));
	evals[1].init(1, toBlock(1, 1), toBlock(1, 2));
	evals[2].init(2, toBlock(1, 2), toBlock(1, 0));

	//const Decimal D = Decimal::D8;
	eMatrix<i64> a(size, 1), b(size, 1), c(size, 1), cc(size, 1);

	si64Matrix
		a0(size, 1),
		a1(size, 1),
		a2(size, 1),
		c0(size, 1),
		c1(size, 1),
		c2(size, 1);

	sbMatrix
		b0(size, 1),
		b1(size, 1),
		b2(size, 1);

	for (u64 t = 0; t < trials; ++t)
	{
		for (u64 i = 0; i < size; ++i)
			b(i) = prng.get<bool>();

		prng.get(a.data(), a.size());

		for (u64 i = 0; i < size; ++i)
		{
			a(i) = a(i) >> 32;
			c(i) = a(i) * b(i);
		}

		createSharing(prng, b, b0, b1, b2);
		createSharing(prng, a, a0, a1, a2);

		for (u64 i = 0; i < size; ++i)
		{
			//if (a(i) != a0(i)[0] + a0(i)[1] + a1(i)[0])
			//	throw std::runtime_error(LOCATION);
			//std::cout << "a[" << i << "] = " << a0(i)[0] << " + " << a0(i)[1] << " + " << a1(i)[0] << std::endl;

			b0.mShares[0](i) &= 1;
			b0.mShares[1](i) &= 1;
			b1.mShares[0](i) &= 1;
			b1.mShares[1](i) &= 1;
			b2.mShares[0](i) &= 1;
			b2.mShares[1](i) &= 1;
		}

		auto as0 = evals[0].asyncMul(p0, a0, b0, c0);
		auto as1 = evals[1].asyncMul(p1, a1, b1, c1);
		auto as2 = evals[2].asyncMul(p2, a2, b2, c2);

		p0.runNext();
		p1.runNext(); 
		p2.runNext();

		as0.get();
		as1.get();
		as2.get();

		//t0 = std::thread([&]() { p0.asyncArithBinMul(a0, b0, c0).get(); });
		//t1 = std::thread([&]() { p1.asyncArithBinMul(a1, b1, c1).get(); });
		//t2 = std::thread([&]() { p2.asyncArithBinMul(a2, b2, c2).get(); });
		//t0.join();
		//t1.join();
		//t2.join();


		//std::cout << "c0.0 " << c0.mShares[0] << std::endl;
		//std::cout << "c0.1 " << c1.mShares[1] << std::endl;

		//std::cout << "c1.0 " << c1.mShares[0] << std::endl;
		//std::cout << "c1.1 " << c2.mShares[1] << std::endl;

		//std::cout << "c2.0 " << c2.mShares[0] << std::endl;
		//std::cout << "c2.1 " << c0.mShares[1] << std::endl;

		//typedef LynxEngine::Word Word;
		if (c0.mShares[0] != c1.mShares[1])
			throw std::runtime_error(LOCATION);
		if (c1.mShares[0] != c2.mShares[1])
		{
			throw std::runtime_error(LOCATION);
		}
		if (c2.mShares[0] != c0.mShares[1])
			throw std::runtime_error(LOCATION);


		for (u64 i = 0; i < size; ++i)
		{

			auto ci0 = c0.mShares[0](i);
			auto ci1 = c1.mShares[0](i);
			auto ci2 = c2.mShares[0](i);
			auto di0 = c0.mShares[1](i);
			auto di1 = c1.mShares[1](i);
			auto di2 = c2.mShares[1](i);
			//std::cout << "ci0 " << ci0 << " " << di1 << std::endl;
			//std::cout << "ci1 " << ci1 << " " << di2 << std::endl;
			//std::cout << "ci2 " << ci2 << " " << di0 << std::endl;

			//cc(i) = p0.shareToWord(c0(i), c1(i)[0]);// ^ c2(i);
			cc(i) = c0.mShares[0](i) + c0.mShares[1](i) + c1.mShares[0](i);
			auto ai = a(i);
			auto bi = b(i);
			auto cci = cc(i);
			auto ci = c(i);
			if (cc(i) != c(i))
			{
				throw std::runtime_error(LOCATION);
			}
		}
	}
}

void sh3_asyncPubArithBinMul_test(const oc::CLP& cmd)
{

	IOService ios;

	auto chl01 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "01").addChannel();
	auto chl10 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "01").addChannel();
	auto chl02 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "02").addChannel();
	auto chl20 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "02").addChannel();
	auto chl12 = Session(ios, "127.0.0.1:1313", SessionMode::Server, "12").addChannel();
	auto chl21 = Session(ios, "127.0.0.1:1313", SessionMode::Client, "12").addChannel();

	PRNG prng(ZeroBlock);
	u64 size = cmd.getOr("size", 100);
	//u64 trials = cmd.getOr("t", 10);;

	CommPkg comms[3];
	comms[0] = { chl02, chl01 };
	comms[1] = { chl10, chl12 };
	comms[2] = { chl21, chl20 };

	Sh3Runtime p0, p1, p2;

	p0.init(0, comms[0]);
	p1.init(1, comms[1]);
	p2.init(2, comms[2]);
	Sh3Encryptor encs[3];
	Sh3Evaluator evals[3];
	encs[0].init(0, toBlock(0, 0), toBlock(0, 1));
	encs[1].init(1, toBlock(0, 1), toBlock(0, 2));
	encs[2].init(2, toBlock(0, 2), toBlock(0, 0));
	evals[0].init(0, toBlock(1, 0), toBlock(1, 1));
	evals[1].init(1, toBlock(1, 1), toBlock(1, 2));
	evals[2].init(2, toBlock(1, 2), toBlock(1, 0));

	eMatrix<i64> b(size, 1), c(size, 1), cc(size, 1);

	sbMatrix
		b0(size, 1),
		b1(size, 1),
		b2(size, 1);
	si64Matrix
		c0(size, 1),
		c1(size, 1),
		c2(size, 1);

	i64 a = prng.get<i64>();

	for (u64 i = 0; i < size; ++i)
		b(i) = prng.get<bool>();

	for (u64 i = 0; i < size; ++i)
	{
		c(i) = a * b(i);
	}

	createSharing(prng, b, b0, b1, b2);

	for (u64 i = 0; i < size; ++i)
	{
		//if (a(i) != a0(i)[0] + a0(i)[1] + a1(i)[0])
		//	throw std::runtime_error(LOCATION);
		//std::cout << "a[" << i << "] = " << a0(i)[0] << " + " << a0(i)[1] << " + " << a1(i)[0] << std::endl;

		b0.mShares[0](i) &= 1;
		b0.mShares[1](i) &= 1;
		b1.mShares[0](i) &= 1;
		b1.mShares[1](i) &= 1;
		b2.mShares[0](i) &= 1;
		b2.mShares[1](i) &= 1;
	}

	auto as0 = evals[0].asyncMul(p0, a, b0, c0);
	auto as1 = evals[1].asyncMul(p1, a, b1, c1);
	auto as2 = evals[2].asyncMul(p2, a, b2, c2);

	p0.runNext();
	p1.runNext();
	p2.runNext();

	as0.get();
	as1.get();
	as2.get();

	//t0 = std::thread([&]() { p0.asyncArithBinMul(a0, b0, c0).get(); });
	//t1 = std::thread([&]() { p1.asyncArithBinMul(a1, b1, c1).get(); });
	//t2 = std::thread([&]() { p2.asyncArithBinMul(a2, b2, c2).get(); });
	//t0.join();
	//t1.join();
	//t2.join();

	//typedef LynxEngine::Word Word;
	for (u64 i = 0; i < size; ++i)
	{

		if (c0.mShares[0] != c1.mShares[1])
			throw std::runtime_error(LOCATION);
		if (c1.mShares[0] != c2.mShares[1])
			throw std::runtime_error(LOCATION);
		if (c2.mShares[0] != c0.mShares[1])
			throw std::runtime_error(LOCATION);
		auto ci0 = c0.mShares[0](i);
		auto ci1 = c1.mShares[0](i);
		auto ci2 = c2.mShares[0](i);
		auto di0 = c0.mShares[1](i);
		auto di1 = c1.mShares[1](i);
		auto di2 = c2.mShares[1](i);
		//std::cout << "ci0 " << ci0 << " " << di1 << std::endl;
		//std::cout << "ci1 " << ci1 << " " << di2 << std::endl;
		//std::cout << "ci2 " << ci2 << " " << di0 << std::endl;

		//cc(i) = p0.shareToWord(c0(i), c1(i)[0]);// ^ c2(i);
		cc(i) = c0.mShares[0](i) + c0.mShares[1](i) + c1.mShares[0](i);
		auto bi = b(i);
		auto cci = cc(i);
		auto ci = c(i);
		if (cc(i) != c(i))
		{
			throw std::runtime_error(LOCATION);
		}
	}
}

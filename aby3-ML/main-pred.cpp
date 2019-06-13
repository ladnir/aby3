
#ifdef PRED_PROTOCOL

#include "online/pred.h"
#include "cryptoTools/Common/CLP.h"
#include "cryptoTools/Crypto/PRNG.h"
#include "DataGenerator/LinearModelGen.h"
#include "Engines/PlainEngine.h"
#include "online/Regression.h"
#include "cryptoTools/Network/Channel.h"
#include "cryptoTools/Network/IOService.h"
#include "Engines/LynxEngine.h"

#include "Regression.h"

using namespace oc;


void neural_pred_main_3pc_sh(int N, int D, int O, int layers, int  nodesPerLayer, int  pIdx, bool print, CLP& cmd, Session& epPrev, Session& epNext)
{
	Lynx::Engine::value_type_matrix val_test_data(N, D);



	std::vector<Lynx::Engine::value_type_matrix> W(layers + 1);
	for (u64 i = 0; i < W.size(); ++i)
		W[i].resize(nodesPerLayer, nodesPerLayer);

	W[0].resize(D, nodesPerLayer);
	W[layers - 1].resize(nodesPerLayer, O);



	cmd.setDefault("trials", "1");
	auto trials = cmd.get<int>("trials");

	u64 dec = 16;
	Lynx::Engine p;


	p.init(pIdx, epPrev, epNext, dec, toBlock(pIdx));

	Lynx::Engine::Matrix test_data;

	std::vector<Lynx::Engine::Matrix> W2(layers);

	for (u64 i = 0; i < layers; ++i)
		W2[i].resize(nodesPerLayer, nodesPerLayer);

	W2[0].resize(D, nodesPerLayer);
	W2[layers - 1].resize(nodesPerLayer, O);


	test_data.resize(val_test_data.rows(), val_test_data.cols());

	auto nodeCount = 0;
	for (auto w : W2)
		nodeCount += w.cols();

	if (pIdx == 0)
	{
		for (u64 i = 0; i < layers; ++i)
			p.localInput(W[i], W2[i]);

		p.localInput(val_test_data, test_data);
	}
	else
	{
		for (u64 i = 0; i < layers; ++i)
			p.remoteInput(0, W2[i]);

		p.remoteInput(0, test_data);
	}

	u64 tttt = 0;
	{

		p.mPreproNext.resetStats();
		p.mPreproPrev.resetStats();

		std::chrono::time_point<std::chrono::system_clock>
			preStop,
			preStart = std::chrono::system_clock::now();

		auto aa = std::async([&]() {

			if (cmd.isSet("noOffline") == false)
				for (u64 i = 0; i < trials; ++i)
					p.preprocess(N * nodeCount);

			preStop = std::chrono::system_clock::now();
		});



		p.mNext.resetStats();
		p.mPrev.resetStats();


		auto start = std::chrono::system_clock::now();

		if (cmd.isSet("noOnline") == false)
		{
			for (u64 i = 0; i < trials; ++i)
				pred_neural(p, test_data, W2);
		}
		//val_W2 = p.reveal(W2);
		auto now = std::chrono::system_clock::now();



		//engine.sync();
		aa.get();
		auto preSeconds = std::chrono::duration_cast<std::chrono::microseconds>(preStop - preStart).count() / 1000000.0;
		auto seconds = std::chrono::duration_cast<std::chrono::microseconds>(now - start).count() / 1000000.0;

		double preBytes = p.mPreproNext.getTotalDataSent() + p.mPreproPrev.getTotalDataSent();
		double bytes = p.mNext.getTotalDataSent() + p.mPrev.getTotalDataSent();

		tttt = preBytes + bytes;

		if (print)
		{
			ostreamLock ooo(std::cout);
			ooo << "N: " << N << " D:" << D << " L:" << layers << " NL:" << nodesPerLayer << " => "
				<< (double(seconds) / trials) << "  s/trial  " << (bytes * 8 / 1024 / 2024) / seconds << " Mbps"
				<< " offline: " << (double(preSeconds) / trials) << "  s/trial  " << (preBytes * 8 / 1024 / 2024) / preSeconds << " Mbps"
				<< " " << preBytes + bytes << " total bytes" << std::endl;

			//for (auto& kk : p.preprocMap)
			//{
			//	auto row = kk.first;
			//	for (auto kkk : kk.second)
			//	{
			//		auto cols = kkk.first;
			//		int count = kkk.second;

			//		ooo << "   " << row << " " << cols << " -> " << count << std::endl;
			//	}
			//}
		}
	}
	std::stringstream ss;
	ss << "party " << pIdx << " " << tttt / trials << " bytes/trial" << std::endl;
	std::cout << ss.str() << std::flush;
}

int neural_pred_main_3pc_sh(int argc, char ** argv)
{

	CLP cmd(argc, argv);
	cmd.setDefault("N", 100);
	cmd.setDefault("D", 1000);
	cmd.setDefault("O", 10);
	cmd.setDefault("L", 2);
	cmd.setDefault("NL", 128);

	auto N = cmd.getMany<int>("N");
	auto D = cmd.getMany<int>("D");
	auto O = cmd.getMany<int>("O");
	auto layers = cmd.getMany<int>("L");
	auto nodesPerLayer = cmd.getMany<int>("NL");

	for (auto l : layers)
		if (l < 2) throw std::runtime_error(LOCATION);

	IOService ios(cmd.isSet("p") ? 3 : 7);
	std::vector<std::thread> thrds;
	for (u64 i = 0; i < 3; ++i)
	{
		if (cmd.isSet("p") == false || cmd.get<int>("p") == i)
		{
			thrds.emplace_back(std::thread([i, N, D, O, layers, nodesPerLayer, &cmd, &ios]() {

				auto next = (i + 1) % 3;
				auto prev = (i + 2) % 3;
				auto cNameNext = std::to_string(std::min(i, next)) + std::to_string(std::max(i, next));
				auto cNamePrev = std::to_string(std::min(i, prev)) + std::to_string(std::max(i, prev));

				auto modeNext = i < next ? SessionMode::Server : SessionMode::Client;
				auto modePrev = i < prev ? SessionMode::Server : SessionMode::Client;


				auto portNext = 1212 + std::min(i, next);
				auto portPrev = 1212 + std::min(i, prev);

				Session epNext(ios, "127.0.0.1", portNext, modeNext, cNameNext);
				Session epPrev(ios, "127.0.0.1", portPrev, modePrev, cNamePrev);

				std::cout << "party " << i << " next " << portNext << " mode=server?:" << (modeNext == SessionMode::Server) << " name " << cNameNext << std::endl;
				std::cout << "party " << i << " prev " << portPrev << " mode=server?:" << (modePrev == SessionMode::Server) << " name " << cNamePrev << std::endl;
				auto chlNext = epNext.addChannel();
				auto chlPrev = epPrev.addChannel();

				chlNext.waitForConnection();
				chlPrev.waitForConnection();

				chlNext.send(i);
				chlPrev.send(i);
				u64 prevAct, nextAct;
				chlNext.recv(nextAct);
				chlPrev.recv(prevAct);

				if (next != nextAct) std::cout << " bad next party idx, act: " << nextAct << " exp: " << next << std::endl;
				if (prev != prevAct) std::cout << " bad prev party idx, act: " << prevAct << " exp: " << prev << std::endl;
				ostreamLock(std::cout) << "party " << i << " start" << std::endl;

				auto print = cmd.isSet("p") || i == 0;

				for (auto n : N)
				{
					for (auto d : D)
					{
						for (auto o : O)
						{
							for (auto l : layers)
							{
								for (auto nl : nodesPerLayer)
								{
									neural_pred_main_3pc_sh(n, d, o, l, nl, i, print, cmd, epPrev, epNext);
								}
							}
						}
					}
				}
			}));
		}
	}

	for (auto& t : thrds)
		t.join();

	return 0;
}

int neural_pred_plain_main(int argc, char ** argv)
{
	CLP cmd(argc, argv);
	cmd.setDefault("N", 100);
	cmd.setDefault("D", 1000);
	cmd.setDefault("O", 10);
	cmd.setDefault("L", 2);
	cmd.setDefault("NL", 128);

	auto N = cmd.get<int>("N");
	auto D = cmd.get<int>("D");
	auto O = cmd.get<int>("O");
	auto layers = cmd.get<int>("L");
	auto nodesPerLayer = cmd.get<int>("NL");

	if (layers < 2) throw std::runtime_error(LOCATION);

	PRNG prng(toBlock(1));
	LinearModelGen gen;

	PlainEngine::Matrix model(D, 1);
	for (u64 i = 0; i < D; ++i)
	{
		model(i, 0) = prng.get<int>() % 10;
	}
	gen.setModel(model);


	PlainEngine::Matrix train_data(N, D), train_label(N, 1);

	gen.sample(train_data, train_label);


	std::cout << "pred__" << std::endl;

	RegressionParam params;
	params.mLearningRate = 1.0 / (1 << 10);
	PlainEngine engine;

	std::vector<PlainEngine::Matrix> W(layers + 1);
	for (u64 i = 0; i < W.size(); ++i)
		W[i].resize(nodesPerLayer, nodesPerLayer);

	W[0].resize(D, nodesPerLayer);
	W[layers - 1].resize(nodesPerLayer, O);

	for (u64 i = 0; i < W.size(); ++i)
		W[i].setZero();

	auto Y = pred_neural(engine, train_data, W);

	if (Y.cols() != 1 || Y.size() != N)
		throw std::runtime_error(LOCATION);

	for (u64 i = 0; i < Y.size(); ++i)
	{
		if (Y(i) > O || Y(i) < 0)
			throw std::runtime_error(LOCATION);
	}

	return 0;
}





int linear_logistic_pred_3pc_sh(int N, int D, int pIdx, bool print, CLP& cmd, Session& chlPrev, Session& chlNext, bool logistic)
{
	Lynx::Engine::value_type_matrix val_train_data(N, D), val_W2(D, 1);
	u64 tttt = 0, dec = 16, trials = cmd.get<u64>("trials");
	Lynx::Engine p;
	p.init(pIdx, chlPrev, chlNext, dec, toBlock(pIdx));

	Lynx::Engine::Matrix train_data(N, D), W2(D, 1);


	if (pIdx == 0)
	{
		p.localInput(val_train_data, train_data);
		p.localInput(val_W2, W2);
	}
	else
	{
		p.remoteInput(0, train_data);
		p.remoteInput(0, W2);
	}

	p.mPreproNext.resetStats();
	p.mPreproPrev.resetStats();

	p.mNext.resetStats();
	p.mPrev.resetStats();

	std::chrono::time_point<std::chrono::system_clock>
		preStop,
		preStart = std::chrono::system_clock::now();

	auto aa = std::async([&]() {

		if (cmd.isSet("noOffline") == false)
		{
			for (u64 t = 0; t < trials; ++t)
				p.preprocess(N);
		}

		preStop = std::chrono::system_clock::now();
	});


	auto start = std::chrono::system_clock::now();

	if (cmd.isSet("noOnline") == false)
	{
		if (logistic)
		{
			for (u64 t = 0; t < trials; ++t)
				pred_logistic(p, train_data, W2);
		}
		else
		{
			for (u64 t = 0; t < trials; ++t)
				pred_linear(p, train_data, W2);
		}
	}
	auto now = std::chrono::system_clock::now();

	aa.get();
	auto preSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(preStop - preStart).count() / 1000.0;
	auto seconds = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() / 1000.0;

	double preBytes = p.mPreproNext.getTotalDataSent() + p.mPreproPrev.getTotalDataSent();
	double bytes = p.mNext.getTotalDataSent() + p.mPrev.getTotalDataSent();

	tttt = preBytes + bytes;

	if (print)
	{
		ostreamLock ooo(std::cout);
		ooo << "N: " << N << " D:" << D << " trials:" << trials << (logistic? "  logistic" : "  linear")<< " => "
			<< (double( seconds)/ trials) << "  s/trial  " << (bytes * 8 / 1024 / 2024) / seconds << " Mbps"
			<< " offline: " << (preSeconds/double(trials) ) << "  s/trial  " << (preBytes * 8 / 1024 / 2024) / preSeconds << " Mbps" << std::endl;
	}

	std::stringstream ss;
	ss << "party " << pIdx << " " << tttt / trials << " bytes/trial " << (preBytes / trials) << std::endl;
	ostreamLock(std::cout) << ss.str() << std::flush;

	return 0;
}


int linear_logistic_pred_3pc_sh(int argc, char** argv)
{
	CLP cmd(argc, argv);
	cmd.setDefault("N", 10);
	cmd.setDefault("D", 10);
	cmd.setDefault("trials", 1);

	auto N = cmd.getMany<int>("N");
	auto D = cmd.getMany<int>("D");

	IOService ios(cmd.isSet("p") ? 3 : 7);
	std::vector<std::thread> thrds;
	for (u64 i = 0; i < 3; ++i)
	{
		if (cmd.isSet("p") == false || cmd.get<int>("p") == i)
		{
			thrds.emplace_back(std::thread([i, N, D, &cmd, &ios]() {

				auto next = (i + 1) % 3;
				auto prev = (i + 2) % 3;
				auto cNameNext = std::to_string(std::min(i, next)) + std::to_string(std::max(i, next));
				auto cNamePrev = std::to_string(std::min(i, prev)) + std::to_string(std::max(i, prev));

				auto modeNext = i < next ? SessionMode::Server : SessionMode::Client;
				auto modePrev = i < prev ? SessionMode::Server : SessionMode::Client;

				auto portNext = 1212 + std::min(i, next);
				auto portPrev = 1212 + std::min(i, prev);

				Session epNext(ios, "127.0.0.1", portNext, modeNext, cNameNext);
				Session epPrev(ios, "127.0.0.1", portPrev, modePrev, cNamePrev);

				ostreamLock(std::cout) << "party " << i << " start" << std::endl;
				auto print = cmd.isSet("p") || i == 0;

				for (auto n : N)
				{
					for (auto d : D)
					{
						linear_logistic_pred_3pc_sh(n, d, i, print, cmd, epPrev, epNext, cmd.isSet("logistic"));
					}
				}
			}));
		}
	}

	for (auto& t : thrds)
		t.join();

	return 0;
}


#endif
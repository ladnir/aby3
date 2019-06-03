#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <math.h>
#include <vector>
#include <algorithm>

#include <time.h>

#include <cryptoTools/Network/IOService.h>

#include <cryptoTools/Common/CLP.h>
#include <Eigen/Dense>

#include "online/Regression.h"
#include "DataGenerator/LinearModelGen.h"
#include "Engines/LynxEngine.h"
#include "Engines/FoxEngine.h"
#include "Engines/PlainEngine.h"
#include "logistic.h"
//
//#define N 10000
////#define N 12665
//#define D 784
//#define B 128
//#define testN 10000
//#define Ep 200
//#define IT N*Ep/B
//#define L 20
//#define P 64


using namespace std;
using namespace Eigen;
using namespace oc;

int logistic_plain_main(int argc, char ** argv)
{
	CLP cmd(argc, argv);
	cmd.setDefault("N", 10000);
	cmd.setDefault("testN", 1000);
	cmd.setDefault("D", 1000);
	cmd.setDefault("B", 128);
	cmd.setDefault("I", 10000);

	auto N = cmd.get<int>("N");
	auto D = cmd.get<int>("D");
	auto B = cmd.get<int>("B");
	auto IT = cmd.get<int>("I");
	auto testN = cmd.get<int>("testN");

	PRNG prng(toBlock(1));
	LogisticModelGen gen;

	PlainEngine::Matrix model(D, 1);
	for (u64 i = 0; i < D; ++i)
	{
		model(i, 0) = prng.get<int>() % 10;
		std::cout << model(i, 0) << " ";
	}

	std::cout << std::endl;
	gen.setModel(model);



	PlainEngine::Matrix train_data(N, D), train_label(N, 1);
	PlainEngine::Matrix test_data(testN, D), test_label(testN, 1);


	gen.sample(train_data, train_label);
	gen.sample(test_data, test_label);


	std::cout << "training __" << std::endl;

	RegressionParam params;
	params.mBatchSize = B;
	params.mIterations = IT;
	params.mLearningRate = 1.0 / (1 << 3);
	PlainEngine engine;

	PlainEngine::Matrix W2(D, 1);
	W2.setZero();


	SGD_Logistic(params, engine, train_data, train_label, W2, &test_data, &test_label);

	for (u64 i = 0; i < D; ++i)
	{
		std::cout << i << " " << gen.mModel(i, 0) << " " << W2(i, 0) << std::endl;
	}

	return 0;
}



int logistic_main_3pc_sh(int N, int D, int B, int IT, int testN, int pIdx, bool print, CLP& cmd, Session& chlPrev, Session& chlNext)
{

	//auto print = cmd.isSet("p") == false || cmd.get<int>("p") == 0;

	PRNG prng(toBlock(1));
	LinearModelGen gen;

	Lynx::Engine::value_type_matrix model(D, 1);
	for (u64 i = 0; i < std::min(D, 10); ++i)
	{
		model(i, 0) = prng.get<int>() % 10;
	}
	gen.setModel(model);



	Lynx::Engine::value_type_matrix val_train_data(N, D), val_train_label(N, 1), val_W2(D, 1);
	Lynx::Engine::value_type_matrix val_test_data(testN, D), val_test_label(testN, 1);
	gen.sample(val_train_data, val_train_label);
	gen.sample(val_test_data, val_test_label);


	//std::cout << "training __" << std::endl;

	RegressionParam params;
	params.mBatchSize = B;
	params.mIterations = IT;
	params.mLearningRate = 1.0 / (1 << 10);


	//Mat W2(D, 1);
	val_W2.setZero();





	u64 dec = 16;
	Lynx::Engine p;


	p.init(pIdx, chlPrev, chlNext, dec, toBlock(pIdx));

	Lynx::Engine::Matrix train_data, train_label, W2, test_data, test_label;

	train_data.resize(val_train_data.rows(), val_train_data.cols());
	train_label.resize(val_train_label.rows(), val_train_label.cols());
	W2.resize(val_train_data.cols(), 1);
	test_data.resize(val_test_data.rows(), val_test_data.cols());
	test_label.resize(val_test_label.rows(), val_test_label.cols());

	if (pIdx == 0)
	{
		p.localInput(val_train_data, train_data);
		p.localInput(val_train_label, train_label);
		p.localInput(val_W2, W2);
		p.localInput(val_test_data, test_data);
		p.localInput(val_test_label, test_label);
	}
	else
	{
		p.remoteInput(0, train_data);
		p.remoteInput(0, train_label);
		p.remoteInput(0, W2);
		p.remoteInput(0, test_data);
		p.remoteInput(0, test_label);
	}

	//SGD_Linear(params, p, train_data, train_label, W2, &test_data, &test_label);



	p.mPreproNext.resetStats();
	p.mPreproPrev.resetStats();

	auto preStart = std::chrono::system_clock::now();

	p.preprocess((B + D) *IT);

	double preBytes = p.mPreproNext.getTotalDataSent() + p.mPreproPrev.getTotalDataSent();


	p.mNext.resetStats();
	p.mPrev.resetStats();


	auto start = std::chrono::system_clock::now();

	if (cmd.isSet("noOnline") == false)
		SGD_Logistic(params, p, train_data, train_label, W2);
	//val_W2 = p.reveal(W2);

	auto end = std::chrono::system_clock::now();


	//engine.sync();
	auto now = std::chrono::system_clock::now();
	auto preSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(start - preStart).count() / 1000.0;
	auto seconds = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() / 1000.0;

	double bytes = p.mNext.getTotalDataSent() + p.mPrev.getTotalDataSent();

	if (print)
	{
		ostreamLock ooo(std::cout);
		ooo << "N: " << N << " D:" << D << " B:" << B << " IT:" << IT << " => "
			<< (double(IT) / seconds) << "  iters/s  " << (bytes * 8 / 1024 / 2024) / seconds << " Mbps"
			<< " offline: " << (double(IT) / preSeconds) << "  iters/s  " << (preBytes * 8 / 1024 / 2024) / preSeconds << " Mbps" << std::endl;

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


	return 0;
}


int logistic_main_3pc_sh(int argc, char** argv)
{

	CLP cmd(argc, argv);
	cmd.setDefault("N", 10000);
	cmd.setDefault("testN", 1000);
	cmd.setDefault("D", 1000);
	cmd.setDefault("B", 128);
	cmd.setDefault("I", 10000);

	auto N = cmd.getMany<int>("N");
	auto D = cmd.getMany<int>("D");
	auto B = cmd.getMany<int>("B");
	auto IT = cmd.getMany<int>("I");
	auto testN = cmd.get<int>("testN");

	IOService ios(cmd.isSet("p") ? 2 : 6);
	std::vector<std::thread> thrds;
	for (u64 i = 0; i < 3; ++i)
	{
		if (cmd.isSet("p") == false || cmd.get<int>("p") == i)
		{
			thrds.emplace_back(std::thread([i, N, D, B, IT, testN, &cmd, &ios]() {

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

				if (next != nextAct)
					std::cout << " bad next party idx, act: " << nextAct << " exp: " << next << std::endl;
				if (prev != prevAct)
					std::cout << " bad prev party idx, act: " << prevAct << " exp: " << prev << std::endl;

				ostreamLock(std::cout) << "party " << i << " start" << std::endl;

				auto print = cmd.isSet("p") || i == 0;

				for (auto n : N)
				{
					for (auto d : D)
					{
						for (auto b : B)
						{
							for (auto it : IT)
							{
								logistic_main_3pc_sh(n, d, b, it, testN, i, print, cmd, epPrev, epNext);
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




//
//#define N 10000
////#define N 12665
//#define D 784
//#define B 128
//#define testN 10000
//#define Ep 200
//#define IT N*Ep/B
//#define L 20
//#define P 64
//
//using namespace std;
//using namespace Eigen;
//
//typedef unsigned long int uint64;
//typedef Matrix<unsigned long int,Dynamic,Dynamic> Mat;
//
//NetIO * io;
//
//typedef OTIterated<NetIO, SHOTExtension> OTIter;
//OTIter* ot;
//OTIter* ot2;
//
//
//uint64 randomlong(){
//	uint64 rand1 = abs(rand());
//    uint64 rand2 = abs(rand());
//    rand1 = rand1 << (sizeof(int)*8);
//    uint64 randULL = (rand1 | rand2);
//    return randULL;
//
//}
//
//Mat reconstruct(Mat A, int party){
//	vector<unsigned long int> A_temp(A.cols()*A.rows());
//
//	int tempi = 10;
//	if(party==ALICE){
//		io->send_data(&tempi,4);
//		io->recv_data(&A_temp[0],sizeof(unsigned long int)*A_temp.size());
//		Mat A_(A.rows(),A.cols());
//		for(int i=0;i<A_.rows();i++){
//			for(int j=0;j<A_.cols();j++){
//				A_(i,j) = A_temp[i*A_.cols()+j];
//			}
//		}
//
//		Mat A_rec = A+A_;
//
//
//		for(int i=0;i<A_rec.rows();i++){
//			for(int j=0;j<A_rec.cols();j++)
//				cout<<A_rec(i,j)<<",";
//			cout<<";";
//		}
//		cout<<endl;
//
//
//		//cout<<A_rec<<endl;
//
//		return A_rec;
//	}
//	else{
//		for(int i=0;i<A.rows();i++){
//			for(int j=0;j<A.cols();j++)
//				A_temp[i*A.cols()+j] = A(i,j);
//		}
//
//		io->recv_data(&tempi,4);
//		io->send_data(&A_temp[0],sizeof(unsigned long int)*A_temp.size());
//
//
//
//		return A;
//	}
//
//
//}
//
//void load_train_data(Mat& train_data0, Mat& train_label0, Mat& xa, int party){
//	ifstream F( "data"+to_string(party)+".txt" );
//	ifstream F2("xa.txt");
//
//
//	int i=0;
//
//	//cout<<"load training data.......\n";
//
//	while(i<N){
//
//		string s;
//		if (!getline(F,s))
//			break;
//		istringstream ss(s);
//		char c;
//
//
//		//read data
//		for(int j=0;j<D;j++){
//			ss>>train_data0(i,j);
//			ss>>c;
//
//		}
//
//		i++;
//
//	}
//
//
//	i=0;
//
//	while(i<N){
//
//		string s;
//		if (!getline(F,s))
//			break;
//		istringstream ss(s);
//		char c;
//
//
//		//read data
//
//		ss>>train_label0(i);
//		ss>>c;
//
//
//		i++;
//
//	}
//
//	i=0;
//
//	while(i<N){
//
//		string s;
//		if (!getline(F2,s))
//			break;
//		istringstream ss(s);
//		char c;
//
//
//		//read data
//		for(int j=0;j<D;j++){
//			ss>>xa(i,j);
//			ss>>c;
//		}
//
//		i++;
//
//	}
//
//
//	F.close();
//	F2.close();
//	return;
//
//}
//
//void load_test_data(Mat& test_data, Mat& test_label){
//	ifstream infile2( "../data/mnist_test.csv" );
//
//	int i=0;
//
//	//cout<<"load testing data.......\n";
//
//	while(infile2){
//
//		string s;
//		if (!getline(infile2,s))
//			break;
//		istringstream ss(s);
//		int temp;
//		char c;
//
//		//read label
//		ss>>temp;
//		ss>>c;
//
//		//if(temp == 0 || temp == 1){
//			test_label(i) = (temp!=0);
//
//
//			//read data (last entry 1)
//			for(int j=0;j<D-1;j++){
//				ss>>test_data(i,j);
//				ss>>c;
//			}
//
//			test_data(i,D-1) = 1;
//			i++;
//		//}
//
//	}
//
//	test_data.conservativeResize(i, D);
//	test_label.conservativeResize(i,1);
//
//	infile2.close();
//
//	return;
//}
//
//vector<int> read_MT(Mat& a,Mat& b_1,Mat& c_1,Mat& b_2,Mat& c_2, int party){
//
//
//	vector<int> perm(Ep*N,0);
//
//
//	ifstream F("MT"+to_string(party)+".txt");
//	char temp;
//
//	{
//		string s;
//		getline(F,s);
//
//
//		istringstream ss(s);
//
//		for(int i=0;i<Ep*N;i++){
//			ss>>perm[i];
//			ss>>temp;
//		}
//	}
//
//
//
//	for(int i=0;i<N;i++){
//		string s;
//		if (!getline(F,s))
//			break;
//		istringstream ss(s);
//		for(int j=0;j<D;j++){
//			ss>>a(i,j);
//
//			ss>>temp;
//		}
//	}
//
//
//	for(int i=0;i<D;i++){
//		string s;
//		if (!getline(F,s))
//			break;
//		istringstream ss(s);
//		for(int j=0;j<IT;j++){
//			ss>>b_1(i,j);
//			ss>>temp;
//		}
//	}
//
//
//	for(int i=0;i<B;i++){
//		string s;
//		if (!getline(F,s))
//			break;
//		istringstream ss(s);
//		for(int j=0;j<IT;j++){
//			ss>>b_2(i,j);
//			ss>>temp;
//		}
//	}
//
//
//	for(int i=0;i<B;i++){
//		string s;
//		if (!getline(F,s))
//			break;
//		istringstream ss(s);
//		for(int j=0;j<IT;j++){
//			ss>>c_1(i,j);
//			ss>>temp;
//		}
//	}
//
//
//
//	for(int i=0;i<D;i++){
//		string s;
//		if (!getline(F,s))
//			break;
//		istringstream ss(s);
//		for(int j=0;j<IT;j++){
//			ss>>c_2(i,j);
//			ss>>temp;
//		}
//	}
//
//
//
//	F.close();
//
//	return perm;
//
//}
//
//void next_batch(Mat& batch,int start, vector<int>& perm, Mat& A){
//
//
//	for(int i=0;i<B;i++){
//		batch.row(i) = A.row(perm[start+i]);
//	}
//	return ;
//}
//
//Mat compute_inner0(Mat& W0,Mat& x0, Mat& e, Mat& b0,Mat& c0, Mat& Wb1 ){
//	Mat f,d;
//	f = W0-b0+Wb1;
//	d = x0*f+e*W0+c0;
//	return d;
//
//}
//
//Mat compute_inner1(Mat& W1,Mat& x1, Mat& e, Mat& b1,Mat& c1, Mat& Wb0 ){
//	Mat f,d;
//	f = W1-b1+Wb0;
//	d = x1*f+e*(W1-f)+c1;
//	return d;
//
//}
//
//Mat logistic(Mat& inner, Mat& y, int party){
//
//	//clock_t t1=clock();
//
//
//	Batcher batcher1, batcher2;
//
//	if(party==ALICE){
//
//		for(int i = 0; i < inner.rows(); ++i) {
//
//			batcher1.add<Integer>(P, inner(i));
//			batcher2.add<Integer>(P, (unsigned long)0);
//
//		}
//	}
//	else{
//		for(int i = 0; i < inner.rows(); ++i) {
//			batcher1.add<Integer>(P, (unsigned long)0);
//			batcher2.add<Integer>(P, inner(i));
//
//
//		}
//
//	}
//	batcher1.make_semi_honest(ALICE);
//	batcher2.make_semi_honest(BOB);
//
//
//	//cout<<(double)(clock()-t1)/CLOCKS_PER_SEC<<"s   1"<<endl;
//
//	Bit* a = new Bit[inner.rows()];
//	Bit* b = new Bit[inner.rows()];
//	Bit* c = new Bit[inner.rows()];
//	Bit* d = new Bit[inner.rows()];
//	bool* lsb_c = new bool[inner.rows()];
//	bool* lsb_d = new bool[inner.rows()];
//
//	//Integer temp2(L+1,(int)pow(2,L),BOB);
//
//
//	for(int i = 0; i < inner.rows(); ++i){
//		//Integer temp = batcher1.next<Integer>() + batcher2.next<Integer>();
//		Integer temp = batcher2.next<Integer>();
//		Integer temp2 = batcher1.next<Integer>()+temp;
//		b[i] = (temp2)[P-1];
//		a[i] = (temp2 - Integer(P, 1ULL<<L))[P-1] ;
//		//a[i] = (batcher1.next<Integer>() + temp)[P-1];
//
//		c[i] = a[i]&(!(b[i]));
//		d[i] = !(a[i]);
//		lsb_c[i] = getLSB(c[i]);
//		lsb_d[i] = getLSB(d[i]);
//
//	}
//
//
//
//
//	//cout<<(double)(clock()-t1)/CLOCKS_PER_SEC<<"s    2"<<endl;
//
//
//	bool* select;
//	select = new bool[inner.rows()*2];
//	copy(lsb_c,lsb_c+inner.rows(),select);
//	copy(lsb_d,lsb_d+inner.rows(),select+inner.rows());
//
//
//	block* m0,*m1,*r;
//
//
//	if(party==ALICE){
//
//		m0 = new block[inner.rows()*2];
//		m1 = new block[inner.rows()*2];
//		r = new block[inner.rows()];
//
//	}
//	else{
//
//		m0 = new block[inner.rows()];
//		m1 = new block[inner.rows()];
//		r = new block[inner.rows()*2];
//	}
//
//	//cout<<(double)(clock()-t1)/CLOCKS_PER_SEC<<"s 1"<<endl;
//
//	uint64 temp_m0, temp_m1;
//	Mat r1(inner.rows(), inner.cols()),r2,R(inner.rows(), inner.cols()),R2;
//
//	for(int i=0;i<r1.rows();i++){
//		r1(i) = randomlong();
//	}
//
//
//	if(party==ALICE){
//		r2.resize(inner.rows(), inner.cols());
//
//		for(int i=0;i<r2.rows();i++){
//			r2(i) = randomlong();
//		}
//	}
//	else{
//		R2.resize(inner.rows(), inner.cols());
//	}
//
//
//	//cout<<(double)(clock()-t1)/CLOCKS_PER_SEC<<"s 2"<<endl;
//
//
//
//
//	for(int i=0;i<inner.rows();++i){
//		temp_m0 = (int)select[i]*inner(i)+r1(i);
//		temp_m1 = (int)(!select[i])*inner(i)+r1(i);
//		m0[i] = makeBlock((uint64)0,temp_m0);
//		m1[i] = makeBlock((uint64)0,temp_m1);
//	}
//
//
//	if(party == ALICE){
//		for(int i=inner.rows();i<inner.rows()*2;++i){
//			temp_m0 = (int)select[i]*(uint64)(1<<L)+r2(i-inner.rows());
//			temp_m1 = (int)(!select[i])*(uint64)(1<<L)+r2(i-inner.rows());
//			m0[i] = makeBlock((uint64)(0),temp_m0);
//			m1[i] = makeBlock((uint64)(0),temp_m1);
//		}
//	}
//
//	//cout<<(double)(clock()-t1)/CLOCKS_PER_SEC<<"s 3"<<endl;
//
//	Mat y_;
//
//	if(party == ALICE){
//
//		ot->send(m0,m1,inner.rows()*2);
//
//		//cout<<(double)(clock()-t1)/CLOCKS_PER_SEC<<"s 4"<<endl;
//
//		ot2->recv(r,select,inner.rows());
//
//		for(int i=0;i<inner.rows();i++){
//			R(i) = ((unsigned long *)(&r[i]))[0];
//		}
//
//		y_ = R-r1-r2-y*(uint64)(1<<L);
//
//	}
//	else{
//		ot->recv(r,select,inner.rows()*2);
//
//		//cout<<(double)(clock()-t1)/CLOCKS_PER_SEC<<"s 4"<<endl;
//
//		ot2->send(m0,m1,inner.rows());
//		for(int i=0;i<inner.rows();i++){
//			R(i) = ((unsigned long *)(&r[i]))[0];
//			R2(i) = ((unsigned long *)(&r[i+inner.rows()]))[0];
//		}
//
//		y_ = R+R2-r1-y*(uint64)(1<<L);
//	}
//
//
//	//cout<<(double)(clock()-t1)/CLOCKS_PER_SEC<<"s 3"<<endl;
//
//	return y_;
//
//}
//
//void update_W0(Mat& W0,Mat& x0,Mat& e, Mat& b0,Mat& d0, Mat& c0, Mat& db1){
//	Mat f, delta;
//
//	f = d0-b0+db1;
//	delta = x0*f+e*d0+c0;
//
//	//reconstruct(delta,1);
//
//	for(int i=0;i<delta.rows();i++){
//		delta(i) = (long int)delta(i)/ ((long int)(1<<23)*B);
//	}
//	//reconstruct(delta,1);
//
//	W0 = W0 - delta;
//
//}
//
//void update_W1(Mat& W1,Mat& x1,Mat& e, Mat& b1,Mat& d1, Mat& c1, Mat& db0){
//	Mat f, delta;
//
//	f = d1-b1+db0;
//
//	delta = x1*f+e*(d1-f)+c1;
//
//	//reconstruct(delta,2);
//
//	for(int i=0;i<delta.rows();i++){
//		delta(i) = (long int)delta(i)/ ((long int)(1<<23)*B);
//	}
//
//	//reconstruct(delta,2);
//
//	W1 = W1 - delta;
//
//
//
//}
//
//double test_model(Mat& W0, Mat& W1, Mat& x, Mat& y){
//	Mat y_,W;
//	double temp1;
//	long int temp2,temp3;
//
//	W = W0+W1;
//	y_ = x*W;
//
//	int count = 0;
//
//	for(int i=0;i<y.rows();i++){
//		temp3 = (long int)y_(i);
//		//temp3 = (temp3<<4);
//
//		//if(temp3>conv<long int>(p)/2)
//		//	temp3 = temp3-conv<long int>(p);
//
//		temp1 = temp3/(double)pow(2,L);
//		//temp1 = conv<long int>(y_[i][0])/(double)pow(2,L);
//		temp2 = (long int)y(i);
//
//		//if(temp2>conv<long int>(p)/2)
//		//	temp2 = temp2-conv<long int>(p);
//
//		if(temp1>0.5 && temp2 == 1){
//			count++;
//		}
//		else if(temp1<0.5 && temp2 == 0){
//			count++;
//		}
//	}
//	return count/(double)y.rows();
//
//
//}
//
//
//
//int logistic_main(int argc, char** argv){
//
//	clock_t t1,t3;
//
//	//setup connection
//	int port, party;
//	parse_party_and_port(argv, argc, &party, &port);
//	io = new NetIO(party==ALICE ? nullptr : "172.31.13.143", port);
//	io->set_nodelay();
//
//
//	setup_semi_honest(io, EmpParty(party), toBlock(11));
//
//	ot = new SHOTIterated<NetIO>(io, party==ALICE, toBlock(22));
//	ot2 = new SHOTIterated<NetIO>(io, party!=ALICE, toBlock(33));
//
//	Mat train_data(N,D), train_label(N,1), xa(N,D);
//
//	load_train_data(train_data, train_label, xa, party-1);
//
//	Mat test_data(testN,D), test_label(testN,1);
//
//	if(party == ALICE){
//
//		load_test_data(test_data, test_label);
//	}
//
//	//cout<<"reading multiplication triples......\n";
//
//	Mat a(N,D),b_1(D,IT),c_1(B,IT),b_2(B,IT),c_2(D,IT);
//
//	t1 = clock();
//
//	vector<int> perm = read_MT(a,b_1,c_1,b_2,c_2,party-1);
//
//	//cout<<(double)(clock()-t1)/CLOCKS_PER_SEC<<"s for reading"<<endl;
//
//
//
//	//cout<<(reconstruct(train_data-a,party)==xa)<<"!!!!\n";
//
//	Mat W(D,1);
//	W.setZero();
//
//
//	Mat x_batch(B,D), tx_batch,txa_batch, y_batch(B,1), xa_batch(B,D), b_IT1(D,1), b_IT2(B,1),c_IT1(B,1), c_IT2(D,1);
//
//	Mat W0b(D,1),W1b(D,1),d0_buf(B,1),d1_buf(B,1);
//
//
//    double total_time = 0.0, communication = 0.0, computation = 0.0, batchtime = 0.0;
//	int start = 0;
//
//	//start training
//	for(int i=0;i<IT;i++){
//
//		t1=clock();
//
//		Mat d0,d1,W_rec, inner;
//
//		next_batch(x_batch,start,perm,train_data);
//		next_batch(y_batch,start,perm,train_label);
//		tx_batch = x_batch.transpose();
//
//
//		next_batch(xa_batch,start,perm,xa);
//		txa_batch = xa_batch.transpose();
//
//		b_IT1=b_1.col(i);
//		b_IT2=b_2.col(i);
//		c_IT1=c_1.col(i);
//		c_IT2=c_2.col(i);
//
//		//Mat a_batch(B,D);
//		//next_batch(a_batch,start,perm,a);
//		//cout<<(reconstruct(a_batch,party)*reconstruct(b_IT1,party) == reconstruct(c_IT1,party))<<endl;
//
//		batchtime += (double)(clock()-t1)/CLOCKS_PER_SEC;
//		t1=clock();
//
//		start+= B;
//
//		//t1=clock();
//		t3 = clock();
//		//send and receive data
//
//
//		if(party == ALICE){
//			W0b = W-b_IT1;
//		}
//		else{
//			W1b = W-b_IT1;
//		}
//
//		vector<unsigned long int> temp0(W0b.cols()*W0b.rows()),temp1(W1b.cols()*W1b.rows());
//
//		if(party == ALICE){
//
//			for(int j=0;j<W0b.rows();j++){
//				for(int k=0;k<W0b.cols();k++)
//					temp0[j*W0b.cols()+k] = W0b(j,k);
//			}
//
//			io->send_data(&temp0[0],sizeof(unsigned long int)*temp0.size());
//			io->recv_data(&temp1[0],sizeof(unsigned long int)*temp1.size());
//
//			for(int j=0;j<W1b.rows();j++){
//				for(int k=0;k<W1b.cols();k++)
//					W1b(j,k) = temp1[j*W1b.cols()+k];
//			}
//
//		}
//		else{
//			for(int j=0;j<W1b.rows();j++){
//				for(int k=0;k<W1b.cols();k++)
//					temp1[j*W1b.cols()+k] = W1b(j,k);
//			}
//
//			io->recv_data(&temp0[0],sizeof(unsigned long int)*temp0.size());
//			io->send_data(&temp1[0],sizeof(unsigned long int)*temp1.size());
//
//
//			for(int j=0;j<W0b.rows();j++){
//				for(int k=0;k<W0b.cols();k++)
//					W0b(j,k) = temp0[j*W0b.cols()+k];
//			}
//
//		}
//
//		communication += (double)(clock()-t1)/CLOCKS_PER_SEC;
//
//
//		//t2=clock();
//		//cout<<(double)(t2-t1)/CLOCKS_PER_SEC<<endl;
//		t1=clock();
//
//
//		//train step1
//
//		if(party == ALICE){
//			inner = compute_inner0(W,x_batch, xa_batch, b_IT1,c_IT1, W1b );
//
//			//t1=clock();
//
//			d0 = logistic(inner, y_batch, party);
//
//			//cout<<"1 step time: "<<(double)(clock()-t1)/CLOCKS_PER_SEC<<endl;
//
//			d0_buf = d0-b_IT2;
//			//reconstruct(d0,party);
//		}
//		else{
//			inner = compute_inner1(W,x_batch, xa_batch, b_IT1,c_IT1, W0b );
//
//			//t1=clock();
//
//			d1 = logistic(inner, y_batch, party);
//
//			//cout<<"1 step time: "<<(double)(clock()-t1)/CLOCKS_PER_SEC<<endl;
//
//			d1_buf = d1-b_IT2;
//
//
//
//			//reconstruct(d1,party);
//		}
//
//
//
//		computation += (double)(clock()-t1)/CLOCKS_PER_SEC;
//		t1=clock();
//
//		//t2=clock();
//		//cout<<(double)(t2-t1)/CLOCKS_PER_SEC<<endl;
//		//t1=clock();
//
//
//		//send and receive data
//		vector<unsigned long int> d_temp0(d0_buf.cols()*d0_buf.rows()),d_temp1(d1_buf.cols()*d1_buf.rows());
//
//
//		if(party == ALICE){
//			for(int j=0;j<d0_buf.rows();j++){
//				for(int k=0;k<d0_buf.cols();k++)
//					d_temp0[j*d0_buf.cols()+k] = d0_buf(j,k);
//			}
//
//			io->send_data(&d_temp0[0],sizeof(unsigned long int)*d_temp0.size());
//			io->recv_data(&d_temp1[0],sizeof(unsigned long int)*d_temp1.size());
//
//			for(int j=0;j<d1_buf.rows();j++){
//				for(int k=0;k<d1_buf.cols();k++)
//					d1_buf(j,k) = d_temp1[j*d1_buf.cols()+k];
//			}
//		}
//		else{
//			for(int j=0;j<d1_buf.rows();j++){
//				for(int k=0;k<d1_buf.cols();k++)
//					d_temp1[j*d1_buf.cols()+k] = d1_buf(j,k);
//			}
//
//			io->recv_data(&d_temp0[0],sizeof(unsigned long int)*d_temp0.size());
//			io->send_data(&d_temp1[0],sizeof(unsigned long int)*d_temp1.size());
//
//
//			for(int j=0;j<d0_buf.rows();j++){
//				for(int k=0;k<d0_buf.cols();k++)
//					d0_buf(j,k) = d_temp0[j*d0_buf.cols()+k];
//			}
//
//		}
//
//		communication += (double)(clock()-t1)/CLOCKS_PER_SEC;
//		t1=clock();
//
//		//t2=clock();
//		//cout<<(double)(t2-t1)/CLOCKS_PER_SEC<<endl;
//		//t1=clock();
//
//		//train step2
//
//		if(party == ALICE){
//			update_W0(W,tx_batch,txa_batch, b_IT2,d0, c_IT2, d1_buf);
//		}
//		else{
//			update_W1(W,tx_batch, txa_batch, b_IT2,d1, c_IT2, d0_buf);
//		}
//
//		computation += (double)(clock()-t1)/CLOCKS_PER_SEC;
//
//
//		//reconstruct(W,party);
//
//		//t2=clock();
//		//cout<<(double)(t2-t1)/CLOCKS_PER_SEC<<endl;
//
//
//		total_time += (double)(clock()-t3)/CLOCKS_PER_SEC;
//
//		if(i%1==1){
//			vector<long int> W_temp(W.cols()*W.rows());
//			int tempi = 10;
//			Mat W1(D,1);
//
//			if(party==ALICE){
//
//				io->send_data(&tempi,4);
//				io->recv_data(&W_temp[0],sizeof(unsigned long int)*W_temp.size());
//
//
//				for(int j=0;j<W1.rows();j++){
//					for(int k=0;k<W1.cols();k++)
//						W1(j,k) = W_temp[j*W1.cols()+k];
//				}
//
//
//				double res = test_model(W,W1,test_data, test_label);
//				cout<<res<<"%\n";
//			}
//			else{
//				for(int j=0;j<W.rows();j++){
//					for(int k=0;k<W.cols();k++)
//						W_temp[j*W.cols()+k] = W(j,k);
//				}
//
//				io->recv_data(&tempi,4);
//				io->send_data(&W_temp[0],sizeof(unsigned long int)*W_temp.size());
//
//
//			}
//
//		}
//
//	}
//
//
//
//	cout<<total_time<<endl;
//	//cout<<"total time:"<<total_time<<"s!!"<<endl;
//	//cout<<"batch time:"<<batchtime<<"s!!"<<endl;
//	//cout<<"communication time:"<<communication<<"s!!"<<endl;
//	//cout<<"computation time:"<<computation<<"s!!"<<endl;
//
//	delete io;
//
//	return 0;
//}


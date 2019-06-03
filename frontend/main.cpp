
#include <cryptoTools/Common/CLP.h>

#include "aby3_tests/aby3_tests.h"
#include "eric.h"
#include "aby3-DB-main.h"
#include "com-psi_tests/UnitTests.h"
#include <tests_cryptoTools/UnitTests.h>
#include <aby3-ML/main-linear.h>

#include "cryptoTools/tests_cryptoTools/UnitTests.h"

using namespace oc;
std::vector<std::string> unitTestTag{ "u", "unitTest" };


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
			auto tests = tests_cryptoTools::Tests;
			tests += aby3_tests;
			tests += ComPsi_tests;

			tests.runIf(cmd);
			return 0;
		}

		if (cmd.isSet("linear-plain"))
		{
			set = true;
			linear_plain_main(cmd);
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
				ComPsi_Intersect(size, c, cmd.isSet("sum"));
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



		if (cmd.isSet("card"))
		{
			set = true;

			auto nn = cmd.getMany<int>("nn");
			if (nn.size() == 0)
				nn.push_back(1 << 16);

			for (auto n : nn)
			{
				auto size = 1 << n;
				ComPsi_cardinality(size);
			}
		}
		
		//if (cmd.isSet("add"))
		//{
		//	set = true;

		//	auto nn = cmd.getMany<int>("nn");
		//	if (nn.size() == 0)
		//		nn.push_back(1 << 16);

		//	for (auto n : nn)
		//	{
		//		auto size = 1 << n;
		//		Sh3_add_test(size);
		//	}
		//}

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

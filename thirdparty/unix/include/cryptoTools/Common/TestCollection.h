#pragma once
#include <vector>
#include <list>
#include <functional>
#include <string>
#include <cryptoTools/Common/Defines.h>
//#include <cryptoTools/Common/Defines.h>

//#define OSU_CRYPTO_PP_CAT(a, b) OSU_CRYPTO_PP_CAT_I(a, b)
//#define OSU_CRYPTO_PP_CAT_I(a, b) OSU_CRYPTO_PP_CAT_II(~, a ## b)
//#define OSU_CRYPTO_PP_CAT_II(p, res) res
//#define OSU_CRYPTO_UNIQUE_NAME(base) OSU_CRYPTO_PP_CAT(base, __COUNTER__)
//
//
#define OSU_CRYPTO_ADD_TEST(harness, test)       
//static int OSU_CRYPTO_UNIQUE_NAME(__add_test_) = []() { 
//    harness.add(STRINGIZE(test), test);          
//    return 0;                                    
//}();

namespace osuCrypto
{
    class CLP;
    class TestCollection
    {
    public:
        struct Test
        {
            std::string mName;
            std::function<void(const CLP&)> mTest;
        };
        TestCollection() = default; 
        TestCollection(std::function<void(TestCollection&)> init)
        {
            init(*this);
        }

        std::vector<Test> mTests;

        enum class Result
        {
            passed,
            skipped,
            failed
        };

        Result runOne(u64 idx, CLP const * cmd = nullptr);
        Result run(std::vector<u64> testIdxs, u64 repeatCount = 1, CLP const * cmd = nullptr);
        Result runAll(uint64_t repeatCount = 1, CLP const * cmd = nullptr);
        Result runIf(CLP& cmd);
        void list();

        std::vector<u64> search(const std::list<std::string>& s);


        void add(std::string name, std::function<void()> test);
        void add(std::string name, std::function<void(const CLP&)> test);

        void operator+=(const TestCollection& add);
    };


    class UnitTestFail : public std::exception
    {
        std::string mWhat;
    public:
        explicit UnitTestFail(std::string reason)
            :std::exception(),
            mWhat(reason)
        {}

        explicit UnitTestFail()
            :std::exception(),
            mWhat("UnitTestFailed exception")
        {
        }

        virtual  const char* what() const throw()
        {
            return mWhat.c_str();
        }
    };

    class UnitTestSkipped : public std::runtime_error
    {
    public:
        UnitTestSkipped()
            : std::runtime_error("skipping test")
        {}

        UnitTestSkipped(std::string r)
            : std::runtime_error(r)
        {}
    };

    extern TestCollection globalTests;
}
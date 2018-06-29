
#include "com-psi/ComPsiServer.h"
#include <cryptoTools/Network/IOService.h>
#include <unordered_map>
#include <iomanip>
#include <cryptoTools/Common/Timer.h>

using namespace osuCrypto;




void eric(int n)
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



    auto keyBitCount = srvs[0].mKeyBitCount;

    std::vector<ColumnInfo> voterSchema{
        //ColumnInfo{ "NA", TypeID::IntID, keyBitCount },
        //ColumnInfo{ "AD", TypeID::IntID, keyBitCount },
        ColumnInfo{ "DL", TypeID::IntID, keyBitCount }
    }, 
    dvmSchema{
        ColumnInfo{ "NA", TypeID::IntID, keyBitCount },
        ColumnInfo{ "AD", TypeID::IntID, 24 },
        ColumnInfo{ "DL", TypeID::IntID, keyBitCount },
        //ColumnInfo{ "DOB", TypeID::IntID, keyBitCount },
        ColumnInfo{ "SSN", TypeID::IntID, keyBitCount }
    }/*,
    stateSchema{
        ColumnInfo{ "NA_vote", TypeID::IntID, keyBitCount },
        ColumnInfo{ "AD_vote", TypeID::IntID, keyBitCount },
        ColumnInfo{ "NA_dmv", TypeID::IntID, keyBitCount },
        ColumnInfo{ "AD_dvm", TypeID::IntID, keyBitCount },
        ColumnInfo{ "DL", TypeID::IntID, keyBitCount },
        ColumnInfo{ "DOB", TypeID::IntID, keyBitCount },
        ColumnInfo{ "SSN", TypeID::IntID, keyBitCount },
        ColumnInfo{ "Reg", TypeID::IntID, 1 }
    },
    uspsData{
        ColumnInfo{ "NA", TypeID::IntID, keyBitCount },
        ColumnInfo{ "NA_new", TypeID::IntID, keyBitCount },
        ColumnInfo{ "AD_new", TypeID::IntID, keyBitCount },
    }*/
    ;


    Table voter1Data, dmv1Data;
    Table voter2Data, dmv2Data;

    voter1Data.init(n, voterSchema);
    dmv1Data.init(n, dvmSchema);

    voter2Data.init(n, voterSchema);
    dmv2Data.init(n, dvmSchema);

    PRNG prng(ZeroBlock);
    
    for (auto& c : voter1Data.mColumns) prng.get<i64>(c.mData.data(), c.mData.size());
    for (auto& c : voter2Data.mColumns) prng.get<i64>(c.mData.data(), c.mData.size());
    for (auto& c : dmv1Data.mColumns) prng.get<i64>(c.mData.data(), c.mData.size());
    for (auto& c : dmv2Data.mColumns) prng.get<i64>(c.mData.data(), c.mData.size());

    auto routine = [&](int i)
    {
        auto& srv = srvs[i];

        SharedTable voter1, voter2, dmv1, dmv2;

        if (i == 0)
        {
            voter1 = srv.localInput(voter1Data);
            voter2 = srv.localInput(voter2Data);
            dmv1 = srv.localInput(dmv1Data);
            dmv2 = srv.localInput(dmv2Data);
        }
        else
        {
            voter1 = srv.remoteInput(0);
            voter2 = srv.remoteInput(0);
            dmv1 = srv.remoteInput(0);
            dmv2 = srv.remoteInput(0);
        }

        Timer t;
        t.setTimePoint("start");

        auto select1 = std::vector<SharedTable::ColRef>{ dmv1["NA"], dmv1["SSN"], dmv1["AD"] };
        auto select2 = std::vector<SharedTable::ColRef>{ dmv2["NA"], dmv2["SSN"], dmv2["AD"] };
        auto state1 = srv.leftJoin(dmv1["DL"], voter1["DL"], select1, "registed");
        auto state2 = srv.leftJoin(dmv2["DL"], voter2["DL"], select2, "registed");


        SelectQuery select;
        auto SSN = select.joinOn(state1["SSN"], state2["SSN"]);
        auto date1 = select.addInput(state1["AD"]);
        auto date2 = select.addInput(state2["AD"]);
        auto reg1 = select.addInput(state1["registed"]);
        auto reg2 = select.addInput(state2["registed"]);

        auto older1 = date1 < date2;
        auto older2 = !older1;
        auto doubleReg = reg1 & reg2;
        auto reveal1 = doubleReg | (older1 & reg1);
        auto reveal2 = doubleReg | (older2 & reg2);

        select.addOutput("NA1", select.addInput(state1["NA"]) * reveal1);
        select.addOutput("NA2", select.addInput(state2["NA"]) * reveal2);

        auto intersection = srv.joinImpl(select);
        t.setTimePoint("end");

        if(i ==0)
            std::cout << "n = " << n << "\n" << t << std::endl;
    };




    std::thread t0 = std::thread(routine, 0);
    std::thread t1 = std::thread(routine, 1);
    routine(2);

    t0.join();
    t1.join();

}
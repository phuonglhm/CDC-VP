#include "Coding_Reconstruction_Test/communication_bench.h"
int sc_main(int argc,char** argv) {
    try {
        std::string scenario=argc>3?argv[3]:"normal";
        communication_test::Bench bench("bench",true,argc>1?std::stoul(argv[1]):32,
            argc>2?std::stoul(argv[2]):50,scenario,32,scenario=="wrap"?4:3,true);
        sc_core::sc_start();return bench.passed?0:1;
    } catch(const std::exception& e) {std::cerr<<e.what()<<"\n";return 1;}
}

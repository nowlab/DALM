#include <iostream>
#include <cstdlib>
#include "dalm.h"
#include "../externals/ezOptionParser.hpp"

bool parse_args(ez::ezOptionParser &opt, int argc, char const **argv){
    opt.overview = "DALM validator\n  an error check tool";
    opt.syntax = "-f arpa -d dalm";
    opt.example = "  \n\n    > validate_dalm -f foo.lm -d foo.dalm\n\n";
    opt.footer = "Double Array Language Model (DALM)\n";

    opt.add("", true, 1, 0, "ARPA File", "-f", "--arpa");
    opt.add("", true, 1, 0, "DALM directory", "-d", "--dalm");

    opt.parse(argc, argv);
    std::vector<std::string> bad_required, bad_expected;
    opt.gotRequired(bad_required);
    opt.gotExpected(bad_expected);
    if(bad_required.size() > 0 || bad_expected.size() > 0){
        std::string usage;
        opt.getUsage(usage);
        std::cerr << usage << std::endl;
        return false;
    }
    return true;
}

int main(int argc, char const **argv) {
    ez::ezOptionParser opt;
    if(!parse_args(opt, argc, argv)){
        return 1;
    }

    std::string arpa;
    std::string dalm;

    opt.get("--arpa")->getString(arpa);
    opt.get("--dalm")->getString(dalm);

    DALM::Logger logger(stderr);
    DALM::Model lm(dalm, logger);
    lm.lm->errorcheck(arpa);
}
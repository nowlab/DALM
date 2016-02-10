#include <cstdio>
#include <cstdlib>

#include <string>
#include <iostream>

#include "../externals/ezOptionParser.hpp"
#include "dalm.h"
#include "dalm/buildutil.h"

bool parse_args(ez::ezOptionParser &opt, int argc, char const **argv){
    opt.overview = "DALM text exporter";
    opt.syntax = "-d dalm -o output";
    opt.example = "  export DALM to text file.\n\n    > export_dalm -d foo.lm -o foo.txt\n\n";
    opt.footer = "Double Array Language Model (DALM)\n";

    opt.add("", true, 1, 0, "DALM dir", "-d", "--dalm");
    opt.add("", true, 1, 0, "Output", "-o", "--output");

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

int main(int argc, char const **argv){
    ez::ezOptionParser opt;
    if(!parse_args(opt, argc, argv)){
        return 1;
    }

    srand(432341);
    DALM::Logger logger(stderr);

    std::string dalm;
    std::string output;
    opt.get("--dalm")->getString(dalm);
    opt.get("--output")->getString(output);

    DALM::Model m(dalm, logger);
    m.lm->export_to_text(output);

    return 0;
}


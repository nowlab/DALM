#include <cstdio>
#include <cstdlib>

#include <string>
#include <iostream>

#include "../externals/ezOptionParser.hpp"
#include "dalm.h"
#include "dalm/buildutil.h"

bool parse_args(ez::ezOptionParser &opt, int argc, char const **argv){
    opt.overview = "DALM builder";
    opt.syntax = "-m method -f arpa -o output [-d n_divide] [-c n_cores]";
    opt.example = "  build DALM with reverse trie divided by 4 parts using 2cores.\n\n    > build_dalm -f reverse -f foo.lm -o foo.dalm -d 4 -c 2\n\n";
    opt.footer = "Double Array Language Model (DALM)\n";

    opt.add("", true, 1, 0, "Optimization method (bst, reverse)", "-m", "--method");
    opt.add("", true, 1, 0, "ARPA File", "-f", "--arpa");
    opt.add("", true, 1, 0, "Output", "-o", "--output");
    opt.add("4", false, 1, 0, "# of divids", "-d", "--div");
    opt.add("-1", false, 1, 0, "# of cores", "-c", "--cores");
    opt.add("1024", false, 1, 0, "Memory Limit on Sort in MB (default: 1024)", "-l", "--limit");

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

    std::string method;
    std::string arpa;
    int n_div;
    int n_cores;
    std::size_t memory_limit;
    std::string outdir;
    opt.get("--method")->getString(method);
    opt.get("--arpa")->getString(arpa);
    opt.get("--output")->getString(outdir);
    opt.get("--div")->getInt(n_div);
    opt.get("--cores")->getInt(n_cores);
    opt.get("--limit")->getULong(memory_limit);
    memory_limit *= (1024 * 1024); // convert to Byte.

    if(n_div >= 256){
        throw std::runtime_error("# of div >= 256");
    }
    build_dalm(outdir, arpa, method, (unsigned char) n_div, n_cores, memory_limit, logger);

    return 0;
}


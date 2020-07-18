#include<string>

namespace DALM{
    class Vocabulary;
    class Logger;
    void build_bst(std::string tempdir, std::string arpafile, std::string outpath, Vocabulary &vocab, Logger &logger, std::size_t memory_limit = 1024*1024*10);
    void build_dalm(std::string outdir, std::string arpa, std::string opt, unsigned char n_div, int n_cores, std::size_t memory_limit, Logger &logger);
}


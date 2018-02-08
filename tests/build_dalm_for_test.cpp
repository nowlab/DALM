#include <sys/stat.h>
#include <dirent.h>
#include <sys/types.h>
#include "../src/dalm.h"
#include "../src/dalm/buildutil.h"
#include "dalm_test_config.h"

DALM::Model *build_dalm_for_test(std::string suffix, std::string mode, DALM::Logger &logger, std::size_t n_div) {
    std::string test_files = std::string(SOURCE_ROOT) + "/tests/data/test_files.txt";
    std::ifstream ifs(test_files.c_str());
    std::string arpa;

    std::getline(ifs, arpa);
    arpa = std::string(SOURCE_ROOT) + "/" + arpa;

    std::string outdir = std::string(BINARY_ROOT) + "/test.dalm." + suffix;

    struct stat s;
    if(stat(outdir.c_str(), &s) == 0) {
        std::cerr << "Cleaning previous test." << std::endl;
        DIR *dp = opendir(outdir.c_str());
        if(dp != NULL){
            for(struct dirent *e = readdir(dp);
                e != NULL; e = readdir(dp)){
                std::string name = e->d_name;
                if(name == "." || name == "..") continue;

                std::string path = outdir + "/" + name;
                std::cerr << "Removing " << path << std::endl;
                remove(path.c_str());
            }

            closedir(dp);
        }

        remove(outdir.c_str());
    }

    build_dalm(outdir, arpa, mode, n_div, 0, 1024*1024, logger, 1000, 31, 32);

    DALM::Model *lm = new DALM::Model(outdir, logger);

    return lm;
}

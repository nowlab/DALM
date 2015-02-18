#include<iostream>
#include<fstream>
#include"../include/arpafile.h"

void build_and_write(char const *arpa, char const *worddict, char const *wordids){
    std::vector<std::string> words;
    std::vector<DALM::VocabId> ids;
    DALM::ARPAFile::create_vocab_and_order(arpa, words, ids);

    std::ofstream wfs(worddict);
    std::ofstream ifs(wordids);
    for(std::size_t i = 0; i < words.size(); ++i){
        wfs << words[i] << std::endl;
        ifs << ids[i] << std::endl;
    }
    wfs.close();
    ifs.close();
}

int main(int argc, char **argv){
    if(argc != 4){
        std::cerr << "Usage: " << argv[0] << " arpa worddict wordids" << std::endl;
        return 1;
    }

    char *arpa = argv[1];
    char *worddict = argv[2];
    char *wordids = argv[3];
    build_and_write(arpa, worddict, wordids);

    return 0;
}
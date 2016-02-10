#include<cstdio>

#include<iostream>
#include<fstream>
#include<sstream>
#include <ezOptionParser.hpp>

#include "../src/dalm.h"

using namespace std;

void push(DALM::VocabId *ngram, size_t n, DALM::VocabId wid){
    for(size_t i = n-1; i+1 >= 1 ; i--){
        ngram[i] = ngram[i-1];
    }
    ngram[0] = wid;
}

int query_without_state(std::string path){
    ////////////////
    // LOADING LM //
    ////////////////

    // Preparing a logger object.
    DALM::Logger logger(stderr);
    logger.setLevel(DALM::LOGGER_INFO);

    // Load ini file.
    DALM::Model ini_file(path, logger);

    // Load order.
    unsigned char order = ini_file.order;

    // Load the vocabulary file.
    DALM::Vocabulary &vocab = *ini_file.vocab;

    // Load the language model.
    DALM::LM &lm = *ini_file.lm;

    ////////////////
    // WORD LIST  //
    ////////////////
    ifstream ifs(ini_file.wordstxt.c_str());
    string word;
    size_t word_count = 0;

    // a wordstxt file contains a word per line.
    getline(ifs, word);
    while(ifs){
        word_count++;
        getline(ifs, word);
    }
    cerr << "WORD COUNT=" << word_count << endl;


    //////////////
    // QUERYING //
    //////////////
    string stagstart = "<s>";
    string stagend = "</s>";
    DALM::VocabId wid_start = vocab.lookup(stagstart.c_str());
    DALM::VocabId wid_end = vocab.lookup(stagend.c_str());
    string line;
    DALM::VocabId ngram[DALM_MAX_ORDER];
    
    getline(cin, line);
    while(cin){
        for(size_t i = 0; i < order; i++){
            ngram[i] = wid_start;
        }
        istringstream iss(line);
        string word;
        iss >> word;
        while(iss){
            // GETTING WORDID
            // If the word is an unknown word, WORDID is Vocabulary::unk().
            DALM::VocabId wid = vocab.lookup(word.c_str());
            push(ngram, order, wid);

            float prob = 0.0;
            // QUERYING
            // Note that the ngram array is in reverse order.
            prob = lm.query(ngram, order);

            cout << word << " => " << prob << endl;

            iss >> word;
        }
        push(ngram, order, wid_end);
        float prob = lm.query(ngram, order);
        cout << stagend << " => " << prob << endl;
        
        getline(cin, line);
    }

    return 0;
}

int query_with_state(std::string path){
    ////////////////
    // LOADING LM //
    ////////////////

    // Preparing a logger object.
    DALM::Logger logger(stderr);
    logger.setLevel(DALM::LOGGER_INFO);

    // Load ini file.
    DALM::Model ini_file(path, logger);

    // Load the vocabulary file.
    DALM::Vocabulary &vocab = *ini_file.vocab;

    // Load the language model.
    DALM::LM &lm = *ini_file.lm;

    ////////////////
    // WORD LIST  //
    ////////////////
    ifstream ifs(ini_file.wordstxt.c_str());
    string word;
    size_t word_count = 0;

    // a wordstxt file contains a word per line.
    getline(ifs, word);
    while(ifs){
        word_count++;
        getline(ifs, word);
    }
    cerr << "WORD COUNT=" << word_count << endl;

    // Prepare a state.
    DALM::State state;

    //////////////
    // QUERYING //
    //////////////
    string stagend = "</s>";
    DALM::VocabId wid_end = vocab.lookup(stagend.c_str());
    string line;

    getline(cin, line);
    while(cin){
        // initialize the state.
        lm.init_state(state);

        istringstream iss(line);
        string word;
        iss >> word;
        while(iss){
            // GETTING WORDID
            // If the word is an unknown word, WORDID is Vocabulary::unk().
            DALM::VocabId wid = vocab.lookup(word.c_str());

            float prob = 0.0;
            // QUERYING
            prob = lm.query(wid, state);

            cout << word << " => " << prob << endl;

            iss >> word;
        }
        float prob = lm.query(wid_end, state);
        cout << stagend << " => " << prob << endl;

        getline(cin, line);
    }

    return 0;
}

bool parse_args(ez::ezOptionParser &opt, int argc, char const **argv){
    opt.overview = "DALM query example";
    opt.syntax = "[--state] arpa";
    opt.example = "    > query_dalm --state --dalm foo.lm < input.txt > output.txt\n\n";
    opt.footer = "Double Array Language Model (DALM)\n";

    opt.add("", false, 0, 0, "Use state", "-s", "--state");
    opt.add("", true, 1, 0, "DALM path (directory)", "-d", "--dalm");

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
    parse_args(opt, argc, argv);

    std::string path;
    opt.get("--dalm")->getString(path);;
    if(opt.isSet("-s")){
        query_with_state(path);
    }else{
        query_without_state(path);
    }
}

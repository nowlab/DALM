#include<cstdio>

#include<iostream>
#include<fstream>
#include<sstream>
#include<chrono>
#include <ezOptionParser.hpp>

#include "../src/dalm.h"

using namespace std;

void push(DALM::VocabId *ngram, size_t n, DALM::VocabId wid){
    for(size_t i = n-1; i+1 >= 1 ; i--){
        ngram[i] = ngram[i-1];
    }
    ngram[0] = wid;
}

void print_ngram(DALM::VocabId* ngram,unsigned char order){
    cout << (int)order << "-gram query( "; 
    for(int i = 0; i < (int)order; i++) cout << ngram[i] << " ";
    cout << ")" << endl;
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
    long long time_sum = 0;
    string stagstart = "<s>";
    string stagend = "</s>";
    DALM::VocabId wid_start = vocab.lookup(stagstart.c_str());
    DALM::VocabId wid_end = vocab.lookup(stagend.c_str());
    string line;
    DALM::VocabId ngram[DALM_MAX_ORDER];
    
    getline(cin, line);
    while(cin){
        float total = 0.0f;
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
            // print_ngram(ngram, order);
            auto start = chrono::system_clock::now();
            prob = lm.query(ngram, order);
            auto end = chrono::system_clock::now();
            time_sum += std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();


            cout << word << " => " << prob << endl;
            total += prob;

            iss >> word;
        }
        push(ngram, order, wid_end);
        float prob = lm.query(ngram, order);
        cout << stagend << " => " << prob << endl;
        total += prob;
        cout << "total => " << total << endl;
        
        getline(cin, line);
    }

    cout << "querying time: " << time_sum << " msec." << endl;
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
    auto start = chrono::system_clock::now();
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

    auto end = chrono::system_clock::now();
    auto diff = end - start;
    cout << "querying time: " << chrono::duration_cast<chrono::milliseconds>(diff).count() << " msec." << endl;
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
    if(!parse_args(opt, argc, argv)){
        return 1;
    }

    std::string path;
    opt.get("--dalm")->getString(path);;
    if(opt.isSet("-s")){
        query_with_state(path);
    }else{
        query_without_state(path);
    }
}

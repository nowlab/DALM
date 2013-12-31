#include<cstdio>

#include<iostream>
#include<string>
#include<fstream>
#include<sstream>

#include<logger.h>
#include<vocabulary.h>
#include<dalm.h>

#define NGRAMORDER 5

using namespace std;

void push(DALM::VocabId *ngram, size_t n, DALM::VocabId wid){
	for(size_t i = n-1; i+1 >= 1 ; i--){
		ngram[i] = ngram[i-1];
	}
	ngram[0] = wid;
}

void read_ini(const char *inifile, string &model, string &words, string &wordstxt){
	ifstream ifs(inifile);
	string line;

	getline(ifs, line);
	while(ifs){
		unsigned int pos = line.find("=");
		string key = line.substr(0, pos);
		string value = line.substr(pos+1, line.size()-pos);
		if(key=="MODEL"){
			model = value;
		}else if(key=="WORDS"){
			words = value;
		}else if(key=="WORDSTXT"){
			wordstxt = value;
		}
		getline(ifs, line);
	}
}

int main(int argc, char **argv){
	if(argc != 2){
		cerr << "Usage : " << argv[0] << " dalm-ini-file" << endl;
		return 1;
	}
	string path = argv[1];
    string inifile= path + "/dalm.ini";

	/////////////////////
	// READING INIFILE //
	/////////////////////
	string model; // Path to the double-array file.
	string words; // Path to the vocabulary file.
	string wordstxt; // Path to the vocabulary file in text format.
	read_ini(inifile.c_str(), model, words, wordstxt);
	
	model = path + "/" + model;
    words = path + "/" + words;
    wordstxt = path + "/" + wordstxt;

	////////////////
	// WORD LIST  //
	////////////////
	ifstream ifs(wordstxt.c_str());
	string word;
	size_t word_count = 0;

	// a wordstxt file contains a word per line.
	getline(ifs, word);
	while(ifs){
		word_count++;
		getline(ifs, word);
	}
	cerr << "WORD COUNT=" << word_count << endl;

	////////////////
	// LOADING LM //
	////////////////

	// Preparing a logger object.
	DALM::Logger logger(stderr);
	logger.setLevel(DALM::LOGGER_INFO);

	// Load the vocabulary file.
	DALM::Vocabulary vocab(words, logger);

	// Load the language model.
	DALM::LM lm(model, vocab, logger);

	// Prepare a state.
	DALM::State state(NGRAMORDER);
	
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


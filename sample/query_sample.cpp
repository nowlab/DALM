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

void read_ini(char *inifile, string &model, string &words){
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
		}
		getline(ifs, line);
	}
}

int main(int argc, char **argv){
	if(argc != 2){
		cerr << "Usage : " << argv[0] << " dalm-ini-file" << endl;
		return 1;
	}
	char *inifile = argv[1];

	/////////////////////
	// READING INIFILE //
	/////////////////////
	string model; // Path to the double-array file.
	string words; // Path to the vocabulary file.
	read_ini(inifile, model, words);
	
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
	
	//////////////
	// QUERYING //
	//////////////
	string stagstart = "<s>";
	string stagend = "</s>";
	DALM::VocabId wid_start = vocab.lookup(stagstart.c_str());
	DALM::VocabId wid_end = vocab.lookup(stagend.c_str());
	string line;
	DALM::VocabId ngram[NGRAMORDER];
	
	getline(cin, line);
	while(cin){
		for(size_t i = 0; i < NGRAMORDER; i++){
			ngram[i] = wid_start;
		}
		istringstream iss(line);
		string word;
		iss >> word;
		while(iss){
			// GETTING WORDID
			// If the word is an unknown word, WORDID is DALM_UNK_WORD.
			DALM::VocabId wid = vocab.lookup(word.c_str());
			push(ngram, NGRAMORDER, wid);

			float prob = 0.0;
			// QUERYING
			// Note that the ngram array is in reverse order.
			prob = lm.query(ngram, NGRAMORDER);

			cout << word << " => " << prob << endl;

			iss >> word;
		}
		push(ngram, NGRAMORDER, wid_end);
		float prob = lm.query(ngram, NGRAMORDER);
		cout << stagend << " => " << prob << endl;
		
		getline(cin, line);
	}

	return 0;
}


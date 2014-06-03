#include <stdint.h>
#include <cstdio>
#include <cstdlib>

#include <string>
#include <iostream>

#include "logger.h"
#include "dalm.h"
#include "version.h"

using namespace std;

void build_DALM(int argc, char **argv, DALM::Logger &logger){
	char *opt = argv[1];
	char *arpa = argv[2];
	char *tree = argv[3];
	char *worddict = argv[4];
	char *wids = argv[5];
	char *binmodel = argv[6];
	char *binwdict = argv[7];
	size_t dividenum = atoi(argv[8]);
	
	unsigned int method=DALM_OPT_UNDEFINED;
	
	string optmethod(opt);
	if(optmethod=="embedding"){
		method = DALM_OPT_EMBEDDING;
	}
	
	logger << "Building vocabfile." << DALM::Logger::endi;	
	DALM::Vocabulary *vocab = new DALM::Vocabulary(string(worddict), string(binwdict), string(wids), 0, logger);

	logger << "Building DALM." << DALM::Logger::endi;
	DALM::LM *dalm = new DALM::LM(string(arpa), string(tree), *vocab, dividenum, method, logger);

	logger << "Dumping DALM." << DALM::Logger::endi;
	dalm->dump(string(binmodel));

	logger << "Dumping DALM end." << DALM::Logger::endi;

	delete dalm;
	delete vocab; 
}

int main(int argc, char **argv){
	if(argc != 9){
		cout << "Usage: " << argv[0] << " optmethod dumped-arpa-file dumped-tree-file worddict word-id-file output-binmodel output-binworddict divide-number" << endl;
		cout << " optmethod=embedding" << endl;
		return 1;
	}

	srand(432341);
	DALM::Logger logger(stderr);

	logger.setLevel(DALM::LOGGER_INFO);
	
	build_DALM(argc, argv, logger);

	return 0;
}


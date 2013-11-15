#include<cstdio>
#include<cstdlib>

#include<string>
#include<iostream>

#include<logger.h>
#include <dalm.h>

using namespace std;

void build_DALM(int argc, char **argv, DALM::Logger &logger){
	char *arpa = argv[1];
	char *tree = argv[2];
	char *worddict = argv[3];
	char *wids = argv[4];
	char *binmodel = argv[5];
	char *binwdict = argv[6];
	size_t dividenum = atoi(argv[7]);
	logger << "Building vocabfile." << DALM::Logger::endi;	
	DALM::Vocabulary *vocab = new DALM::Vocabulary(string(worddict),string(binwdict),string(wids), 0, logger);

	logger << "Building DALM." << DALM::Logger::endi;
	DALM::LM *dalm = new DALM::LM(string(arpa),string(tree),*vocab,dividenum,logger);

	logger << "Dumping DALM." << DALM::Logger::endi;
	dalm->dump(string(binmodel));

	logger << "Dumping DALM end." << DALM::Logger::endi;

	delete dalm;
	delete vocab; 
}

int main(int argc, char **argv){
	if(argc != 8){
		cout << "Usage: " << argv[0] << " dumped-arpa-file dumped-tree-file worddict word-id-file output-binmodel output-binworddict divide-number" << endl;
		return 1;
	}

	srand(432341);
	DALM::Logger logger(stderr);

	logger.setLevel(DALM::LOGGER_INFO);
	
	build_DALM(argc, argv, logger);

	return 0;
}


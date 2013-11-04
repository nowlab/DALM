#include<cstdio>

#include<string>
#include<iostream>

#include<logger.h>
#include<arpafile.h>
#include<treefile.h>

using namespace std;

void dump_arpa(string arpaFile, string dumpFile, DALM::Logger &logger){
	DALM::ARPAFile::dump(arpaFile, dumpFile, logger);
}

void dump_tree(string treeFile, string dumpFile, DALM::Logger &logger){
	DALM::TreeFile::dump(treeFile, dumpFile, logger);
}

int main(int argc, char **argv){
	if(argc != 4){
		cout << "Usage: " << argv[0] << " --arpa arpa-file output-dump-file" << endl;
		cout << "Usage: " << argv[0] << " --tree tree-file output-dump-file" << endl;
		return 1;
	}
	
	DALM::Logger logger(stderr);
	logger.setLevel(DALM::LOGGER_INFO);
	
	logger << "Dump Start." << DALM::Logger::endi;
	
	if(string(argv[1])=="--arpa"){
		dump_arpa(string(argv[2]), string(argv[3]), logger);
	}else if(string(argv[1])=="--tree"){
		dump_tree(string(argv[2]), string(argv[3]), logger);
	}else{
		logger << "Unknown Format." << DALM::Logger::endc;
		return 1;
	}
	
	logger << "Dump Done." << DALM::Logger::endi;
	
	return 0;
}

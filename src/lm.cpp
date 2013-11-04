#include <cmath>

#include <vector>

#include <lm.h>
#include <arpa_file.h>

using namespace DALM;

LM::LM(std::string pathtoarpa, std::string pathtotree, Vocabulary &vocab, size_t dividenum, Logger &logger)
		:vocab(vocab), logger(logger) {

	logger << "[LM::LM] TEXTMODE begin." << Logger::endi;
	logger << "[LM::LM] pathtoarpa=" << pathtoarpa << Logger::endi;
	logger << "[LM::LM] pathtotree=" << pathtotree << Logger::endi;

	build(pathtoarpa, pathtotree, dividenum);
	logger << "[LM::LM] end." << Logger::endi;
}

LM::LM(std::string dumpfilepath,Vocabulary &vocab, Logger &logger)
	:vocab(vocab), logger(logger){

	logger << "[LM::LM] BINMODE begin." << Logger::endi;
	logger << "[LM::LM] dumpfilepath=" << dumpfilepath << Logger::endi;

	FILE *fp = fopen(dumpfilepath.c_str(), "rb");
	if(fp != NULL){
		readParams(fp);
		fclose(fp);
	} else {
		logger << "[LM::LM] Dump file may have IO error." << Logger::endc;
		throw "error.";
	}
	logger << "[LM::LM] end." << Logger::endi;
}

LM::~LM() {
	for(size_t i = 0; i < danum; i++){
		delete da[i];
	}
	delete [] da;
}

float LM::query(VocabId *ngram, size_t n){
	if(ngram[0] == DALM_UNK_WORD){
		return 0.0;
	}
	for(size_t i=1;i<n;i++){
		if(ngram[i] == DALM_UNK_WORD){
			n=i;
			break;
		}
	}
	
	size_t daid = (n==1)?(ngram[0]%danum):(ngram[1]%danum);
	return da[daid]->get_prob((int*)ngram,n-1);
}

std::set<float> **LM::make_value_sets(std::string &pathtoarpa, size_t dividenum){
	std::set<float> **value_sets = new std::set<float>*[dividenum];
	for(size_t i = 0; i < dividenum; i++){
		value_sets[i] = new std::set<float>;
		value_sets[i]->insert(0.0);
	}

  ARPAFile *arpafile = new ARPAFile(pathtoarpa, vocab);
  size_t total = arpafile->get_totalsize();
  for(size_t i=0;i<total;i++){
    unsigned short n;
    VocabId *ngram;
    float prob;
    float bow;
    bool bow_presence;
    bow_presence = arpafile->get_ngram(n,ngram,prob,bow);

		if(bow_presence){
			size_t daid = ngram[n-1]%danum;
			value_sets[daid]->insert(bow);
		}
    delete [] ngram;
  }
  delete arpafile;

  return value_sets;
}

void LM::errorcheck(std::string &pathtoarpa){
  ARPAFile *arpafile = new ARPAFile(pathtoarpa, vocab);
  logger << "[LM::build] DoubleArray building error check." << Logger::endi;
  int error_num = 0;
  size_t total = arpafile->get_totalsize();
  for(size_t i=0;i<total;i++){
    unsigned short n;
    VocabId *ngram;
    float prob;
    float bow;
    bool bow_presence;
    bool builderror;
    bow_presence = arpafile->get_ngram(n,ngram,prob,bow);
		size_t daid = (n==1)?ngram[0]%danum:ngram[n-2]%danum;
    builderror = da[daid]->checkinput(n,ngram,bow,prob,bow_presence);
    if(!builderror){
      logger << "[LM::build] DA build error occured." << Logger::endi;
      logger << "[LM::build] error arpa line is " << i << Logger::endi;
      error_num++;
    }
    delete [] ngram;
  }
  delete arpafile;
  logger << "[LM::build] total error" << error_num << Logger::endi;
  logger << "[LM::build] DoubleArray building error check end." << Logger::endi;
}

void LM::build(std::string &pathtoarpa, std::string &pathtotreefile, size_t dividenum){
	logger << "[LM::build] LM build begin." << Logger::endi;
	danum = dividenum;

	std::set<float> **value_sets = make_value_sets(pathtoarpa, dividenum);

	logger << "[LM::build] DoubleArray build begin." << Logger::endi;
	da = new DA*[dividenum];
	TreeFile **tf = new TreeFile*[dividenum];
	DABuilder **builder = new DABuilder*[dividenum];
	
	for(size_t i = 0; i < dividenum; i++){
		da[i] = new DA(i, dividenum, da, logger); 
		tf[i] = new TreeFile(pathtotreefile, vocab);
		builder[i] = new DABuilder(da[i], tf[i], value_sets[i], vocab.size());
	}

	logger << "[LM::build] Make Double Array from trie." << Logger::endi;
	size_t threads = PThreadWrapper::thread_available();
	size_t running = 0;
	for(size_t i = 0; i < dividenum; i++){
		builder[i]->start();
		running++;
		if(running%threads==0){
			while(true){
				size_t runcounter = 0;
				for(size_t j = 0; j < i; j++){
					if(builder[i]->is_running()) runcounter++;
				}
				if(runcounter<running){
					running = runcounter;
					break;
				}
			}
		}
	}
	for(size_t i = 0; i < dividenum; i++){
		builder[i]->join();
	}

	logger << "[LM::build] DoubleArray build end." << Logger::endi; 	
	errorcheck(pathtoarpa);

	for(size_t i = 0; i < dividenum; i++){
		delete builder[i];
		delete tf[i];
		delete value_sets[i];
	}
	delete [] builder;
	delete [] tf;
	delete [] value_sets;
	
	logger << "[LM::build] LM build end." << Logger::endi;
}

void LM::dumpParams(FILE *fp){
	fwrite(&danum, sizeof(size_t), 1, fp);
	for(size_t i = 0; i < danum; i++){
		da[i]->dump(fp);
	}
}

void LM::readParams(FILE *fp){
	fread(&danum, sizeof(size_t), 1, fp);
	da = new DA*[danum];
	for(size_t i = 0; i < danum; i++){
		da[i] = new DA(fp, da, logger);
	}
}


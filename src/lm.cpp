#include <cmath>

#include <unistd.h>
#include <vector>

#include "dalm.h"
#include "arpafile.h"
#include "value_array.h"
#include "value_array_index.h"
#include "version.h"
#include "pthread_wrapper.h"

using namespace DALM;

LM::LM(std::string pathtoarpa, std::string pathtotree, Vocabulary &vocab, size_t dividenum, Logger &logger)
		:value_array(NULL), vocab(vocab), logger(logger) {

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
		Version v(fp, logger); // Version check.
		value_array = new ValueArray(fp, logger);
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
	if(value_array != NULL) delete value_array;
}

float LM::query(VocabId *ngram, size_t n){
	size_t daid = (n==1)?(ngram[0]%danum):(ngram[1]%danum);
	return da[daid]->get_prob(ngram, n);
}

float LM::query(VocabId word, State &state){
	return da[state.get_daid()]->get_prob(word, state);
}

float LM::query(VocabId word, State &state, Fragment &f){
	return da[state.get_daid()]->get_prob(word, state, f);
}

float LM::query(VocabId word, const Fragment &f, State &state_left, Gap &gap){
	return da[((uint64_t)f.get_state_id())>>32]->get_prob(word, f, state_left, gap);
}

float LM::query(VocabId word, const Fragment &fprev, State &state_left, Gap &gap, Fragment &fnew){
	return da[((uint64_t)fprev.get_state_id())>>32]->get_prob(word, fprev, state_left, gap, fnew);
}

void LM::init_state(State &state){
	VocabId ngram[1];
	std::string stagstart = "<s>";
	ngram[0] = vocab.lookup(stagstart.c_str());
	size_t daid = ngram[0] % danum;
	da[daid]->init_state(ngram, 1, state);
}

void LM::set_state(VocabId *ngram, size_t n, State &state){
	size_t daid = ngram[0] % danum;
	da[daid]->init_state(ngram, n, state);
}

void LM::set_state(State &state, State &state_prev, Gap &gap){
	da[state.get_daid()]->init_state(state, state_prev, gap);
}

/* depricated */
StateId LM::get_state(VocabId *ngram, size_t n){
	for(size_t i=0;i<n;i++){
		if(ngram[i] == vocab.unk()){
			n=i;
			break;
		}
	}

	size_t daid = ngram[0]%danum;
	return ((daid << 32)|da[daid]->get_state(ngram,n));
}

void LM::errorcheck(std::string &pathtoarpa){
  ARPAFile *arpafile = new ARPAFile(pathtoarpa, vocab);
  logger << "[LM::errorcheck] start." << Logger::endi;
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
      logger << "[LM::build] Error: line=" << i << Logger::ende;
      error_num++;
    }
    delete [] ngram;
  }
  delete arpafile;
  logger << "[LM::build] # of errors: " << error_num << Logger::endi;
}

void LM::build(std::string &pathtoarpa, std::string &pathtotreefile, size_t dividenum){
	logger << "[LM::build] LM build begin." << Logger::endi;
	danum = dividenum;

	value_array = new ValueArray(pathtoarpa, vocab, logger);
	ValueArrayIndex value_array_index(*value_array);

	logger << "[LM::build] DoubleArray build begin." << Logger::endi;
	da = new DA*[dividenum];
	TreeFile **tf = new TreeFile*[dividenum];
	DABuilder **builder = new DABuilder*[dividenum];
	
	for(size_t i = 0; i < dividenum; i++){
		da[i] = new DA(i, dividenum, *value_array, da, logger); 
		tf[i] = new TreeFile(pathtotreefile, vocab);
		builder[i] = new DABuilder(da[i], tf[i], value_array_index, vocab.size());
	}

	logger << "[LM::build] Make Double Array." << Logger::endi;
	size_t threads = PThreadWrapper::thread_available();
	logger << "[LM::build] threads available=" << threads << Logger::endi;
	size_t running = 0;
	for(size_t i = 0; i < dividenum; i++){
		builder[i]->start();
		running++;
		if(running>=threads){
			while(true){
				usleep(500);
				size_t runcounter = 0;
				for(size_t j = 0; j <= i; j++){
					if(!builder[j]->is_finished()) runcounter++;
				}
				if(runcounter<threads){
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
	}
	delete [] builder;
	delete [] tf;
	
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
		da[i] = new DA(fp, *value_array, da, logger);
	}
}


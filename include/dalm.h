#ifndef DALM_H_
#define DALM_H_

#include <string>
#include <fstream>
#include <vector>
#include <set>

#include "logger.h"
#include "vocabulary.h"
#include "da.h"
#include "handler.h"
#include "version.h"
#include "state.h"
#include "fragment.h"
#include "fileutil.h"
#include "vocabulary_file.h"

typedef unsigned long int StateId;

namespace DALM {
	class LM {
		public:
			LM(std::string pathtoarpa, std::string pathtotreefile, Vocabulary &vocab, size_t dividenum, unsigned int opt, Logger &logger);
			LM(std::string dumpfilepath, Vocabulary &vocab, unsigned char order, Logger &logger);
			virtual ~LM();
			
			float query(VocabId *ngram, size_t n);
			float query(VocabId word, State &state);

			float query(VocabId word, State &state, Fragment &f);
			float query(const Fragment &f, State &state_prev, Gap &gap);
			float query(const Fragment &fprev, State &state_prev, Gap &gap, Fragment &fnew);

			void init_state(State &state);
			void set_state(VocabId *ngram, size_t n, State &state);
			void set_state(State &state, const State &state_prev, const Fragment *fragments, const Gap &gap);
			
			/* depricated */
			StateId get_state(VocabId *ngram, size_t n);
			
			void dump(std::string dumpfilepath){
				logger << "[LM::dump] start dumping to file(" << dumpfilepath << ")" << Logger::endi;
				FILE *fp = std::fopen(dumpfilepath.c_str(),"wb");
				v.dump(fp);
				dumpParams(fp);
				fclose(fp);
				logger << "[LM::dump] File-dump done." << Logger::endi;
			}

		private:
			void errorcheck(std::string &pathtoarpa);
			void build(std::string &pathtoarpa, std::string &pathtotreefile, size_t dividenum);
			void dumpParams(FILE *fp);
			void readParams(BinaryFileReader &reader, unsigned char order);

			DA **da;
			Vocabulary &vocab;
			Version v;
			DAHandler *handler;
			Logger &logger;
			VocabId stagid;
	};
	class Model {
	public:
	    Model(std::string basedir, unsigned char order, Logger &logger);
	    virtual ~Model(){
	        delete lm;
	        delete vocab;
	        delete vocab_file;
	    }
        std::string model;
	    std::string words;
	    std::string wordstxt;
	    Vocabulary *vocab;
	    LM *lm;
	    VocabularyFile *vocab_file;
	    unsigned char order;
	};
}

#endif /* DALM_H_ */


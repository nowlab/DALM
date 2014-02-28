#ifndef DALM_H_
#define DALM_H_

#include <string>
#include <fstream>
#include <vector>
#include <set>

#include "logger.h"
#include "vocabulary.h"
#include "da.h"
#include "version.h"
#include "state.h"

typedef unsigned long int StateId;

namespace DALM {
	class Fragment;

	class LM {
		friend class Fragment;

		public:
			LM(std::string pathtoarpa, std::string pathtotreefile, Vocabulary &vocab, size_t dividenum, Logger &logger);
			LM(std::string dumpfilepath, Vocabulary &vocab, Logger &logger);
			virtual ~LM();

			float query(VocabId *ngram, size_t n);
			float query(VocabId word, State &state);

			float query(VocabId word, State &state, Fragment &f);
			float query(const Fragment &f, State &state_prev, Gap &gap);
			float query(const Fragment &fprev, State &state_prev, Gap &gap, Fragment &fnew);

			void init_state(State &state);
			void set_state(VocabId *ngram, size_t n, State &state);
			void set_state(State &state, State &state_prev, Gap &gap);
			
			/* depricated */
			StateId get_state(VocabId *ngram, size_t n);

			void dump(std::string dumpfilepath){
				logger << "[LM::dump] start dumping to file(" << dumpfilepath << ")" << Logger::endi;
				FILE *fp = std::fopen(dumpfilepath.c_str(),"wb");
				Version v;
				v.dump(fp);
				value_array->dump(fp);
				dumpParams(fp);
				fclose(fp);
				logger << "[LM::dump] File-dump done." << Logger::endi;
			}

		private:
			void errorcheck(std::string &pathtoarpa);
			void build(std::string &pathtoarpa, std::string &pathtotreefile, size_t dividenum);
			void dumpParams(FILE *fp);
			void readParams(FILE *fp);

			DA **da;
			ValueArray *value_array;
			unsigned char danum;
			Vocabulary &vocab;
			Logger &logger;
	};
}

#endif /* DALM_H_ */

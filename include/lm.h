#ifndef DALM_H_
#define DALM_H_

#include <string>
#include <fstream>
#include <vector>
#include <set>

#include <logger.h>
#include <vocabulary.h>
#include <da.h>
#include "version.h"

namespace DALM {
	class LM {
		public:
			LM(std::string pathtoarpa, std::string pathtotreefile, Vocabulary &vocab, size_t dividenum, Logger &logger);
			LM(std::string dumpfilepath, Vocabulary &vocab, Logger &logger);
			virtual ~LM();

			float query(VocabId *ngram, size_t n);

			void dump(std::string dumpfilepath){
				logger << "[LM::dump] start dumping to file(" << dumpfilepath << ")" << Logger::endi;
				FILE *fp = std::fopen(dumpfilepath.c_str(),"wb");
				Version v;
				v.dump(fp);
				dumpParams(fp);
				fclose(fp);
				logger << "[LM::dump] File-dump done." << Logger::endi;
			}

		private:
			void errorcheck(std::string &pathtoarpa);
			std::set<float> **make_value_sets(std::string &pathtoarpa, size_t dividenum);
			void build(std::string &pathtoarpa, std::string &pathtotreefile, size_t dividenum);
			size_t query_index(VocabId *ngram, size_t n);
			void dumpParams(FILE *fp);
			void readParams(FILE *fp);

			DA **da;
			size_t danum;
			Vocabulary &vocab;
			Logger &logger;
	};
}

#endif /* DALM_H_ */

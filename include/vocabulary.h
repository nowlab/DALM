#ifndef VOCABULARY_H_
#define VOCABULARY_H_

#include<string>

#include<darts.h>

#include<logger.h>

#define DALM_UNK_WORD 0

namespace DALM {
	typedef unsigned int VocabId;
	
	typedef struct __vocab_list {
		char *vocab;
		struct __vocab_list *next;

		__vocab_list(){
			vocab=NULL;
			next=NULL;
		}
	} _VocabList;

	class Vocabulary {
		public:
			Vocabulary(std::string vocabfilepath, std::string savedabinpath, Logger &logger);
			Vocabulary(std::string arbit_vocab_filepath,std::string savedabinpath,std::string probnumfilepath, size_t offset, Logger &logger);
			Vocabulary(std::string dabinpath, Logger &logger);
			virtual ~Vocabulary();
			VocabId lookup(const char *vocab);
			size_t size(){
				//return da.size();
				if(wcount!=0){
					return wcount;
				}else{
					logger << "[Vocabulary::size] function 'size' does not supported on BINMODE" << Logger::ende;

					throw "UNSUPPORTED";
				}
			}
			VocabId unk(){ return unkid; }

		private:
			size_t read_file(std::string &vocabfilepath, char **&vocabarray);
			void delete_vocabulary(size_t vocabcount, char **&vocabarray);
			size_t wcount;
			VocabId unkid;
			Darts::DoubleArray da;
			Logger &logger;
	};
}

#endif /* VOCABULARY_H_ */

#ifndef DALM_H_
#define DALM_H_

#include <string>
#include <fstream>
#include <vector>
#include <set>
#include <glob.h>
#include "dalm/vocabulary.h"
#include "dalm/logger.h"
#include "dalm/state.h"
#include "dalm/fragment.h"
#include "dalm/fileutil.h"
#include "dalm/da.h"
#include "dalm/version.h"
#include "dalm/handler.h"
#include "dalm/vocabulary_file.h"

typedef unsigned long int StateId;

namespace DALM {
    class LM {
        public:
            LM(std::string pathtoarpa, std::string pathtotreefile, Vocabulary &vocab, unsigned int dividenum, unsigned int opt, std::string tmpdir, unsigned int n_cores, Logger &logger);
            LM(std::string dumpfilepath, Vocabulary &vocab, unsigned char order, Logger &logger);
            virtual ~LM();
            
            float query(VocabId *ngram, unsigned char n);
            float query(VocabId word, State &state);

            float query(VocabId word, State &state, Fragment &f);
            float query(const Fragment &f, State &state_prev, Gap &gap);
            float query(const Fragment &fprev, State &state_prev, Gap &gap, Fragment &fnew);
            float update(VocabId word, Fragment &f, bool &extended, bool &inedependent_left, bool &independent_right, float &bow);


            void init_state(State &state);
            void set_state(VocabId *ngram, unsigned char n, State &state);
            void set_state(State &state, const State &state_prev, const Fragment *fragments, const Gap &gap);

            float sum_bows(State &state, unsigned char begin, unsigned char end);
            
            /* depricated */
            StateId get_state(VocabId *ngram, unsigned char n);
            
            void dump(std::string dumpfilepath){
                logger << "[LM::dump] start dumping to file(" << dumpfilepath << ")" << Logger::endi;
                BinaryFileWriter writer(dumpfilepath);
                v.dump(writer);
                dumpParams(writer);
                logger << "[LM::dump] File-dump done." << Logger::endi;
            }

            void export_to_text(std::string exportfilepath){
                logger << "[LM::dump] start exporting to file(" << exportfilepath << ")" << Logger::endi;
                TextFileWriter writer(exportfilepath);
                exportParams(writer);
                logger << "[LM::dump] File-dump done." << Logger::endi;

            }
            std::size_t errorcheck(std::string &pathtoarpa);

        private:
            void build(std::string &pathtoarpa, std::string &pathtotreefile, unsigned int dividenum, std::string &tmpdir, unsigned int n_cores);
            void dumpParams(BinaryFileWriter &writer);
            void exportParams(TextFileWriter &writer);
            void readParams(BinaryFileReader &reader, unsigned char order);

            Vocabulary &vocab;
            Version v;
            DAHandler *handler;
            Logger &logger;
            VocabId stagid;
    };

    class Model {
    public:
        Model(std::string modeldir, unsigned char order, Logger &logger);
        Model(std::string modeldir, Logger &logger);
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
		std::string modeldir;
        Logger &logger;
    private:
        void load();
    };
}

#endif /* DALM_H_ */


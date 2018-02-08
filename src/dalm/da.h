#ifndef DA_H_
#define DA_H_

#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <utility>
#include <string>
#include <set>

#include "state.h"
#include "pthread_wrapper.h"
#include "treefile.h"
#include "value_array.h"
#include "value_array_index.h"
#include "fragment.h"
#include "logger.h"
#include "fileutil.h"
#include "trie_reader.h"

#define DALM_OOV_PROB -100.0f

#define DALM_N_BITS_UINT64_T 64UL

namespace DALM{
    typedef union {
        int base_val;
        float logprob;
        float logbow;
        uint32_t bits;
    } Base;

    typedef union {
        int check_val;
        float logprob;
        float logbow;
    } Check;

    typedef struct {
        Base base;
        Check check;
    } DAPair;

    typedef struct {
        int base;
        int check;
    } DASimple;

    typedef struct {
        float logp;
        float bow;
    } DAVal;

    typedef struct {
        int base;
        int check;
        float logprob;
        float logbow;
    } DABody;

    typedef struct {
        Base logprob;
        int check;
    } DATail;

    class DA{
        public:
            DA(){}
            virtual ~DA(){}

            virtual void make_da(std::string &pathtotreefile, ValueArrayIndex *value_array_index, Vocabulary &vocab) = 0;
            virtual void dump(BinaryFileWriter &writer) = 0;

            virtual void export_to_text(TextFileWriter &writer){
                throw std::runtime_error("not implemented");
            }

            virtual void write_and_free(BinaryFileWriter &writer) = 0;
            virtual void reload(BinaryFileReader &reader) = 0;

            virtual float get_prob(VocabId *word, unsigned char order) = 0;
            virtual float get_prob(VocabId word, State &state) = 0;
            virtual float get_prob(VocabId word, State &state, Fragment &f) = 0;
            virtual float get_prob(const Fragment &f, State &state, Gap &gap) = 0;
            virtual float get_prob(const Fragment &fprev, State &state, Gap &gap, Fragment &fnew) = 0;
            virtual float update(VocabId word, Fragment &f, bool &extended, bool &independent_left, bool &independent_right, float &bow){
                throw std::runtime_error("not implemented");
            }

            virtual void init_state(VocabId *word, unsigned char order, State &state) = 0;
            virtual void init_state(State &state, const State &state_prev, const Fragment *fragments, const Gap &gap) = 0;

            /* depricated */
            virtual unsigned long int get_state(VocabId *word, unsigned char order) = 0;
            virtual bool checkinput(unsigned short n,unsigned int *ngram,float bow,float prob,bool bow_presence) = 0;
            virtual void fill_inserted_node(std::string &pathtotreefile, Vocabulary &vocab) = 0;

            virtual float sum_bows(State &state, unsigned char begin, unsigned char end){
                float total = 0.0;
                for(unsigned char i = begin; i < end; ++i){
                    total += state.get_bow(i);
                }
                return total;
            }

    protected:
            unsigned char context_size;
    };

    class DABuilder : public PThreadWrapper {
        public:
            DABuilder(
                    DA *da,
                    std::string &pathtotreefile,
                    ValueArrayIndex *value_array_index,
                    Vocabulary &vocab,
                    std::string &tmpdir
                    ) :
                da(da),
                pathtotreefile(pathtotreefile), 
                value_array_index(value_array_index), 
                vocab(vocab),
                tmpdir(tmpdir),
                fp(NULL)
                {
                    finished=false;
                }
            
            virtual ~DABuilder(){
                if(fp!=NULL){
                    rewind(fp);
                    BinaryFileReader reader(fp);
                    da->reload(reader);
                }
            }

            virtual void run(){
                da->make_da(pathtotreefile, value_array_index, vocab);

                BinaryFileWriter *writer = BinaryFileWriter::get_temporary_writer(tmpdir);
                da->write_and_free(*writer);
                fp = writer->pointer();
                delete writer;

                finished=true;
            }

            bool is_finished(){
                return finished;
            }

        private:
            DA *da;
            std::string &pathtotreefile;
            ValueArrayIndex *value_array_index;
            Vocabulary &vocab;
            bool finished;
            std::string &tmpdir;
            FILE *fp;
    };

    class NodeFiller : public PThreadWrapper {
        public:
            NodeFiller(
                    DA *da,
                    std::string &pathtotreefile,
                    Vocabulary &vocab
                    ) :
                da(da),
                pathtotreefile(pathtotreefile), 
                vocab(vocab)
            {
                    finished=false;
            }

            virtual void run(){
                da->fill_inserted_node(pathtotreefile, vocab);
                finished = true;
            }

            bool is_finished(){
                return finished;
            }
    
        private:
            DA *da;
            std::string &pathtotreefile;
            Vocabulary &vocab;
            bool finished;
    };
}

#endif // DA_H_


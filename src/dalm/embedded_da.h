#ifndef QUAIL_DECODER_EMBEDDED_DA_H
#define QUAIL_DECODER_EMBEDDED_DA_H

#include "da.h"
#ifdef DALM_NEW_XCHECK
#include "bit_vector.h"
#endif

namespace DALM {
    class EmbeddedDA : public DA{
    public:
        EmbeddedDA(unsigned char daid, unsigned char datotal, ValueArray &value_array, EmbeddedDA **neighbours, Logger &logger);
        EmbeddedDA(BinaryFileReader &reader, ValueArray &value_array, EmbeddedDA **neighbours, unsigned char order, Logger &logger);
        virtual ~EmbeddedDA();

        virtual void write_and_free(BinaryFileWriter &writer);
        virtual void reload(BinaryFileReader &reader);

        virtual void make_da(std::string &pathtotreefile, ValueArrayIndex *value_array_index, Vocabulary &vocab);
        virtual void dump(BinaryFileWriter &writer);

        virtual float get_prob(VocabId *word, unsigned char order);
        virtual float get_prob(VocabId word, State &state);
        virtual float get_prob(VocabId word, State &state, Fragment &f);
        virtual float get_prob(const Fragment &f, State &state, Gap &gap);
        virtual float get_prob(const Fragment &fprev, State &state, Gap &gap, Fragment &fnew);

        virtual void init_state(VocabId *word, unsigned char order, State &state);
        virtual void init_state(State &state, const State &state_prev, const Fragment *fragments, const Gap &gap);

        /* depricated */
        virtual unsigned long int get_state(VocabId *word, unsigned char order);

        virtual bool checkinput(unsigned short n,unsigned int *ngram,float bow,float prob,bool bow_presence);
        virtual void fill_inserted_node(std::string &pathtotreefile, Vocabulary &vocab){};

        virtual float sum_bows(State &state, unsigned char begin, unsigned char end);

    private:
        void det_base(int *word,float *val,unsigned amount,unsigned now);
        int get_pos(int word,unsigned now);
        int get_terminal(unsigned now);
        void resize_array(unsigned newarray_size);

        void replace_value();

        bool prob_check(int length, int order, unsigned int *ngram, float prob);
        bool bow_check(int length, unsigned int *ngram, float bow);

        unsigned int array_size;
        DAPair *da_array;
        int *value_id;
        unsigned char daid;
        unsigned char datotal;
        ValueArray &value_array;
        EmbeddedDA **da;

        unsigned max_index;
        unsigned first_empty_index;

        Logger &logger;

        // Member from MatsuTaku
        size_t loop_counts_=0;
        size_t skip_counts_=0;
  #ifdef DALM_NEW_XCHECK
        // Additional bit array
        BitVector<1> empty_element_bits;
  #endif
        std::map<size_t, size_t> children_cnt_table_;
        std::map<size_t, double> children_fb_time_table_;
    };

    class EmbeddedDAHandler : public DAHandler{
    public:
        EmbeddedDAHandler(): value_array(NULL){}
        EmbeddedDAHandler(BinaryFileReader &reader, unsigned char order, Logger &logger){
            reader >> danum;
            da = new DA*[danum];
            value_array = new ValueArray(reader, logger);
            for(size_t i = 0; i < danum; i++){
                da[i] = new EmbeddedDA(reader, *value_array, (EmbeddedDA **) da, order, logger);
            }
        }
        virtual ~EmbeddedDAHandler(){
            if(value_array!=NULL) delete value_array;
        }

        virtual void build(unsigned int danum, std::string pathtoarpa, std::string &pathtotreefile,
                           Vocabulary &vocab, std::string &tmpdir, unsigned int n_cores, Logger &logger){
            DAHandler::danum=danum;

            value_array = new ValueArray(pathtoarpa, vocab, logger);
            ValueArrayIndex value_array_index(*value_array);

            da = new DA*[danum];
            DABuilder **builder = new DABuilder*[danum];

            for(size_t i = 0; i < danum; i++){
                da[i] = new EmbeddedDA(i, danum, *value_array, (EmbeddedDA **)da, logger);
                builder[i] = new DABuilder(da[i], pathtotreefile, &value_array_index, vocab, tmpdir);
            }

            logger << "[EmbeddedDAHandler::build] Building Double Array." << Logger::endi;
            run(builder, n_cores, logger);

            for(size_t i = 0; i < danum; i++){
                delete builder[i];
            }
            delete [] builder;
        }

        virtual float sum_bows(State &state, unsigned char begin, unsigned char end){
            return da[state.get_word(0)%danum]->sum_bows(state, begin, end);
        }

    protected:
        virtual size_t get_daid(VocabId *ngram, size_t n){
            return (n==1)?(ngram[0]%danum):(ngram[1]%danum);
        }

        virtual size_t get_daid(VocabId word, State &state){
            return (state.has_context())?(state.get_word(0)%danum):(word%danum);
        }

        virtual size_t get_daid(const Fragment &f, State &state_prev, Gap &g){
            if(state_prev.get_count()==0){
                throw "BUG!";
            }else{
                return state_prev.get_word(0)%danum;
            }
        }

        virtual size_t get_daid(const Fragment &f){
            throw "Not supported";
        }

        virtual size_t get_daid(State &state, const State &state_prev){
            if(state_prev.has_context()) return state_prev.get_word(0)%danum;
            else return 0;
        }

        virtual size_t get_state_daid(VocabId *ngram, size_t n){
            return ngram[0]%danum;
        }

        virtual size_t get_errorcheck_daid(VocabId *ngram, size_t n){
            return (n==1)?(ngram[0]%danum):(ngram[n-2]%danum);
        }

        virtual void dump_method_dependents(BinaryFileWriter &fp){
            value_array->dump(fp);
        }

    private:
        ValueArray *value_array;
    };

}

#endif //QUAIL_DECODER_EMBEDDED_DA_H

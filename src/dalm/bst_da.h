#ifndef QUAIL_DECODER_BST_DA_H
#define QUAIL_DECODER_BST_DA_H

#include "da.h"
#ifdef DALM_NEW_XCHECK
#include "bit_vector.h"
#endif

namespace DALM {
    union Val32 {
        float f;
        uint32_t u;
        int32_t i;
    };

    class BstDA : public DA{
    public:
        BstDA(unsigned int daid, unsigned int datotal, BstDA **neighbours, Logger &logger);
        BstDA(BinaryFileReader &reader, BstDA **neighbours, unsigned char order, Logger &logger);
        virtual ~BstDA();

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
        virtual void fill_inserted_node(std::string &path, Vocabulary &vocab);

        virtual float sum_bows(State &state, unsigned char begin, unsigned char end);

    private:
        struct Traverse {
            int index;
            Val32 base_and_independent_right;

            Traverse(){}
            Traverse(BstDA *da, int index){
                init(da, index);
            }

            void init(BstDA *da, int index=0){
                this->index = index;
                this->base_and_independent_right.i = da->da_array_[index].base.base_val;
            }

            bool independent_right() const{
                return base_and_independent_right.i < 0;
            }

            int base() const{
                return std::abs(base_and_independent_right.i);
            }
        };

        struct Target {
            float bow_;

            // if prob_and_independent_left > 0 then independent_left
            Val32 prob_and_independent_left;
            int index;

            Target(){
                prob_and_independent_left.f = DALM_OOV_PROB;
                index = 0;
            }

            bool independent_left() const{
                return prob_and_independent_left.f > 0;
            }

            bool invalid() const {
                return prob_and_independent_left.f == DALM_OOV_PROB;
            }

            float prob() const{
                Val32 p;
                p.u = (prob_and_independent_left.u | 0x80000000U);
                return p.f;
            }

            float bow(float bow_max) const {
                return bow_ + bow_max;
            }
        };

        void det_base(VocabId *words, DAPair *children, std::size_t n_children, float *&values, int context, int &head,
                      int &max_slot, int endmarker_pos, float node_bow, uint64_t *&validate, uint64_t words_prefix,
                      std::size_t prefix_length);
        int traverse(VocabId word, int from, bool &independent_right);
        int endmarker_node(int from, float &bow);
        bool traverse_query(VocabId word, Traverse &t);
        bool traverse_to_target(VocabId word, const Traverse &t, Target &info);
        int target_node(int from, VocabId word, float &prob, bool &independent_left);
        void resize_array(long size, float *&values, uint64_t *&validate);
        bool get_unigram_prob(VocabId word, Target &target);

        void replace_value(float *values);

        long array_size_;
        DAPair *da_array_;
        unsigned int daid_;
        unsigned int datotal_;
        float bow_max_;
        VocabId unk_;
        BstDA **da_;

        Logger &logger_;

        // Member from MatsuTaku
        size_t loop_counts_=0;
        size_t skip_counts_=0;
#ifdef DALM_NEW_XCHECK
        // Additional bit array
        BitVector<1> empty_element_bits;
#endif
        std::map<size_t, size_t> children_cnt_table_;
        std::map<size_t, double> children_fb_time_table_;

        void fit_to_size(int max_slot);
    };

    class BstDAHandler : public DAHandler{
    public:
        BstDAHandler(){}
        BstDAHandler(BinaryFileReader &reader, unsigned char order, Logger &logger){
            reader >> danum;
            da = new DA*[danum];
            for(size_t i = 0; i < danum; i++){
                da[i] = new BstDA(reader, (BstDA **) da, order, logger);
            }
        }
        virtual ~BstDAHandler(){}

        virtual void build(unsigned int danum, std::string pathtoarpa, std::string &pathtotreefile,
                           Vocabulary &vocab, std::string &tmpdir, unsigned int n_cores, Logger &logger){
            DAHandler::danum=danum;

            da = new DA*[danum];
            DABuilder **builder = new DABuilder*[danum];

            for(unsigned int i = 0; i < danum; i++){
                da[i] = new BstDA(i, danum, (BstDA **)da, logger);
                builder[i] = new DABuilder(da[i], pathtotreefile, NULL, vocab, tmpdir);
            }

            logger << "[BstDAHandler::build] Building Double Array." << Logger::endi;
            run(builder, n_cores, logger);

            for(size_t i = 0; i < danum; i++){
                delete builder[i];
            }
            delete [] builder;

            NodeFiller **fixer = new NodeFiller *[danum];
            for(size_t i = 0; i < danum; i++){
                fixer[i] = new NodeFiller(da[i], pathtotreefile, vocab);
            }
            size_t threads = PThreadWrapper::thread_available(n_cores);
            logger << "[BstDAHandler::build] threads available=" << threads << Logger::endi;
            size_t running = 0;
            for(size_t i = 0; i < danum; i++){
                fixer[i]->start();
                running++;
                if(running>=threads){
                    while(true){
                        usleep(500);
                        size_t runcounter = 0;
                        for(size_t j = 0; j <= i; j++){
                            if(!fixer[j]->is_finished()) runcounter++;
                        }
                        if(runcounter<threads){
                            running = runcounter;
                            break;
                        }
                    }
                }
            }
            for(size_t i = 0; i < danum; i++){
                fixer[i]->join();
                delete fixer[i];
            }
            delete [] fixer;
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
            if(state.has_context()) return state.get_word(0)%danum;
            else return 0;
        }

        virtual size_t get_state_daid(VocabId *ngram, size_t n){
            return ngram[0]%danum;
        }

        virtual size_t get_errorcheck_daid(VocabId *ngram, size_t n){
            return (n==1)?(ngram[n-1]%danum):(ngram[n-2]%danum);
        }

        virtual void dump_method_dependents(BinaryFileWriter &fp){
        }

    private:
    };
}

#endif //QUAIL_DECODER_BST_DA_H

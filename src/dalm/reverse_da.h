#ifndef QUAIL_DECODER_REVERSE_DA_H
#define QUAIL_DECODER_REVERSE_DA_H

#include <cmath>
#include <limits>
#include "bst_da.h"
#include "da.h"

namespace DALM {
    class ReverseDA : public DA{
    public:
        ReverseDA(unsigned char daid, unsigned char datotal, ReverseDA **neighbours, Logger &logger);
        ReverseDA(BinaryFileReader &reader, ReverseDA **neighbours, unsigned char order, Logger &logger);
        virtual ~ReverseDA();

        virtual void make_da(std::string &pathtotreefile, ValueArrayIndex *value_array_index, Vocabulary &vocab);
        virtual void dump(BinaryFileWriter &writer);
        virtual void export_to_text(TextFileWriter &writer);

        virtual void write_and_free(BinaryFileWriter &writer);
        virtual void reload(BinaryFileReader &reader);

        virtual float get_prob(VocabId *word, unsigned char order);
        virtual float get_prob(VocabId word, State &state);
        virtual float get_prob(VocabId word, State &state, Fragment &f);
        virtual float get_prob(const Fragment &f, State &state, Gap &gap);
        virtual float get_prob(const Fragment &fprev, State &state, Gap &gap, Fragment &fnew);
        virtual float update(VocabId word, Fragment &f, bool &extended, bool &independent_left, bool &independent_right, float &bow);

        virtual void init_state(VocabId *word, unsigned char order, State &state);
        virtual void init_state(State &state, const State &state_prev, const Fragment *fragments, const Gap &gap);

        /* depricated */
        virtual unsigned long int get_state(VocabId *word, unsigned char order);

        virtual bool checkinput(unsigned short n,unsigned int *ngram,float bow,float prob,bool bow_presence);
        virtual void fill_inserted_node(std::string &pathtotreefile, Vocabulary &vocab);

    private:
        struct Traverse {
            int index;
            Val32 base_or_prob;

            Traverse(){}
            Traverse(ReverseDA *da, int index){
                init(da, index);
            }

            void init(ReverseDA *da, int index=0){
                this->index = index;
                this->base_or_prob.i = da->da_array_[index].base.base_val;
            }

            bool has_target() const {
                return base_or_prob.i >= 0;
            }

            int base() const{
                return base_or_prob.i;
            }

            float prob() const{
                return base_or_prob.f;
            }
        };

        struct Target {
            float prob;
            int index;
            Val32 bow_and_left_and_right;

            Target(){
                prob = DALM_OOV_PROB;
                bow_and_left_and_right.f = 0.0f;
                index = 0;
            }

            void set(ReverseDA *da, Traverse &t){
                if(t.has_target()){
                    index = t.base() + 1;
                    prob = da->da_array_[index].check.logprob;
                    bow_and_left_and_right.f = da->da_array_[index].base.logbow;
                } else {
                    prob = t.prob();
                    bow_and_left_and_right.f = std::numeric_limits<float>::infinity();
                }
            }

            bool independent_left() const{
                return bow_and_left_and_right.f > 0;
            }

            bool independent_right() const{
                return std::isinf(bow_and_left_and_right.f);
            }

            float bow(float bow_max) const {
                Val32 b;
                b.u = (bow_and_left_and_right.u | 0x80000000U);
                return b.f + bow_max;
            }
        };

        void det_base(VocabId *children, float node_prob, Base node_bow, std::size_t n_children, int context,
                      float *&values, std::size_t &n_entries, int &head, int &max_slot, uint64_t *&validate,
                      uint64_t words_prefix, std::size_t prefix_length);
        int traverse(VocabId word, int context);
        int endmarker_node(int context);
        void resize_array(int n_array, float *&values, uint64_t *&validate);

        bool jump(Traverse &t, VocabId word);
        void replace_value(float *&values);
        void fit_to_size(int max_slot);

        long array_size_;
        DAPair *da_array_;
        unsigned int daid_;
        unsigned int datotal_;
        float bow_max_;
        ReverseDA **da_;
        Logger &logger_;

        // Member from MatsuTaku
        char log_buffer_[128];
        size_t loop_counts_=0;
        size_t skip_counts_=0;
        std::map<size_t, size_t> children_cnt_table_;
        std::map<size_t, double> children_fb_time_table_;
    };

    class ReverseDAHandler : public DAHandler{
    public:
        ReverseDAHandler(){}
        ReverseDAHandler(BinaryFileReader &reader, unsigned char order, Logger &logger){
            reader >> danum;
            da = new DA*[danum];
            for(size_t i = 0; i < danum; i++){
                da[i] = new ReverseDA(reader, (ReverseDA **) da, order, logger);
            }
        }
        virtual ~ReverseDAHandler(){}

        virtual void build(unsigned int danum, std::string pathtoarpa, std::string &pathtotreefile, Vocabulary &vocab,
                           std::string &tmpdir, unsigned int n_cores, Logger &logger){
            DAHandler::danum=danum;
            da = new DA*[danum];
            DABuilder **builder = new DABuilder*[danum];

            for(size_t i = 0; i < danum; i++){
                da[i] = new ReverseDA(i, danum, (ReverseDA **)da, logger);
                builder[i] = new DABuilder(da[i], pathtotreefile, NULL, vocab, tmpdir);
            }

            logger << "[ReverseDAHandler::build] Building Double Array." << Logger::endi;
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
            logger << "[ReverseDAHandler::build] threads available=" << threads << Logger::endi;
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
            // just sum all, so its no mutter the DA-ID.
            return da[0]->sum_bows(state, begin, end);
        }

    protected:
        virtual size_t get_daid(VocabId *ngram, size_t n){
            return ngram[0]%danum;
        }

        virtual size_t get_daid(VocabId word, State &state){
            return word%danum;
        }

        virtual size_t get_daid(const Fragment &f, State &state_prev, Gap &g){
            return f.status.word%danum;
        }

        virtual size_t get_daid(const Fragment &f){
            return f.status.word%danum;
        }

        virtual size_t get_daid(State &state, const State &state_prev){
            if(state_prev.has_context()) return state_prev.get_word(0)%danum;
            else return 0;
        }

        virtual size_t get_state_daid(VocabId *ngram, size_t n){
            return ngram[0]%danum;
        }

        virtual size_t get_errorcheck_daid(VocabId *ngram, size_t n){
            return ngram[n-1]%danum;
        }

        virtual void dump_method_dependents(BinaryFileWriter &fp){}
    };


}
#endif //QUAIL_DECODER_REVERSE_DA_H

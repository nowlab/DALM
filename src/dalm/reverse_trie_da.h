#ifndef QUAIL_DECODER_REVERSE_TRIE_DA_H
#define QUAIL_DECODER_REVERSE_TRIE_DA_H

#include "da.h"

namespace DALM {
    class ReverseTrieDA : public DA{
    public:
        ReverseTrieDA(unsigned int daid, unsigned int datotal, ReverseTrieDA **neighbours, Logger &logger);
        ReverseTrieDA(BinaryFileReader &reader, ReverseTrieDA **neighbours, unsigned char order, Logger &logger);
        virtual ~ReverseTrieDA();

        virtual void make_da(std::string &pathtotreefile, ValueArrayIndex *value_array_index, Vocabulary &vocab);
        virtual void dump(BinaryFileWriter &writer);

        virtual void write_and_free(BinaryFileWriter &writer);
        virtual void reload(BinaryFileReader &reader);

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
        virtual void fill_inserted_node(std::string &pathtotreefile, Vocabulary &vocab);

    private:
        void det_base(VocabId *words, DABody *children, std::size_t n_children, int context, int &head, int &max_slot);
        void det_base_leaves(VocabId *words, DABody *children, std::size_t n_children, int context, int &leaf_head, int &leaf_max_slot);
        int traverse(VocabId word, int from);
        void resize_array(long n_array);
        void resize_leaves(long n_leaves);
        void fit_to_size(int max_slot, int leaf_max_slot);

        bool jump(int context, VocabId word, int &next, float &prob, float &bow, bool &finalized, bool &independent_right);

        long array_size_;
        long n_leaves_;
        DABody *da_array_;
        DATail *leaves_;
        unsigned int daid_;
        unsigned int datotal_;
        ReverseTrieDA **da_;

        Logger &logger_;
    };

    class ReverseTrieDAHandler : public DAHandler{
    public:
        ReverseTrieDAHandler(){}
        ReverseTrieDAHandler(BinaryFileReader &reader, unsigned char order, Logger &logger){
            reader >> danum;
            da = new DA*[danum];
            for(unsigned int i = 0; i < danum; i++){
                da[i] = new ReverseTrieDA(reader, (ReverseTrieDA **)da, order, logger);
            }
        }
        virtual ~ReverseTrieDAHandler(){}

        virtual void build(unsigned int danum, std::string pathtoarpa, std::string &pathtotreefile,
                           Vocabulary &vocab, std::string &tmpdir, unsigned int n_cores, Logger &logger){
            DAHandler::danum=danum;
            da = new DA*[danum];
            DABuilder **builder = new DABuilder*[danum];

            for(unsigned int i = 0; i < danum; i++){
                da[i] = new ReverseTrieDA(i, danum, (ReverseTrieDA **)da, logger);
                builder[i] = new DABuilder(da[i], pathtotreefile, NULL, vocab, tmpdir);
            }

            logger << "[ReverseTrieDAHandler::build] Building Double Array." << Logger::endi;
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
            logger << "[ReverseTrieDAHandler::build] threads available=" << threads << Logger::endi;
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

#endif //QUAIL_DECODER_REVERSE_TRIE_DA_H

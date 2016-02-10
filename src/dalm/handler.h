#ifndef HANDLER_H_
#define HANDLER_H_

#include <unistd.h>
#include "da.h"
#include "vocabulary.h"
#include "arpafile.h"
#include "logger.h"
#include "state.h"
#include "fileutil.h"

namespace DALM{
    class DAHandler{
        public:
            DAHandler(){
                da = NULL;
                danum = 0;
            }

            virtual ~DAHandler(){
                if(da != NULL){
                    for(size_t i = 0; i < danum; i++){
                        delete da[i];
                    }
                    delete [] da;
                }
            }

            inline float get_prob(VocabId *ngram, unsigned char n){
                return da[get_daid(ngram, n)]->get_prob(ngram, n);
            }

            inline float get_prob(VocabId word, State &state){
                return da[get_daid(word, state)]->get_prob(word, state);
            }

            inline float get_prob(VocabId word, State &state, Fragment &f){
                return da[get_daid(word, state)]->get_prob(word, state, f);
            }

            inline float get_prob(const Fragment &f, State &state_prev, Gap &gap){
                return da[get_daid(f, state_prev, gap)]->get_prob(f, state_prev, gap);
            }

            inline float get_prob(const Fragment &fprev, State &state_prev, Gap &gap, Fragment &fnew){
                return da[get_daid(fprev, state_prev, gap)]->get_prob(fprev, state_prev, gap, fnew);
            }

            inline float update(VocabId word, Fragment &f, bool &extended, bool &independent_left, bool &independent_right, float &bow) {
                return da[get_daid(f)]->update(word, f, extended, independent_left, independent_right, bow);
            }

            inline void init_state(VocabId *ngram, unsigned char n, State &state){
                da[get_state_daid(ngram, n)]->init_state(ngram, n, state);
            }

            inline void init_state(State &state, const State &state_prev, const Fragment *fragments, const Gap &gap){
                da[get_daid(state, state_prev)]->init_state(state, state_prev, fragments, gap);
            }

            /* depricated */
            inline unsigned long int get_state(VocabId *ngram, unsigned char n){
                return da[get_state_daid(ngram, n)]->get_state(ngram,n);
            }

            virtual void build(unsigned int danum, std::string pathtoarpa, std::string &pathtotreefile, Vocabulary &vocab, std::string &tmpdir, unsigned int n_cores, Logger &logger) = 0;

            bool errorcheck(unsigned short n,unsigned int *ngram,float bow,float prob,bool bow_presence){
                size_t daid = get_errorcheck_daid(ngram, n);
                return da[daid]->checkinput(n,ngram,bow,prob,bow_presence);
            }

            void dump(BinaryFileWriter &writer){
                writer << danum;
                dump_method_dependents(writer);
                for(size_t i = 0; i < danum; i++){
                    da[i]->dump(writer);
                }
            }

            void export_to_text(TextFileWriter &writer){
                writer << danum;
                export_method_dependents(writer);
                for(size_t i = 0; i < danum; i++){
                    da[i]->export_to_text(writer);
                }
            }


        virtual float sum_bows(State &state, unsigned char begin, unsigned char end) = 0;

        protected:
            virtual size_t get_daid(VocabId *ngram, size_t n) = 0;
            virtual size_t get_daid(VocabId word, State &state) = 0;
            virtual size_t get_daid(const Fragment &f, State &state_prev, Gap &g)=0;
            virtual size_t get_daid(const Fragment &f)=0;
            virtual size_t get_daid(State &state, const State &state_prev)=0;
            virtual size_t get_state_daid(VocabId *ngram, size_t n) = 0;
            virtual size_t get_errorcheck_daid(VocabId *ngram, size_t n) = 0;
            virtual void dump_method_dependents(BinaryFileWriter &fp) = 0;
            virtual void export_method_dependents(TextFileWriter &fp){}

            void run(DABuilder **builder, int n_cores, Logger &logger){
                size_t threads = PThreadWrapper::thread_available(n_cores);
                logger << "[Handler::run] threads available=" << threads << Logger::endi;
                size_t running = 0;
                for(size_t i = 0; i < danum; i++){
                    builder[i]->start();
                    running++;
                    if(running>=threads){
                        while(true){
                            usleep(500);
                            size_t runcounter = 0;
                            for(size_t j = 0; j <= i; j++){
                                if(!builder[j]->is_finished()) runcounter++;
                            }
                            if(runcounter<threads){
                                running = runcounter;
                                break;
                            }
                        }
                    }
                }
                for(size_t i = 0; i < danum; i++){
                    builder[i]->join();
                }
            }

            DA **da;
            unsigned int danum;
    };

}

#endif // HANDLER_H_


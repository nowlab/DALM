#ifndef HANDLER_H_
#define HANDLER_H_

#include <unistd.h>
#include "da.h"
#include "vocabulary.h"
#include "arpafile.h"
#include "logger.h"
#include "state.h"

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
			
			inline float get_prob(VocabId *ngram, size_t n){
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

			inline void init_state(VocabId *ngram, size_t n, State &state){
				da[get_state_daid(ngram, n)]->init_state(ngram, n, state);
			}

			inline void init_state(State &state, const State &state_prev, const Fragment *fragments, const Gap &gap){
				da[get_daid(state, state_prev)]->init_state(state, state_prev, fragments, gap);
			}
			
			/* depricated */
			inline unsigned long int get_state(VocabId *ngram, size_t n){
				return da[get_state_daid(ngram, n)]->get_state(ngram,n);
			}

			virtual void build(size_t danum, std::string pathtoarpa, std::string &pathtotreefile, Vocabulary &vocab, Logger &logger) = 0;
			
			bool errorcheck(unsigned short n,unsigned int *ngram,float bow,float prob,bool bow_presence){
				size_t daid = get_errorcheck_daid(ngram, n);
				return da[daid]->checkinput(n,ngram,bow,prob,bow_presence);
			}
			
			void dump(std::FILE *fp){
				fwrite(&danum, sizeof(unsigned char), 1, fp);
				dump_method_dependents(fp);
				for(size_t i = 0; i < danum; i++){
					da[i]->dump(fp);
				}
			}
			
		protected:
			virtual size_t get_daid(VocabId *ngram, size_t n) = 0;
			virtual size_t get_daid(VocabId word, State &state) = 0;
			virtual size_t get_daid(const Fragment &f, State &state_prev, Gap &g)=0;
			virtual size_t get_daid(State &state, const State &state_prev)=0;
			virtual size_t get_state_daid(VocabId *ngram, size_t n) = 0;
			virtual size_t get_errorcheck_daid(VocabId *ngram, size_t n) = 0;
			virtual void dump_method_dependents(std::FILE *fp) = 0;

			void run(DABuilder **builder, Logger &logger){
				size_t threads = PThreadWrapper::thread_available();
				logger << "[LM::build] threads available=" << threads << Logger::endi;
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
			size_t danum;
	};

	class EmbeddedDAHandler : public DAHandler{
		public:
			EmbeddedDAHandler(): value_array(NULL){}
			EmbeddedDAHandler(FILE *fp, unsigned char order, Logger &logger){
				fread(&danum, sizeof(unsigned char), 1, fp);
				da = new DA*[danum];
				value_array = new ValueArray(fp, logger);
				for(size_t i = 0; i < danum; i++){
					da[i] = new EmbeddedDA(fp, *value_array, (EmbeddedDA **) da, order, logger);
				}
			}
			virtual ~EmbeddedDAHandler(){
				if(value_array!=NULL) delete value_array;
			}
			
			virtual void build(size_t danum, std::string pathtoarpa, std::string &pathtotreefile, Vocabulary &vocab, Logger &logger){
				DAHandler::danum=danum;

				value_array = new ValueArray(pathtoarpa, vocab, logger);
				ValueArrayIndex value_array_index(*value_array);

  			da = new DA*[danum];
			  DABuilder **builder = new DABuilder*[danum];

				for(size_t i = 0; i < danum; i++){
					da[i] = new EmbeddedDA(i, danum, *value_array, (EmbeddedDA **)da, logger);
					builder[i] = new DABuilder(da[i], pathtotreefile, &value_array_index, vocab);
				}

				logger << "[EmbeddedDAHandler::build] Building Double Array." << Logger::endi;
				run(builder, logger);

				for(size_t i = 0; i < danum; i++){
					delete builder[i];
				}
				delete [] builder;
			}

		protected:
			virtual size_t get_daid(VocabId *ngram, size_t n){
				return (n==1)?(ngram[0]%danum):(ngram[1]%danum);
			}

			virtual size_t get_daid(VocabId word, State &state){
				return (state.has_context())?(state.get_word(0)%danum):(word%danum);
			}
			
			virtual size_t get_daid(const Fragment &f, State &state_prev, Gap &g){
				if(g.get_count()==0){
					throw "BUG!";
				}else{
					return state_prev.get_word(0)%danum;
				}
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

			virtual void dump_method_dependents(std::FILE *fp){
				value_array->dump(fp);
			}

		private:
			ValueArray *value_array;
	};
}

#endif // HANDLER_H_


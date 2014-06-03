#ifndef DA_H_
#define DA_H_

#include <stdint.h>
#include <cstdio>

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

#define DALM_OOV_PROB -100.0

namespace DALM{
	typedef union {
		int base_val;
		float logprob;
		float logbow;
		uint32_t bits;
	} _base;

	typedef union {
		int check_val;
		float logprob;
	} _check;

	typedef union {
		float bow;
		uint32_t bits;
	} _bowval;

	typedef struct {
		_base base;
		_check check;
	} DAPair;

	class DA{
		public:
			DA(){}
			virtual ~DA(){}

			virtual void make_da(std::string &pathtotreefile, ValueArrayIndex *value_array_index, Vocabulary &vocab) = 0;
			virtual void dump(FILE *fp) = 0;

			virtual float get_prob(VocabId *word, unsigned char order) = 0;
			virtual float get_prob(VocabId word, State &state) = 0;
			virtual float get_prob(VocabId word, State &state, Fragment &f) = 0;
			virtual float get_prob(const Fragment &f, State &state, Gap &gap) = 0;
			virtual float get_prob(const Fragment &fprev, State &state, Gap &gap, Fragment &fnew) = 0;

			virtual void init_state(VocabId *word, unsigned char order, State &state) = 0;
			virtual void init_state(State &state, const State &state_prev, const Fragment *fragments, const Gap &gap) = 0;

			/* depricated */
			virtual unsigned long int get_state(VocabId *word, unsigned char order) = 0;
			virtual bool checkinput(unsigned short n,unsigned int *ngram,float bow,float prob,bool bow_presence) = 0;

		protected:
			unsigned char context_size;
	};

	class EmbeddedDA : public DA{
		public:
			EmbeddedDA(unsigned char daid, unsigned char datotal, ValueArray &value_array, EmbeddedDA **neighbours, Logger &logger);
			EmbeddedDA(FILE *fp, ValueArray &value_array, EmbeddedDA **neighbours, unsigned char order, Logger &logger);
			virtual ~EmbeddedDA();

			virtual void make_da(std::string &pathtotreefile, ValueArrayIndex *value_array_index, Vocabulary &vocab);
			virtual void dump(FILE *fp);

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
	};

	class DABuilder : public PThreadWrapper {
		public:
			DABuilder(DA *da, std::string &pathtotreefile, ValueArrayIndex *value_array_index, Vocabulary &vocab)
				:da(da), pathtotreefile(pathtotreefile), value_array_index(value_array_index), vocab(vocab){
					finished=false;
				}
			
			virtual void run(){
				da->make_da(pathtotreefile, value_array_index, vocab);
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
	};
}

#endif // DA_H_


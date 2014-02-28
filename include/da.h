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
	} _base;

	typedef union {
    float bow;
    uint32_t bits;
  } _bowval;

	typedef struct {
		_base base;
		int check;
	} DAPair;

	class DA{
		public:
			DA(unsigned char daid, unsigned char datotal, ValueArray &value_array, DA **neighbours, Logger &logger);
			DA(FILE *fp, ValueArray &value_array, DA **neighbours, Logger &logger);
			virtual ~DA();

			void make_da(TreeFile &tf, ValueArrayIndex &value_array_index, unsigned unigram_type);
			void dump(FILE *fp);

			float get_prob(VocabId *word, unsigned char order);
			float get_prob(VocabId word, State &state);
			float get_prob(VocabId word, State &state, Fragment &f);
			float get_prob(const Fragment &f, State &state, Gap &gap);
			float get_prob(const Fragment &fprev, State &state, Gap &gap, Fragment &fnew);

			void init_state(VocabId *word, unsigned char order, State &state);
			void init_state(State &state, State &state_prev, Gap &gap);

			/* depricated */
			unsigned long int get_state(VocabId *word, unsigned char order);

			bool checkinput(unsigned short n, unsigned int *ngram,float bow,float prob,bool bow_presence);

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
			DA **da;

			unsigned int max_index;
			unsigned int first_empty_index;

			Logger &logger;
	};

	class DABuilder : public PThreadWrapper {
		public:
			DABuilder(DA *da, TreeFile *tf, ValueArrayIndex &value_array_index, size_t vocabsize)
				:da(da), tf(tf), value_array_index(value_array_index), vocabsize(vocabsize){
					finished=false;
				}

			virtual void run(){
				da->make_da(*tf, value_array_index, vocabsize);
				finished=true;
			}

			bool is_finished(){
				return finished;
			}

		private:
			DA *da;
			TreeFile *tf;
			ValueArrayIndex &value_array_index;
			size_t vocabsize;
			bool finished;
	};
}

#endif // DA_H_


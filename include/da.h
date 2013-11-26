#ifndef DA_H_
#define DA_H_

#include <cstdio>

#include <iostream>
#include <utility>
#include <string>
#include <set>

#include <pthread_wrapper.h>
#include <treefile.h>
#include <logger.h>

#define DALM_OOV_PROB -100.0

namespace DALM{
	typedef union{
		int base_val;
		float logprob;
	} _base;

	class DA{
		public:
			DA(size_t daid, size_t datotal, DA **neighbours, Logger &logger);
			DA(FILE *fp, DA **neighbours, Logger &logger);
			virtual ~DA();

			void make_da(TreeFile &tf, unsigned unigram_type);
			void dump(FILE *fp);

			float get_prob(int *word,int order);
			unsigned long int get_state(int *word,int order);

			bool checkinput(unsigned short n,unsigned int *ngram,float bow,float prob,bool bow_presence);

			void value_set(std::set<float> *value_set);
			void value_replace();

		private:
			void det_base(int *word,float *val,unsigned amount,unsigned now);
			int get_pos(int word,unsigned now);
			int get_terminal(unsigned now);
			unsigned value_insert(float tmp_val);
			unsigned value_find(float tmp_val);
			void resize_array(unsigned newarray_size);

			bool prob_check(int length, int order, unsigned int *ngram, float prob);
			bool bow_check(int length, unsigned int * ngram, float bow);
			unsigned value_hash(float tmp_val);

			unsigned array_size;
			_base *base_array;
			int *check_array, *value_id;
			float *value_array;
			unsigned varray_size, vtable_size;
			unsigned *value_table;
			unsigned daid;
			unsigned datotal;
			DA **da;

			unsigned max_index;
			unsigned first_empty_index;

			Logger &logger;
	};

	class DABuilder : public PThreadWrapper {
		public:
			DABuilder(DA *da, TreeFile *tf, std::set<float> *value_set, size_t vocabsize)
				:da(da), tf(tf), value_set(value_set), vocabsize(vocabsize){
					running=false;
				}

			virtual void run(){
				running=true;
				da->value_set(value_set);
				da->make_da(*tf,vocabsize);
				running=false;
			}

			bool is_running(){
				return running;
			}

		private:
			DA *da;
			TreeFile *tf;
			std::set<float> *value_set;
			size_t vocabsize;
			bool running;
	};
}

#endif // DA_H_


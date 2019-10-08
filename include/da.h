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
#include "fileutil.h"

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

			virtual float sum_bows(State &state, unsigned char begin, unsigned char end) = 0;

		protected:
			unsigned char context_size;
	};

	class EmbeddedDA : public DA{
		public:
			EmbeddedDA(unsigned char daid, unsigned char datotal, ValueArray &value_array, EmbeddedDA **neighbours, Logger &logger);
			EmbeddedDA(BinaryFileReader &reader, ValueArray &value_array, EmbeddedDA **neighbours, unsigned char order, Logger &logger);
			~EmbeddedDA() override;

			void make_da(std::string &pathtotreefile, ValueArrayIndex *value_array_index, Vocabulary &vocab) override;
			void dump(FILE *fp) override;

			float get_prob(VocabId *word, unsigned char order) override;
			float get_prob(VocabId word, State &state) override;
			float get_prob(VocabId word, State &state, Fragment &f) override;
			float get_prob(const Fragment &f, State &state, Gap &gap) override;
			float get_prob(const Fragment &fprev, State &state, Gap &gap, Fragment &fnew) override;

			void init_state(VocabId *word, unsigned char order, State &state) override;
			void init_state(State &state, const State &state_prev, const Fragment *fragments, const Gap &gap) override;

			/* depricated */
			unsigned long int get_state(VocabId *word, unsigned char order) override;

			bool checkinput(unsigned short n,unsigned int *ngram,float bow,float prob,bool bow_presence) override;

			float sum_bows(State &state, unsigned char begin, unsigned char end) override;

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

	        size_t loop_counts_=0;
			size_t skip_counts_=0;
#ifdef DALM_NEW_XCHECK
            // Additional bit array
            template <bool DefaultBit>
			class _BitVector {
				class _Ref {
				private:
					uint64_t *pointer_;
					size_t ctz_;
				public:
					explicit _Ref(uint64_t* pointer, size_t ctz) : pointer_(pointer), ctz_(ctz) {}
					explicit operator bool() const {
						return *pointer_ & (1ull << ctz_);
					}
					_Ref& operator=(bool bit) {
						if (bit)
							*pointer_ |= (1ull << ctz_);
						else
							*pointer_ &= compl (1ull << ctz_);
						return *this;
					}
				};

			public:
				static constexpr uint64_t kDefaultWord = DefaultBit ? -1ull : 0;

				_BitVector() : size_(0) {}
				_Ref operator[](size_t index) {
					return _Ref(&container_[index / 64], index % 64);
				}
				uint64_t* ptr_at(size_t word_index) {
					return &container_[word_index];
				}
				uint64_t word_at(size_t word_index) const {
					return word_index < container_.size() ? container_[word_index] : kDefaultWord;
				}
				uint64_t bits64_from(size_t index) const {
					auto insets = index%64;
					if (insets == 0) {
						return word_at(index/64);
					} else {
						return (word_at(index/64) >> insets) | (word_at(index/64+1) << (64-insets));
					}
				};
				void resize(size_t new_size, bool bit) {
					container_.resize((new_size-1)/64+1, bit ? -1ull : 0);
					if (new_size > size_ and bit) {
						*ptr_at(size_/64) |= -1ull << size_%64;
						if (new_size % 64 > 0) {
							container_[new_size/64] &= -1ull >> (64 - new_size%64);
						}
					}
					size_ = new_size;
				}

			private:
				size_t size_;
				std::vector<uint64_t> container_;
			};
			_BitVector<1> empty_element_bits;
#endif
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


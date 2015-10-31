#ifndef DALM_STATE_H_
#define DALM_STATE_H_

#include<cstring>
#include<numeric>
#include "vocabulary.h"

typedef unsigned long int StateId;
#ifndef DALM_MAX_ORDER
#define DALM_MAX_ORDER 6
#endif

namespace DALM{
	class State{
		public:
			class HashFunction {
			public:
				virtual std::size_t operator()(const VocabId *words, std::size_t size) const = 0;
			};

			State(): count(0){
			}

			virtual ~State(){
			}

			inline void copy_words_bows(const State &s){
				memcpy(vids, s.vids, sizeof(VocabId)*s.count);
				memcpy(bows, s.bows, sizeof(float)*s.count);
			}

			inline float get_bow(unsigned char i) const{
				return bows[i];
			}

			inline void set_bow(size_t i, float bow){
				bows[i] = bow;
			}

			inline void set_count(unsigned char c){
				count = c;
			}

			inline unsigned char get_count() const{
				return count;
			}

			inline void refresh(){
				count=0;
			}

			inline void push_word(VocabId word){
				if(count>0){
					unsigned char end = count-1;
					for(unsigned char i=end; i>=1; i--){
						vids[i] = vids[i-1];
					}
				}
				vids[0] = word;
			}
			
			inline void shift_words(unsigned char begin, unsigned char end, unsigned char shift_width){
				std::copy_backward(vids+begin, vids+end, vids+end+shift_width);
			}

			inline void set_word(unsigned char i, VocabId word){
				vids[i] = word;
			}

			inline VocabId get_word(unsigned char i) const{
				return vids[i];
			}

			inline int compare(const State &state) const{
				int cmp = std::memcmp(
						vids, 
						state.vids, 
						std::min(count,state.count)*sizeof(VocabId));
				if(cmp!=0) return cmp;
				else return count-state.count;
			}

			inline bool has_context() const{
				return count!=0;
			}

			inline float sum_bows(unsigned char begin, unsigned char end){
				float sum = 0.0;
				for(unsigned char i = begin; i < end; i++){
					sum += bows[i];
				}
				return sum;
			}

			inline std::size_t hash(const HashFunction &func) const{
				return func(vids, count);
			}

		private:
			unsigned char count;
			VocabId vids[DALM_MAX_ORDER-1];
			float bows[DALM_MAX_ORDER-1];
	};
}

#endif //DALM_STATE_H_


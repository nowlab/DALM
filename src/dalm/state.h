#ifndef DALM_STATE_H_
#define DALM_STATE_H_

#include<cstring>
#include<numeric>
#include <stdint.h>
#include <stdexcept>
#include "vocabulary.h"

typedef unsigned long int StateId;
#ifndef DALM_MAX_ORDER
#define DALM_MAX_ORDER 6
#endif

namespace DALM{
	typedef union {
		float bow;
		uint32_t bits;
        int pos;
	} BowVal;

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
                memcpy(bows, s.bows, sizeof(BowVal)*s.count);
            }

            inline float get_bow(int i) const{
                return bows[i].bow;
            }

            inline void set_bow(int i, float bow){
                bows[i].bow = bow;
            }

            inline int get_pos(int i) const {
                return bows[i].pos;
            }

            inline void set_pos(int i, int pos){
                bows[i].pos = pos;
            }

            inline void set_count(unsigned char c){
                count = c;
            }

            inline unsigned char get_count() const{
                return count;
            }

            inline void increment(){
                ++count;
            }

            inline void refresh(){
                count=0;
            }

            inline void push_word(VocabId word){
                if(count>0){
                    int end = std::min((int)count, DALM_MAX_ORDER-2);
                    for(int i=end; i>=1; i--){
                        vids[i] = vids[i-1];
                    }
                }
                vids[0] = word;
            }
            
            inline void shift_words(int begin, int end, int shift_width){
                std::copy_backward(vids+begin, vids+end, vids+end+shift_width);
            }

            inline void set_word(int i, VocabId word){
                vids[i] = word;
            }

            inline VocabId get_word(int i) const{
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

            /* deleted */
            inline float sum_bows(int begin, int end){
                throw std::runtime_error("API Changed. Please rewrite your program to use DALM::LM::sum_bows.");
            }

            inline std::size_t hash(const HashFunction &func) const{
                return func(vids, count);
            }

        private:
            unsigned char count;
            VocabId vids[DALM_MAX_ORDER-1];
            BowVal bows[DALM_MAX_ORDER-1];
    };
}

#endif //DALM_STATE_H_


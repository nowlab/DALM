#ifndef DALM_STATE_H_
#define DALM_STATE_H_

#include<cstring>
#include "vocabulary.h"

typedef unsigned long int StateId;

namespace DALM{
	class State{
		public:
			State(unsigned char modelorder): order(modelorder-1), count(0), head(order-1), daid(0), head_pos(0){
				sids = new StateId[order];
				memset(sids, 0, sizeof(StateId)*order);
				vids = new VocabId[order];
				memset(vids, 0, sizeof(VocabId)*order);
				bows = new float[order];
				memset(bows, 0, sizeof(float)*order);
			}

			State(const State &s):order(s.order), count(s.count), head(s.head), daid(s.daid), head_pos(s.head_pos){
				sids = new StateId[order];
				memcpy(sids, s.sids, sizeof(StateId)*order);
				vids = new VocabId[order];
				memcpy(vids, s.vids, sizeof(VocabId)*order);
				bows = new float[order];
				memcpy(bows, s.bows, sizeof(float)*order);
			}
			
      State &operator=(const State &s){
        if(order != s.order){
          order = s.order;
          delete [] sids;
          delete [] vids;
          delete [] bows;

          sids = new StateId[order];
          vids = new VocabId[order];
          bows = new float[order];
        }
        count = s.count;
        head = s.head;
        daid = s.daid;
        head_pos = s.head_pos;
        memcpy(sids, s.sids, sizeof(StateId)*order);
        memcpy(vids, s.vids, sizeof(VocabId)*order);
        memcpy(bows, s.bows, sizeof(float)*order);
        return *this;
      }

			virtual ~State(){
				delete [] sids;
				delete [] vids;
				delete [] bows;
			}
			
			inline StateId &operator[](unsigned char i){
				return sids[i];
			}

			inline StateId get_head_pos(){
				return head_pos;
			}

			inline void set_head_pos(StateId sid){
				head_pos = sid;
			}

			inline StateId get_sid(unsigned char i) const{
				return sids[i];
			}

			inline float get_bow(unsigned char i) const{
				return bows[i];
			}

			inline void set_bow(size_t i, float bow) const{
				bows[i] = bow;
			}

			inline void set_count(unsigned char c){
				count = c;
			}

			inline unsigned char get_count(){
				return count;
			}

			inline void set_daid(unsigned char id){
				daid = id;
			}

			inline unsigned char get_daid(){
				return daid;
			}

			inline void refresh(){
				count=0;
				push_word(DALM_UNK_WORD);
			}

			inline void push_word(VocabId word){
				head = (head+order-1)%order;
				vids[head] = word;
			}
			
			inline void set_word(unsigned char i, VocabId word){
				vids[(head+i)%order] = word;
			}

			inline VocabId get_word(unsigned char i){
				return vids[(head+i)%order];
			}

			inline unsigned char get_order(){
				return order;
			}

			inline int compare(State *state){
				for(size_t i = 0; i < count && i < state->count; i++){
					int diff = get_word(i) - state->get_word(i);
					if(diff!=0) return diff;
				}
				return count-state->count;
			}

		private:
			unsigned char order;
			unsigned char count;
			unsigned char head;
			StateId *sids;
			VocabId *vids;
			float *bows;
			unsigned char daid;
			StateId head_pos;
	};
}

#endif //DALM_STATE_H_


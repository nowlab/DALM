#ifndef DALM_STATE_H_
#define DALM_STATE_H_

#include<cstring>
#include "vocabulary.h"

typedef unsigned long int StateId;

namespace DALM{
	class State{
		public:
			State(unsigned short modelorder): order(modelorder-1){
				sids = new StateId[order];
				memset(sids, 0, sizeof(StateId)*order);
				vids = new VocabId[order];
				memset(vids, 0, sizeof(VocabId)*order);
				count = 0;
				daid = 0;
				head = order-1;
			}

			State(const State &s):order(s.order), count(s.count), head(s.head), daid(s.daid){
				sids = new StateId[order];
				memcpy(sids, s.sids, sizeof(StateId)*order);
				vids = new VocabId[order];
				memcpy(vids, s.vids, sizeof(VocabId)*order);
			}
			
			virtual ~State(){
				delete [] sids;
				delete [] vids;
			}
			
			StateId &operator[](size_t i){
				return sids[i];
			}

			StateId get_sid(size_t i) const{
				return sids[i];
			}

			void set_count(unsigned short c){
				count = c;
			}

			unsigned short get_count(){
				return count;
			}

			void set_daid(size_t id){
				daid = id;
			}

			size_t get_daid(){
				return daid;
			}

			void refresh(){
				count=0;
			}

			void push_word(VocabId word){
				head = (head+order-1)%order;
				vids[head] = word;
			}
			
			void set_word(size_t i, VocabId word){
				vids[(head+i)%order] = word;
			}

			size_t get_word(size_t i){
				return vids[(head+i)%order];
			}

			unsigned short get_order(){
				return order;
			}

			int compare(State *state){
				for(size_t i = 0; i < count && i < state->count; i++){
					int diff = get_word(i) - state->get_word(i);
					if(diff!=0) return diff;
				}
				return count-state->count;
			}

		private:
			unsigned short order;
			unsigned short count;
			unsigned short head;
			StateId *sids;
			VocabId *vids;
			size_t daid;
	};
}

#endif //DALM_STATE_H_


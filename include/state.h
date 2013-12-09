#ifndef DALM_STATE_H_
#define DALM_STATE_H_

#include<cstring>
#include<vocabulary.h>

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
			
			virtual ~State(){
				delete [] sids;
			}
			
			StateId &operator[](size_t i){
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


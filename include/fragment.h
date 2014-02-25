#ifndef FRAGMENT_H_
#define FRAGMENT_H_

#include "state.h"

namespace DALM {
	class Fragment {
		public:
			Fragment():head(0),depth(0),prob(0.0){}
			virtual ~Fragment(){}
	
			inline void set(StateId id, unsigned short d, float p){
				head = id;
				depth = d;
				prob = p;
			}

			Fragment &operator=(const Fragment &f){
				head = f.head;
				depth = f.depth;
				prob = f.prob;

				return *this;
			}
	
			inline StateId get_state_id() const{
				return head;
			}	

			inline unsigned short get_depth() const{
				return depth;
			}
	
			inline float get_prob() const{
				return prob;
			}

		private:
			StateId head;
			unsigned short depth;
			float prob;
	};

	class Gap {
		public:
			Gap(){
				gap = 0;
			}
			virtual ~Gap(){}
	
			inline size_t get_gap(){ return gap; }
			inline void succ(){
				gap++;
			}

		private:
			size_t gap;
	};
}

#endif //FRAGMENT_H_


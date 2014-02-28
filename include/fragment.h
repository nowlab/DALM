#ifndef FRAGMENT_H_
#define FRAGMENT_H_

#include "vocabulary.h"
#include "state.h"

namespace DALM {
	struct Fragment {
		StateId pos;
		unsigned char depth;
		unsigned char daid;
		float prob;
	};

	int compare_fragments(const Fragment &f1, const Fragment &f2);

	class Gap {
		public:
			Gap(State &/*s*/){
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


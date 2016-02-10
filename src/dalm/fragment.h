#ifndef FRAGMENT_H_
#define FRAGMENT_H_

#include "vocabulary.h"
#include "state.h"

namespace DALM {
	struct dainfo{
		VocabId word;
		int pos;
	};
	union Fragment {
		StateId sid;
		struct dainfo status;
	};

	int compare_fragments(const Fragment &f1, const Fragment &f2);

	class Gap {
		public:
			Gap(State &s){
				gap = 0;
			}
			virtual ~Gap(){}
	
			inline unsigned char get_gap() const{ return gap; }
			inline void succ(){
				gap++;
			}

			inline bool &is_finalized(){ return finalized; }
			inline bool &is_extended(){ return extended; }

		private:
			unsigned char gap;
			bool finalized;
			bool extended;
	};
}

#endif //FRAGMENT_H_


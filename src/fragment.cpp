#include "fragment.h"

int DALM::compare_fragments(const Fragment &f1, const Fragment &f2){
	if(f1.daid != f2.daid){
		return (f1.daid < f2.daid)?-1:1;
	}

	if(f1.pos != f2.pos){
		return (f1.pos < f2.pos)?-1:1;
	}

	return 0;
}


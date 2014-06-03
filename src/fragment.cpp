#include "fragment.h"

int DALM::compare_fragments(const Fragment &f1, const Fragment &f2){
	return f1.sid-f2.sid;
	/*
	if(f1.sid != f2.sid){
		return (f1.sid < f2.sid)?-1:1;
	}

	return 0;
	*/
}


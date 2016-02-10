#include "dalm/fragment.h"

int DALM::compare_fragments(const Fragment &f1, const Fragment &f2){
	//return f1.sid-f2.sid;
	if(f1.sid != f2.sid){
		//int ret = (f1.sid < f2.sid)?-1:1;
		//std::cerr << "compare_fragments=" << ret << std::endl;
		return (f1.sid < f2.sid)?-1:1;
	}

	//std::cerr << "compare_fragments=0" << std::endl;
	return 0;
}


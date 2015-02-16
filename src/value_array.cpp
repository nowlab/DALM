#include <cstdio>
#include <map>
#include <vector>
#include <algorithm>

#include "arpafile.h"
#include "value_array.h"

using namespace DALM;

class CmpFreq{
	public:
		bool operator()(
				const std::pair< float, unsigned int > &x, 
				const std::pair< float, unsigned int > &y){
			return x.second > y.second;
		}
};

ValueArray::ValueArray(std::string &pathtoarpa, Vocabulary &vocab, Logger &logger): logger(logger){
	std::map< float, unsigned int > value_map;

	logger << "[ValueArray] Couting values" << Logger::endi;
	count_values(pathtoarpa, vocab, value_map);	
	logger << "[ValueArray] Build value_array." << Logger::endi;
	set_value_array(value_map); // value_map is cleared.
	logger << "[ValueArray] value_array.size =" << size << Logger::endi;
}

ValueArray::ValueArray(FileReader &reader, Logger &logger): logger(logger){
	reader >> size;
	value_array = new float[size];
	reader.read_many(value_array, size);
}

void ValueArray::dump(std::FILE *fp){
	fwrite(&size, sizeof(size_t), 1, fp);
	fwrite(value_array, sizeof(float), size, fp);
}

ValueArray::~ValueArray(){
	delete [] value_array;
}

float ValueArray::operator[](size_t i){
	return value_array[i];
}

void ValueArray::count_values(
			std::string &pathtoarpa, 
			Vocabulary &vocab, 
			std::map< float, unsigned int > &value_map){

  ARPAFile *arpafile = new ARPAFile(pathtoarpa, vocab);
  size_t total = arpafile->get_totalsize();
	size_t tenpercent = total/10;
  for(size_t i=0; i<total; i++){
		if((i+1) % tenpercent == 0){
			logger << "[ValueArray] Reading ARPA: " << (i+1)/tenpercent << "0% done." << Logger::endi;
		}

    unsigned short n;
    VocabId *ngram;
    float prob;
    float bow;
    bool bow_presence;
    bow_presence = arpafile->get_ngram(n,ngram,prob,bow);
		if(bow_presence){
			if(value_map.find(bow)==value_map.end()){
				value_map[bow] = 1;
			}else{
				value_map[bow] += 1;
			}
		}
		delete [] ngram;
	}
	delete arpafile;
}

void ValueArray::set_value_array(std::map< float, unsigned int > &value_map){
	std::vector< std::pair<float, unsigned int> > values;
	values.reserve(value_map.size());

	std::map<float, unsigned int>::iterator it = value_map.begin();
	while(it != value_map.end()){
		values.push_back(*it);
		it++;
	}
	value_map.clear();

	std::sort(values.begin(), values.end(), CmpFreq());

	size = values.size()+1;
	value_array = new float[size];

	// special case for backoff.
	value_array[0] = -0.0;

	for(size_t i = 1; i < size; i++){
		value_array[i] = values[i-1].first;
	}
}


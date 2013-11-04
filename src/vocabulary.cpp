#include <cstring>

#include <fstream>
#include <sstream>

#include <vocabulary.h>

#include <darts.h>

using namespace DALM;

Vocabulary::Vocabulary(std::string vocabfilepath, std::string savedabinpath,std::string probnumfilepath, size_t offset, Logger &logger) :logger(logger) {
	char **vocabulary;
	size_t vocabcount = read_file(vocabfilepath, vocabulary);
	Darts::DoubleArray::result_type *id = new Darts::DoubleArray::result_type[vocabcount];

	std::ifstream ifs(probnumfilepath.c_str());
	std::string line;
	getline(ifs,line); 
	for(size_t i = 0; i < vocabcount; i++){
		size_t temp=0;
		std::stringstream ss;
		ss << line;
		ss >> temp;
		id[i] = temp+offset;
		getline(ifs,line);
	}

	da.build(vocabcount, (const char **)vocabulary, NULL, id, NULL);
	delete_vocabulary(vocabcount, vocabulary);

	da.save(savedabinpath.c_str());

	delete [] id;
	wcount = vocabcount;
	logger << "[Vocabulary::Vocabulary]wcount:" << wcount << Logger::endi;
}


Vocabulary::Vocabulary(std::string vocabfilepath, std::string savedabinpath, Logger &logger) :logger(logger) {
	char **vocabulary;
	size_t vocabcount = read_file(vocabfilepath, vocabulary);
	Darts::DoubleArray::result_type *id = new Darts::DoubleArray::result_type[vocabcount];

	for(size_t i = 0; i < vocabcount; i++){
		id[i] = i+1;
	}

	da.build(vocabcount, (const char **)vocabulary, NULL, id, NULL);
	delete_vocabulary(vocabcount, vocabulary);

	da.save(savedabinpath.c_str());

	delete [] id;
	wcount = vocabcount;
	logger << "[Vocabulary::Vocabulary]wcount:" << wcount << Logger::endi;
}

Vocabulary::Vocabulary(std::string dabinpath, Logger &logger) : logger(logger){
	if(da.open(dabinpath.c_str())==-1){
		logger << "Vocabulary: Darts binary file open error." << Logger::endc;
		throw "error";
	}
	wcount=0; //unsupported.
}

size_t Vocabulary::read_file(std::string &vocabfilepath, char **&vocabulary){
	std::ifstream fs(vocabfilepath.c_str());
	size_t vocabcount = 0;
	_VocabList *vl=new _VocabList();
	_VocabList *top = vl;

	if(!fs){
		logger << "Vocabulary File(" << vocabfilepath << ") open failed." << Logger::endc;
		throw "error";
	}

	std::string line;
	getline(fs, line);

	while(!fs.eof()){
		vocabcount++;

		vl->vocab = new char[line.size()+1];
		strcpy(vl->vocab, line.c_str());
		vl->next = new _VocabList();
		vl = vl->next;

		getline(fs, line);
	}

	vocabulary = new char*[vocabcount];

	size_t i=0;
	vl = top;
	while(vl->next != NULL){
		vocabulary[i] = vl->vocab;

		_VocabList *tmp = vl;
		vl = vl->next;
		delete tmp;
		i++;
	}
	delete vl;

	return vocabcount;
}

void Vocabulary::delete_vocabulary(size_t vocabcount, char **&vocabulary){
	for(size_t i=0; i < vocabcount; i++){
		delete [] vocabulary[i];
	}
	delete [] vocabulary;
}

VocabId Vocabulary::lookup(const char *vocab){
	Darts::DoubleArray::result_type id = da.exactMatchSearch<Darts::DoubleArray::result_type>(vocab);

	if(id < 0){
		return DALM_UNK_WORD;
	}else{
		return (VocabId) id;
	}
}

Vocabulary::~Vocabulary() {
	da.clear();
}


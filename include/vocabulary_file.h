#ifndef VOCABULARY_ITERATOR_H_
#define VOCABULARY_ITERATOR_H_

#include <string>
#include <fstream>

namespace DALM {

    class VocabularyFile{
    public:
        VocabularyFile(std::string wordstxt);
        virtual ~VocabularyFile(){}

        std::string next();
        bool eof();

    private:
        std::ifstream ifs;
        std::string line;
    };
}

#endif //VOCABULARY_ITERATOR_H_

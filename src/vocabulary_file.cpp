#include "vocabulary_file.h"

using namespace DALM;

VocabularyFile::VocabularyFile(std::string wordstxt) : ifs(wordstxt.c_str()){
    if(ifs.fail()){
        throw "fails to open WORDSTXT file.";
    }
}

std::string VocabularyFile::next() {
    return line;
}

bool VocabularyFile::eof() {
    std::getline(ifs, line);
    return ifs.eof();
}
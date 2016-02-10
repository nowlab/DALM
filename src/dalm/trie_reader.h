#ifndef REVERSE_TRIE_READER_H_
#define REVERSE_TRIE_READER_H_

#include "vocabulary.h"
#include "fileutil.h"

namespace DALM{
    namespace File {
        struct ARPAEntry{
            ARPAEntry(): prob(0.0), bow(0.0), n(0), bow_presence(false) {
                std::fill(this->words, this->words+DALM_MAX_ORDER, 0);
                std::fill(padding, padding+(1-DALM_MAX_ORDER%2)*4+2, 0);
            }
            ARPAEntry(DALM::VocabId const *words, unsigned char n, float prob, float bow, bool bow_presence, bool reverse=true)
                    : prob(prob), bow(bow), n(n), bow_presence(bow_presence){
                if(reverse){
                    for(std::size_t i = 0; i < n; ++i){
                        this->words[i] = words[n - i - 1];
                    }
                }else{
                    std::copy(words, words+n, this->words);
                }
                std::fill(this->words+n, this->words+DALM_MAX_ORDER, 0);
                std::fill(padding, padding+(1-DALM_MAX_ORDER%2)*4+2, 0);
            }

            DALM::VocabId words[DALM_MAX_ORDER];
            float prob;
            float bow;
            unsigned char n;
            bool bow_presence;
            unsigned char padding[(1-DALM_MAX_ORDER%2)*4+2]; // align to 8bytes in order to uninitialize worning by valgrind.
        };

        class TrieFileReader {
        public:
            TrieFileReader(std::string path) :reader_(path){
                reader_ >> n_entries_;
            }

            std::size_t size(){
                return n_entries_;
            }

            ARPAEntry next(){
                ARPAEntry entry;
                reader_ >> entry;
                return entry;
            }
        private:
            std::size_t n_entries_;
            BinaryFileReader reader_;
        };
    }
}

#endif //REVERSE_TRIE_READER_H_

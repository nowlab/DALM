#ifndef REVERSE_TRIE_READER_H_
#define REVERSE_TRIE_READER_H_

#include <limits>
#include <cmath>
#include "vocabulary.h"
#include "fileutil.h"

namespace DALM{
    namespace File {
        union ValueIndex{
          float value;
          int index;
        };
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

        class TrieNode{
        public:
            TrieNode():bow_presence(false),n_words(0),n_children(0),has_grandchild(false){
                unique_id = count_up_unique_id();
                parent_id = -1;
                children_has_bow = false;
                prob.value = 0;
                bow.value = 0;
            }
            TrieNode(int x):bow_presence(false),n_words(0),n_children(0),has_grandchild(false){
                static_unique_id = x;
                unique_id = count_up_unique_id();
                prob.value = -std::numeric_limits<float>::infinity();
                bow.value = -std::numeric_limits<float>::infinity();
                children_has_bow = false;
            }

            TrieNode(const ARPAEntry &arpa):bow_presence(arpa.bow_presence),n_words(0),n_children(0), has_grandchild(false){
                unique_id = count_up_unique_id();
                children_has_bow = false;
                prob.value = arpa.prob;
                bow.value = arpa.bow;
                if( std::isinf(arpa.bow)){
                    independent_right = true;
                    bow.index = 0.0f;
                }else{
                    independent_left = false;
                }

                if( prob.value > 0){
                    independent_left = true;
                    prob.value = -prob.value;
                }else{
                    independent_left = false;
                }

                for(size_t i = 0; i < arpa.n; i++){
                    words[n_words++] = arpa.words[i];
                }
            }
            
            TrieNode(const VocabId* words_, const size_t n):
            bow_presence(false),n_words(0),n_children(0),has_grandchild(false){
                unique_id = count_up_unique_id();
                prob.value = -std::numeric_limits<float>::infinity();
                bow.value = -std::numeric_limits<float>::infinity();
                children_has_bow = false;
                for(size_t i = 0; i < n; ++i){
                    words[n_words++] = words_[i];
                }
            }

            bool is_ancestor_of(const TrieNode &node) const {
                if(unique_id == 0)return true;
                if(n_words >= node.n_words || prefix_match(node) != n_words) return false;
                return true;
            }

            bool is_parent_of(const TrieNode &node) const{
                if(unique_id == 0 && node.n_words == 1)return true;
                if(n_words != node.n_words-1 || prefix_match(node) != n_words) return false;
                return true;
            }

            void add_word(VocabId vocab_id){
                words[n_words++] = vocab_id;
            }

            void add_child(int node_id){
                //children_ids.push_back(node_id);
                n_children++;
            }

            bool has_child(){
                return n_children>0;
            }

            int unique_id;
            int parent_id;
            ValueIndex prob;
            ValueIndex bow;
            bool bow_presence;
            bool independent_right;
            bool independent_left;
            size_t n_words;
            size_t n_children;
            DALM::VocabId words[DALM_MAX_ORDER+1];
            bool has_grandchild;
            bool children_has_bow;
            static int static_unique_id;
        private:

            int count_up_unique_id(){return static_unique_id++;};
            
            //何個一致したか？
            size_t prefix_match(const TrieNode &node)const {
                size_t x = std::min(n_words, node.n_words);
                for(size_t i = 0; i < x; ++i){
                    if(words[i] != node.words[i]) return i;
                }
                return x;
            }
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
        struct NodeInfo{
            int parent_id;
            int node_id;
            unsigned int word;
            int n_;
            static int tp;
            NodeInfo(){};
            NodeInfo(int parent_, int node_, unsigned int w, int n): parent_id(parent_), node_id(node_), word(w), n_(n){};
        };

        class TrieNodeFileReader {
        public:
            TrieNodeFileReader(std::string path) :reader_(path){
                reader_ >> n_entries_;
            }

            std::size_t size(){
                return n_entries_;
            }

            TrieNode next(){
                TrieNode entry;
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

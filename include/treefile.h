#ifndef TREEFILE_H_
#define TREEFILE_H_

#include<cstdio>
#include<cstdlib>

#include<fstream>
#include<vector>
#include <stdexcept>

#include "vocabulary.h"
#include "fileutil.h"

namespace DALM {
	class TreeFile {
	public:
		TreeFile(std::string treefile, Vocabulary &vocab) : vocab_(vocab), file_(treefile){
			read_header();
		}

		size_t get_ngramorder(){
			return ngramorder_;
		}

		size_t get_totalsize(){
			return total_;
		}

		size_t num_by_order(size_t order){
			return ngramnums_[order-1];
		}
			
		Vocabulary &get_vocab(){ return vocab_; }

		void get_ngram(unsigned short &order, VocabId *&ngram, float &value){
            std::string word;
            std::vector<VocabId> ngram_vec;
            ngram_vec.reserve(ngramorder_);
            char sep;
            order = 1;
            while((sep = file_.read_token(word, " \t\n"))== ' '){
                ngram_vec.push_back(vocab_.lookup(word.c_str()));
                ++order;
            }
            ngram_vec.push_back(vocab_.lookup(word.c_str()));
            ngram = new VocabId[order];
            std::copy(ngram_vec.begin(), ngram_vec.end(), ngram);

            if(sep == '\t'){
                std::string value_str;
                sep = file_.read_token(value_str, "\n");
                if(sep != '\n'){
                    throw std::runtime_error("eof after value.");
                }
                value = (float)std::atof(value_str.c_str());
            }else{
                value = 0.0;
            }
		}

	private:
        std::string skip_blank() {
            std::string line;
            while(true){
                file_.read_token(line, "\n");
                if(!line.empty()) break;
            }
            return line;
        }

		void read_header(){
            ngramorder_ =0;
            total_ =0;
            std::string line = skip_blank();
            if(line != "\\data\\"){
                throw std::runtime_error("file format error. \\data\\ expected.");
            }

            std::string token;
            while(file_.read_token(token, "=\n") == '='){
                std::string num_str;
                if(file_.read_token(num_str, "\n") == '\n'){
                    std::size_t num = (std::size_t)atoi(num_str.c_str());
                    ngramnums_.push_back(num);
                    ++ngramorder_;
                    total_ +=num;
                }
            }

            if(token == ""){
                line = skip_blank();
            }else{
                line = token;
            }
			if(line != "\\n-grams:"){
				throw std::runtime_error("file format error. \\n-grams: expected.");
			}
		}

		Vocabulary &vocab_;

		std::size_t ngramorder_;
		std::size_t total_;
		std::vector<size_t> ngramnums_;
		TextFileReader file_;
	};
}

#endif //TREEFILE_H_

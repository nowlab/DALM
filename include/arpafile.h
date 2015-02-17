#ifndef ARPAFILE_H_
#define ARPAFILE_H_

#include<cstdio>
#include<cstdlib>
#include<cstring>

#include<fstream>
#include<stdexcept>
#include<vector>

#include "logger.h"
#include "vocabulary.h"
#include "fileutil.h"

namespace DALM {
	class ARPAFile {
	public:
		ARPAFile(std::string arpafile, Vocabulary &vocab) : vocab_(vocab), file_(arpafile.c_str()), processing_order_(0), index_(0){
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

		bool get_ngram(unsigned short &order, VocabId *&ngram, float &prob, float &bow){
            std::string prob_str;
            if(file_.read_token(prob_str, "\t") != '\t'){
                throw std::runtime_error("eof after prob.");
            }

            bool bow_presence = true;
            prob = (float)std::atof(prob_str.c_str());
            order = (unsigned short)processing_order_;
            ngram = new VocabId[order];
            std::string word;
            char sep = '\0';
            for(size_t i = 0; i < processing_order_; i++){
                sep = file_.read_token(word, " \t\n");
                ngram[i] = vocab_.lookup(word.c_str());
            }

            if(sep == ' '){
                throw std::runtime_error("ARPA file format error. # of words mismatch.");
            }else if(sep == '\t'){
                std::string bow_str;
                sep = file_.read_token(bow_str, "\n");
                if(sep != '\n'){
                    throw std::runtime_error("eof after bow.");
                }
                bow = (float)std::atof(bow_str.c_str());
            }else{
                bow = 0.0;
                bow_presence = false;
            }

			++index_;
			if(index_ >= ngramnums_[processing_order_ - 1]){
                std::string line = skip_blank();

				if(processing_order_ == ngramorder_){
					if(line != "\\end\\"){
						throw "ARPA file format error. \\end\\ expected.";
					}
				}else{
					std::ostringstream oss;
					oss << "\\";
					oss << processing_order_+1;
					oss << "-grams:";
					std::string expected = oss.str();
					if(line != expected){
						throw std::runtime_error("ARPA file format error. " + expected + " expected.");
					}
				}

				index_ = 0;
				++processing_order_;
			}

			return bow_presence;
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
				throw std::runtime_error("ARPA file format error. \\data\\ expected.");
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
			if(line != "\\1-grams:"){
				throw std::runtime_error("ARPA file format error. \\1-grams: expected.");
			}
			processing_order_ = 1;
			index_ = 0;
		}

		Vocabulary &vocab_;
		size_t ngramorder_;
		size_t total_;
		std::vector<size_t> ngramnums_;
		TextFileReader file_;
		std::size_t processing_order_;
		std::size_t index_;
	};
}

#endif //ARPAFILE_H_

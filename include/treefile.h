#ifndef TREEFILE_H_
#define TREEFILE_H_

#include<cstdio>
#include<cstdlib>

#include<fstream>
#include<vector>
#include <stdexcept>

#include "vocabulary.h"

namespace DALM {
	class TreeFile {
	public:
		TreeFile(std::string treefile, Vocabulary &vocab) : vocab_(vocab), file_(treefile.c_str()){
			if(!file_){
				throw std::runtime_error("error");
			}

			read_header();
		}

		~TreeFile(){
			file_.close();
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
			std::string line;
			std::getline(file_, line);
			if(file_.eof()){
				throw std::runtime_error("eof");
			}

			std::size_t tabindex = line.find('\t');
			if(tabindex == std::string::npos || tabindex == 0){
				throw std::runtime_error("error");
			}

			std::vector<VocabId> ngram_vec;
			ngram_vec.reserve(ngramorder_);
			std::string words = line.substr(0, tabindex);
			std::istringstream iss(words);
			std::string token;
			order = 0;
			iss >> token;
			while(!iss.eof()){
				ngram_vec.push_back(vocab_.lookup(token.c_str()));
				++order;
				iss >> token;
			}
			ngram_vec.push_back(vocab_.lookup(token.c_str()));
			++order;
			ngram = new VocabId[order];
			std::copy(ngram_vec.begin(), ngram_vec.end(), ngram);

			std::string valuestr = line.substr(tabindex+1);
			if(valuestr.empty()) value = 0.0;
			else value = (float)std::atof(valuestr.c_str());
		}

	private:
		void read_header(){
			ngramorder_ =0;
			total_ =0;

			std::string line;
			std::getline(file_, line); // skip \data\ line.
			if(line != "\\data\\"){
				throw std::runtime_error("error");
			}
			std::getline(file_, line);
			while(!file_.eof()){
				std::size_t num = (std::size_t)std::atoi(line.substr(8).c_str());
				ngramnums_.push_back(num);
				++ngramorder_;
				total_ +=num;

				std::getline(file_, line);
				if(line.empty()){
					break;
				}
			}

			std::getline(file_, line);
			if(line != "\\n-grams:"){
				throw std::runtime_error("error");
			}
		}

		Vocabulary &vocab_;

		std::size_t ngramorder_;
		std::size_t total_;
		std::vector<size_t> ngramnums_;
		std::ifstream file_;
	};
}

#endif //TREEFILE_H_

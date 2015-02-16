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
			if(!file_){
				throw std::runtime_error("error");
			}

			read_header();
		}

		virtual ~ARPAFile(){
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

		bool get_ngram(unsigned short &order, VocabId *&ngram, float &prob, float &bow){
			std::string line;
			std::getline(file_, line);
			if(file_.eof()){
				throw std::runtime_error("eof");
			}

			bool bow_presence = true;
			std::istringstream iss(line);
			iss >> prob;

			order = (unsigned short)processing_order_;
			ngram = new VocabId[order];
			for(size_t i = 0; i < processing_order_; i++){
				std::string word;
				iss >> word;
				ngram[i] = vocab_.lookup(word.c_str());
			}

			if(iss.eof()){
				bow = 0.0;
				bow_presence = false;
			}else{
				iss >> bow;
			}

			++index_;
			if(index_ >= ngramnums_[processing_order_ - 1]){
				while (std::getline(file_, line) && line.empty()) {} // skip blank line.

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
		void read_header(){
			ngramorder_ =0;
			total_ =0;
			std::string line;

			while (std::getline(file_, line) && line.empty()) {} // skip blank line.
			if(line != "\\data\\"){
				throw std::runtime_error("error.");
			}

			std::getline(file_, line);
			while(!file_.eof()){
				std::size_t num = (std::size_t)atoi(line.substr(8).c_str());
				ngramnums_.push_back(num);
				++ngramorder_;
				total_ +=num;

				std::getline(file_, line);
				if(line == "") break;
			}
			while (std::getline(file_, line) && line.empty()) {} // skip blank line.
			if(line != "\\1-grams:"){
				throw std::runtime_error("ARPA file format error.");
			}
			processing_order_ = 1;
			index_ = 0;
		}

		Vocabulary &vocab_;
		size_t ngramorder_;
		size_t total_;
		std::vector<size_t> ngramnums_;
		std::ifstream file_;
		std::size_t processing_order_;
		std::size_t index_;
	};
}

#endif //ARPAFILE_H_

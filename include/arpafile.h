#ifndef ARPAFILE_H_
#define ARPAFILE_H_

#include<cstdio>
#include<cstdlib>
#include<cstring>

#include<fstream>
#include<stdexcept>
#include<vector>
#include <algorithm>

#include "logger.h"
#include "vocabulary.h"
#include "fileutil.h"

namespace DALM {
	class ARPAFile {
		struct UnigramEntry{
			std::string word;
			float prob;
			float bow;
		};
		struct UnigramCompare{
			bool operator()(UnigramEntry const &u1, UnigramEntry const &u2) const {
				// for compatibility of mkworddict.sh
				// simulate LC_ALL=C sort -k1nr -k3nr unigrams.txt
				return u1.prob > u2.prob  || (u1.prob == u2.prob && u1.bow > u2.bow) || (u1.prob == u2.prob && u1.bow == u2.bow && u1.word < u2.word);
			}
		};
		struct UnigramOrder{
			std::string word;
			VocabId id;
		};
		struct UnigramOrderCompare{
			bool operator()(UnigramOrder const &u1, UnigramOrder const &u2) const {
				return u1.word < u2.word;
			}
		};

	public:
		ARPAFile(std::string arpafile, Vocabulary &vocab) : vocab_(vocab), file_(arpafile), processing_order_(0), index_(0){
			read_header(file_, ngramorder_, total_, ngramnums_, processing_order_, index_);
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
                std::string line = skip_blank(file_);

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

        static void create_vocab_and_order(std::string arpafile, std::vector<std::string> &words, std::vector<VocabId> &ids){
			TextFileReader file(arpafile);
			size_t ngramorder;
			size_t total;
			std::vector<size_t> ngramnums;
			std::size_t processing_order;
			std::size_t index;

			read_header(file, ngramorder, total, ngramnums, processing_order, index);

			std::size_t unigram_count = ngramnums[0];
			std::vector< UnigramEntry > unigrams;
			unigrams.reserve(unigram_count);

			for(index=0; index < unigram_count; ++index){
				std::string prob_str;
				if(file.read_token(prob_str, "\t") != '\t'){
					throw std::runtime_error("eof after prob.");
				}

				UnigramEntry entry;
				entry.prob = (float)std::atof(prob_str.c_str());
				entry.bow = 0.0;
				char sep = file.read_token(entry.word, " \t\n");

				if(sep == ' '){
					throw std::runtime_error("ARPA file format error. # of words mismatch.");
				}else if(sep == '\t'){
					std::string bow_str;
					sep = file.read_token(bow_str, "\n");
					if(sep != '\n'){
						throw std::runtime_error("eof after bow.");
					}
					entry.bow = (float)std::atof(bow_str.c_str());
				}
				unigrams.push_back(entry);
			}

			// for compatibility of mkworddict.sh
			// simulate LC_ALL=C sort -k1nr -k3nr unigrams.txt
			std::stable_sort(unigrams.begin(), unigrams.end(), UnigramCompare());

			std::vector<UnigramOrder> word_order;
			word_order.reserve(unigram_count + 1);
			UnigramOrder o;
			o.word = "<#>";
			o.id = 1;
			word_order.push_back(o);
			for(std::size_t i = 0; i < unigram_count; i++){
				UnigramOrder o;
				o.word = unigrams[i].word;
				o.id = (VocabId)i + 2;
				word_order.push_back(o);
			}
			std::sort(word_order.begin(), word_order.end(), UnigramOrderCompare());
			words.reserve(word_order.size());
			ids.reserve(word_order.size());
			for(std::size_t i = 0; i < word_order.size(); i++){
				words.push_back(word_order[i].word);
				ids.push_back(word_order[i].id);
			}
        }

	private:

        static std::string skip_blank(TextFileReader &file) {
            std::string line;
            while(true){
                file.read_token(line, "\n");
                if(!line.empty()) break;
            }
            return line;
        }

		static void read_header(
				TextFileReader &file, std::size_t &ngramorder, std::size_t &total,
				std::vector<std::size_t> &ngramnums, std::size_t &processing_order, std::size_t &index){
			ngramorder =0;
			total =0;
			std::string line = skip_blank(file);
			if(line != "\\data\\"){
				throw std::runtime_error("ARPA file format error. \\data\\ expected.");
			}

            std::string token;
            while(file.read_token(token, "=\n") == '='){
                std::string num_str;
                if(file.read_token(num_str, "\n") == '\n'){
                    std::size_t num = (std::size_t)atoi(num_str.c_str());
                    ngramnums.push_back(num);
                    ++ngramorder;
                    total +=num;
                }
            }

            if(token == ""){
                line = skip_blank(file);
            }else{
                line = token;
            }
			if(line != "\\1-grams:"){
				throw std::runtime_error("ARPA file format error. \\1-grams: expected.");
			}
			processing_order = 1;
			index = 0;
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

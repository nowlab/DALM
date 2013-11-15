#ifndef TREEFILE_H_
#define TREEFILE_H_

#include<cstdio>
#include<cstdlib>

#include<fstream>
#include<vector>

#include<vocabulary.h>

namespace DALM {
	class TreeFile {
		public:
			TreeFile(std::string dumpfile, Vocabulary &vocab) : vocab(vocab){
				fp = std::fopen(dumpfile.c_str(), "rb");

				std::fread(&ngramorder, sizeof(size_t), 1, fp);
				std::fread(&total, sizeof(size_t), 1, fp);

				for(size_t i = 0; i < ngramorder; i++){
					size_t num;
					std::fread(&num, sizeof(size_t), 1, fp);
					ngramnums.push_back(num);
				}
			}

			~TreeFile(){
				fclose(fp);
			}

			size_t get_ngramorder(){
				return ngramorder;
			}

			size_t get_totalsize(){
				return total;
			}

			size_t num_by_order(size_t order){
				return ngramnums[order-1];
			}
			
			Vocabulary &get_vocab(){ return vocab; }

			void get_ngram(unsigned short &order, VocabId *&ngram, float &value){
				std::fread(&order, sizeof(unsigned short), 1, fp);
				ngram = new VocabId[order];

				//std::cerr << "[ ";
				for(size_t i = 0; i < order; i++){
					unsigned short wsize=0;
					std::fread(&wsize, sizeof(unsigned short), 1, fp);

					char *word = new char[wsize+1];
					std::fread(word, sizeof(char), wsize, fp);
					word[wsize] = '\0';
					//std::cerr << word << ":";

					ngram[i] = vocab.lookup(word);
					//std::cerr << ngram[i] << " ";
					delete [] word;
				}
				//std::cerr << "]" << std::endl;

				std::fread(&value, sizeof(float), 1, fp);
			}

			static void dump(std::string TreeFile, std::string dumpFile, Logger &logger){
				logger << "[TreeFile::dump] TreeFile=" << TreeFile << Logger::endi;
				logger << "[TreeFile::dump] dumpFile=" << dumpFile << Logger::endi;

				std::ifstream treestream(TreeFile.c_str());
				FILE *outfp = std::fopen(dumpFile.c_str(), "wb");

				if(!treestream){
					logger << "[TreeFile::dump] ARPA File(filepath=" << TreeFile << ") have IO error." << Logger::endc;
					throw "error";
				}

				size_t total = dump_header(treestream, outfp, logger);
				dump_ngrams(treestream, outfp, total, logger);

				std::fclose(outfp);
			}

		private:

			static size_t dump_header(std::ifstream &treestream, FILE *outfp, Logger &logger){
				size_t ngramorder=0;
				size_t total=0;
				std::vector<size_t> ngramnums;
				std::string line;

				std::getline(treestream, line); // skip \data\ line.
				if(line != "\\data\\"){
					logger << "ARPA file format error. abort." << Logger::endc;
					throw "error.";
				}

				std::getline(treestream, line);

				while(!treestream.eof()){
					size_t num = std::atoi(line.substr(8).c_str());
					ngramnums.push_back(num);
					ngramorder++;
					total+=num;

					std::getline(treestream, line);
					if(line.substr(0, 5) == ""){
						break;
					}
				}

				std::fwrite(&ngramorder, sizeof(size_t), 1, outfp);
				std::fwrite(&total, sizeof(size_t), 1, outfp);
				std::fwrite(&(ngramnums[0]), sizeof(size_t), ngramnums.size(), outfp);

				logger << "[TreeFile::dump_header] ngramorder=" << ngramorder << Logger::endi;
				logger << "[TreeFile::dump_header] total=" << total << Logger::endi;
				for(size_t i = 0; i < ngramnums.size(); i++){
					logger << "[TreeFile::dump_header] order[" << i+1 << "]=" << ngramnums[i] << Logger::endi;
				}

				return total;
			}

			static void dump_ngrams(std::ifstream &treestream, FILE *outfp, size_t total, Logger &logger){
				size_t count = 0;
				size_t ten_percent = total/10;

				std::string line;
				std::getline(treestream, line); // skip \n-grams: line.
				std::getline(treestream, line);
				while(!treestream.eof()){
					dump_ngram(outfp, line, logger);
					count++;

					if(ten_percent != 0 && count % ten_percent == 0){
						logger << count / ten_percent << "0% Done." << Logger::endi;
					}

					getline(treestream, line);
					if(line == "") break;
				}

				std::getline(treestream, line); // skip \end\ line.
			}

			// [order/2bytes] {[charbytes/2bytes] [words/(charbytes)bytes]}* [value/4bytes]
			static void dump_ngram(FILE *outfp, std::string &line, Logger &logger){
				std::vector<std::string> ngram;
				std::string token;
				float value;

				size_t tabindex = line.find('\t');
				if(tabindex == std::string::npos || tabindex == 0){
					logger << "File Format Error! line=" << line << Logger::endc;
					throw "error";
				}

				std::string words = line.substr(0, tabindex);

				std::istringstream iss(words);
				iss >> token;
				while(!iss.eof()){
					ngram.push_back(token);

					iss >> token;
				}
				ngram.push_back(token);

				unsigned short n = ngram.size();
				std::fwrite(&n, sizeof(unsigned short), 1, outfp);
				for(size_t i = 0; i < n; i++){
					unsigned short wsize = ngram[i].size();
					//std::cerr << "[" <<ngram[i] << "] ";
					std::fwrite(&wsize, sizeof(unsigned short), 1, outfp);
					std::fwrite(ngram[i].c_str(), sizeof(char), wsize, outfp);
				}
				//std::cerr << std::endl;

				std::string valuestr = line.substr(tabindex+1);
				//std::cerr << "valuestr=[" << valuestr << "] ";
				if(valuestr=="") value=0.0;
				else value = std::atof(valuestr.c_str());
				//std::cerr << " value=" << value << std::endl;
				std::fwrite(&value, sizeof(float), 1, outfp);
			}

			Vocabulary &vocab;

			size_t ngramorder;
			size_t total;
			std::vector<size_t> ngramnums;
			FILE *fp;
	};
}

#endif //TREEFILE_H_

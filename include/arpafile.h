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

namespace DALM {
	class ARPAFile {
		public:
			ARPAFile (std::string dumpfile, Vocabulary &vocab) : vocab(vocab){
				fp = std::fopen(dumpfile.c_str(), "rb");

				std::fread(&ngramorder, sizeof(size_t), 1, fp);
				std::fread(&total, sizeof(size_t), 1, fp);

				for(size_t i = 0; i < ngramorder; i++){
					size_t num;
					std::fread(&num, sizeof(size_t), 1, fp);
					ngramnums.push_back(num);
				}
			}

			~ARPAFile(){
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

			bool get_ngram(unsigned short &order, VocabId *&ngram, float &prob, float &bow){
				std::fread(&order, sizeof(unsigned short), 1, fp);
				ngram = new VocabId[order];

				for(size_t i = 0; i < order; i++){
					unsigned short wsize;
					std::fread(&wsize, sizeof(unsigned short), 1, fp);

					char *word = new char[wsize+1];
					std::fread(word, sizeof(char), wsize, fp);
					word[wsize] = '\0';
					ngram[i] = vocab.lookup(word);
					delete [] word;
				}
				bool bow_presence=false;
				std::fread(&bow_presence,sizeof(bool),1,fp);
				std::fread(&prob, sizeof(float), 1, fp);
				std::fread(&bow, sizeof(float), 1, fp);
				return bow_presence;
			}

			static void dump(std::string arpaFile, std::string dumpFile, Logger &logger){
				logger << "[ARPAFile::dump] arpaFile=" << arpaFile << Logger::endi;
				logger << "[ARPAFile::dump] dumpFile=" << dumpFile << Logger::endi;

				std::ifstream arpastream(arpaFile.c_str());
				FILE *outfp = std::fopen(dumpFile.c_str(), "wb");

				if(!arpastream){
					logger << "[ARPAFile::dump] ARPA File(filepath=" << arpaFile << ") have IO error." << Logger::endc;
					throw "error";
				}

				size_t total = dump_header(arpastream, outfp, logger);
				dump_ngrams(arpastream, outfp, total, logger);

				std::fclose(outfp);
			}

		private:

			static size_t dump_header(std::ifstream &arpastream, FILE *outfp, Logger &logger){
				size_t ngramorder=0;
				size_t total=0;
				std::vector<size_t> ngramnums;
				std::string line;

				while (std::getline(arpastream, line) && line.empty()) {} // skip blank line.
				if(line != "\\data\\"){
					logger << "ARPA file format error. abort." << Logger::endc;
					throw std::runtime_error("error.");
				}

				std::getline(arpastream, line);
				while(!arpastream.eof()){
					size_t num = atoi(line.substr(8).c_str());
					ngramnums.push_back(num);
					ngramorder++;
					total+=num;

					std::getline(arpastream, line);
					if(line == "") break;
				}

				std::fwrite(&ngramorder, sizeof(size_t), 1, outfp);
				std::fwrite(&total, sizeof(size_t), 1, outfp);
				std::fwrite(&(ngramnums[0]), sizeof(size_t), ngramnums.size(), outfp);

				logger << "[ARPAFile::dump_header] ngramorder=" << ngramorder << Logger::endi;
				logger << "[ARPAFile::dump_header] total=" << total << Logger::endi;
				for(size_t i = 0; i < ngramnums.size(); i++){
					logger << "[ARPAFile::dump_header] order[" << i+1 << "]=" << ngramnums[i] << Logger::endi;
				}

				return total;
			}

			static void dump_ngrams(std::ifstream &arpastream, FILE *outfp, size_t total, Logger &logger){
				size_t count = 0;
				size_t ten_percent = total/10;
				std::string line;
				std::getline(arpastream, line);
				while(!arpastream.eof()){
					unsigned short n = (unsigned short) atoi(line.substr(1,1).c_str()); // \n-gram

					std::getline(arpastream, line);
					while(!arpastream.eof()){
						dump_ngram(outfp, line, n, logger);
						count++;

						if(ten_percent!=0 && count % ten_percent == 0){
							logger << count / ten_percent << "0% Done." << Logger::endi;
						}

						getline(arpastream, line);
						if(line == "") break;
					}

					std::getline(arpastream, line);
					if(line=="\\end\\"){
						break;
					}
				}
			}

			// [order/2bytes] {[charbytes/2bytes] [words/(charbytes)bytes]}* [prob/4bytes] [bow/4bytes]
			static void dump_ngram(FILE *outfp, std::string &line, unsigned short n, Logger &logger){
				std::fwrite(&n, sizeof(unsigned short), 1, outfp);
				float prob = 0.0;
				float bow = 0.0;
				bool bow_presence = true;
				std::istringstream iss(line);
				iss >> prob;

				for(size_t i = 0; i < n; i++){
					std::string word;
					iss >> word;
					unsigned short wsize = word.size();
					std::fwrite(&wsize, sizeof(unsigned short), 1, outfp);
					std::fwrite(word.c_str(), sizeof(char), wsize, outfp);			
				}

				if(iss.eof()) bow_presence = false;
				else iss >> bow;

				std::fwrite(&bow_presence , sizeof(bool),1,outfp);
				std::fwrite(&prob, sizeof(float), 1, outfp);
				std::fwrite(&bow, sizeof(float), 1, outfp);
			}

			Vocabulary &vocab;
			size_t ngramorder;
			size_t total;
			std::vector<size_t> ngramnums;
			FILE *fp;
	};
}

#endif //ARPAFILE_H_

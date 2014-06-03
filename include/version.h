#ifndef VERSION_H_
#define VERSION_H_

#define DALM_FILE_TYPE 0xDA73
#define DALM_FILE_VERSION 6

#define DALM_OPT_UNDEFINED 0
#define DALM_OPT_EMBEDDING 1

#include<cstdio>

#include "logger.h"

namespace DALM {
	class Version {
		public:
			Version(){
				type = DALM_FILE_TYPE;
				version = DALM_FILE_VERSION;
				opt = DALM_OPT_EMBEDDING;
			}
			
			Version(unsigned int optimize, Logger &logger){
				type = DALM_FILE_TYPE;
				version = DALM_FILE_VERSION;
				if(optimize!=DALM_OPT_EMBEDDING){
					logger << "[Version::Version] Unrecognized format." << Logger::ende;
					throw "[Version::Version] Unrecognized format.";
				}
				
				opt = optimize;
			}

			Version(FILE *fp, Logger &logger){
				unsigned int ftype;
				unsigned int fversion;
				unsigned int fopt=0;

				fread(&ftype, sizeof(unsigned int), 1, fp);
				fread(&fversion, sizeof(unsigned int), 1, fp);
				
				if(ftype!=DALM_FILE_TYPE){
					logger << "[Version::Version] Unrecognized filetype." << Logger::ende;
					throw "[Version::Version] Unrecognized filetype.";
				}

				if(fversion==DALM_FILE_VERSION){
					type=ftype;
					version=fversion;
					fread(&fopt, sizeof(unsigned int), 1, fp);
					if(fopt!=DALM_OPT_EMBEDDING){
						logger << "[Version::Version] Unrecognized optimization." << Logger::ende;
						throw "[Version::Version] Unrecognized optimization.";
					}
					opt=fopt;
				}else{
					logger << "[Version::Version] Unrecognized version. version=" << fversion << " If you update DALM, please rebuild the model" << Logger::ende;
					throw "[Version::Version] Unrecognized version.";
				}
			}

			virtual ~Version(){}

			void dump(FILE *fp){
				fwrite(&type, sizeof(unsigned int), 1, fp);
				fwrite(&version, sizeof(unsigned int), 1, fp);
				fwrite(&opt, sizeof(unsigned int), 1, fp);
			}

			unsigned int get_opt(){ return opt; }

		private:
			unsigned int type;
			unsigned int version;
			unsigned int opt;
	};
}

#endif //VERSION_H_


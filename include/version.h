#ifndef VERSION_H_
#define VERSION_H_

#define DALM_FILE_TYPE 0xDA73
#define DALM_FILE_VERSION 2.0

#include<cstdio>

#include "logger.h"

namespace DALM {
	class Version {
		public:
			Version(){
				type = DALM_FILE_TYPE;
				version = DALM_FILE_VERSION;
			}

			Version(FILE *fp, Logger &logger){
				type = DALM_FILE_TYPE;
				version = DALM_FILE_VERSION;

				unsigned int ftype;
				float fversion;
				fread(&ftype, sizeof(unsigned int), 1, fp);
				fread(&fversion, sizeof(float), 1, fp);
				if(ftype!=type || fversion!=version){
					logger << "[Version::Version] Unrecognized format. If you update DALM, please rebuild the model." << Logger::ende;
					throw "[Version::Version] Unrecognized format.";
				}
			}
			virtual ~Version(){}

			void dump(FILE *fp){
				fwrite(&type, sizeof(unsigned int), 1, fp);
				fwrite(&version, sizeof(float), 1, fp);
			}
		private:
			unsigned int type;
			float version;
	};
}

#endif //VERSION_H_


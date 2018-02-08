#ifndef VERSION_H_
#define VERSION_H_

#define DALM_FILE_TYPE 0xDA73
#define DALM_FILE_VERSION 8

#define DALM_OPT_UNDEFINED 0
#define DALM_OPT_EMBEDDING 1
#define DALM_OPT_REVERSE 2
// #define DALM_OPT_REVERSE_TRIE 3
#define DALM_OPT_BST 4
#define DALM_OPT_PT 8
#define DALM_OPT_QPT 9

#include<cstdio>

#include "logger.h"

namespace DALM {
    class Version {
        public:
            Version(){
                type = DALM_FILE_TYPE;
                version = DALM_FILE_VERSION;
                opt = DALM_OPT_REVERSE;
            }
            
            Version(unsigned int optimize, Logger &logger){
                type = DALM_FILE_TYPE;
                version = DALM_FILE_VERSION;
                if(optimize!=DALM_OPT_REVERSE && optimize!=DALM_OPT_EMBEDDING && optimize != DALM_OPT_BST &&
                    optimize!=DALM_OPT_PT && optimize!=DALM_OPT_QPT){
                    logger << "[Version::Version] Unrecognized format." << Logger::ende;
                    throw "[Version::Version] Unrecognized format.";
                }
                
                opt = optimize;
            }

            Version(BinaryFileReader &reader, Logger &logger){
                unsigned int ftype;
                unsigned int fversion;
                unsigned int fopt=0;

                reader >> ftype;
                reader >> fversion;

                if(ftype!=DALM_FILE_TYPE){
                    logger << "[Version::Version] Unrecognized filetype." << Logger::ende;
                    throw "[Version::Version] Unrecognized filetype.";
                }

                if(fversion==DALM_FILE_VERSION){
                    type=ftype;
                    version=fversion;
                    reader >> fopt;
                    if(fopt!=DALM_OPT_EMBEDDING && fopt!=DALM_OPT_REVERSE && fopt != DALM_OPT_BST && 
                        fopt!=DALM_OPT_PT && fopt!=DALM_OPT_QPT){
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

            void dump(BinaryFileWriter &writer){
                writer << type;
                writer << version;
                writer << opt;
            }

            unsigned int get_opt(){ return opt; }

        private:
            unsigned int type;
            unsigned int version;
            unsigned int opt;
    };
}

#endif //VERSION_H_


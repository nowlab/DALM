#ifndef _DALM_H_
#define _DALM_H_

#include "../src/dalm.h"

namespace DALM{
    class PyModel{
    public:
        PyModel(std::string path, std::string level){
            MSGLEVEL lv = LOGGER_DEBUG;
            if(level=="debug"){
                lv = LOGGER_DEBUG;
            }else if(level=="info"){
                lv = LOGGER_INFO;
            }else if(level=="warning"){
                lv = LOGGER_WARNING;
            }else if(level=="error"){
                lv = LOGGER_ERROR;
            }else if(level=="critical"){
                lv = LOGGER_CRITICAL;
            }
            logger = new Logger(stderr);
            logger->setLevel(lv);

            model = new Model(path, *logger);
        }

        virtual ~PyModel(){
            delete model;
            delete logger;
        }

        DALM::Model &raw_model() const {
            return *model;
        }

        DALM::Vocabulary &vocab() const {
            return *model->vocab;
        }

        DALM::LM &lm() const {
            return *model->lm;
        }

    private:
        Logger *logger;
        Model *model;
    };
}

#endif //_DALM_H_

#ifndef VALUE_ARRAY_H_
#define VALUE_ARRAY_H_

#include<cstdio>
#include<string>
#include<map>

#include "vocabulary.h"
#include "logger.h"
#include "fileutil.h"

namespace DALM{
    class ValueArray{
        public:
            ValueArray(std::string &pathtoarpa, Vocabulary &vocab, Logger &logger);
            ValueArray(BinaryFileReader &reader, Logger &logger);
            virtual ~ValueArray();

            float operator[](size_t i);
            void dump(BinaryFileWriter &writer);

            size_t length(){
                return size;
            }
        
        private:
            void count_values(std::string &pathtoarpa, Vocabulary &vocab, std::map< float, unsigned int > &value_map);
            void set_value_array(std::map< float, unsigned int > &value_map);
            float *value_array;
            size_t size;
            Logger &logger;
    };
}

#endif //VALUE_ARRAY_H_

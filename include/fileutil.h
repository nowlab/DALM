#ifndef FILEUTIL_H_
#define FILEUTIL_H_

#include <string>
#include <cstdio>
#include <stdexcept>
#include <assert.h>

#define DALM_BUFFER_SIZE 10000

namespace DALM{
    class BinaryFileReader {
    public:
        BinaryFileReader(std::string path){
            fp_ = std::fopen(path.c_str(), "rb");
            if(fp_ == NULL){
                throw std::runtime_error("File IO error.");
            }
        }
        virtual ~BinaryFileReader(){
            std::fclose(fp_);
        }

        template <typename T>
        BinaryFileReader &operator>>(T &d){
            read_or_die(&d, sizeof(T), 1);
            return *this;
        }

        template <typename T>
        void read_many(T *target, std::size_t n){
            read_or_die(target, sizeof(T), n);
        }

    private:
        void read_or_die(void *target, std::size_t size, std::size_t n){
            std::size_t n_read = std::fread(target, size, n, fp_);
            if(n_read < n){
                throw "File read error.";
            }
        }
        FILE *fp_;
    };

    class TextFileReader {
    public:
        TextFileReader(std::string path){
            fp_ = std::fopen(path.c_str(), "r");
            if(fp_ == NULL){
                throw std::runtime_error("File IO error.");
            }
            set_buffer();
        }
        ~TextFileReader(){
            std::fclose(fp_);
        }

        // MEMO separators must be end with \0.
        char read_token(std::string &token, char const *separators){
            token.clear();
            std::size_t begin = last_;
            while(true){
                for(bool has_sep = false; !has_sep; ++last_){
                    assert(last_ < DALM_BUFFER_SIZE);
                    for(char const *iter = separators; ;++iter){
                        if(buffer_[last_] == *iter){
                            has_sep = true;
                            break;
                        }
                        if(*iter == '\0') break;
                    }
                }
                --last_;
                if(buffer_[last_] == '\0'){
                    token += &(buffer_[begin]);
                    set_buffer();
                    begin = 0;
                    if(buffer_[0]=='\0') return '\0';
                }else{
                    char ret = buffer_[last_];
                    buffer_[last_] = '\0';
                    token += &(buffer_[begin]);
                    ++last_;
                    if(buffer_[last_]=='\0') set_buffer();
                    return ret;
                }
            }
        }

    private:
        void set_buffer(){
            if(std::fgets(buffer_, DALM_BUFFER_SIZE, fp_) == NULL) {
                if(!std::feof(fp_)){
                    throw std::runtime_error("File IO error.");
                }
            }
            last_ = 0;
        }

        char buffer_[DALM_BUFFER_SIZE];
        std::size_t last_;
        FILE *fp_;
    };
}

#endif //FILEUTIL_H_
#ifndef FILEUTIL_H_
#define FILEUTIL_H_

#include <string>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <sstream>
#include <errno.h>

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

        BinaryFileReader(FILE *fp){
            fp_ = fp;
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
                throw std::runtime_error("File read error.");
            }
        }
        FILE *fp_;
    };

    class BinaryFileWriter {
    public:
        BinaryFileWriter(std::string path){
            fp_ = std::fopen(path.c_str(), "wb");
            if(fp_ == NULL){
                throw std::runtime_error("File IO error.");
            }
        }

        BinaryFileWriter(FILE *fp){
            fp_ = fp;
        }

        virtual ~BinaryFileWriter(){
            if(fp_ != NULL) std::fclose(fp_);
        }

        FILE *pointer(){
            FILE *ret = fp_;
            fp_ = NULL;
            return ret;
        }

        template <typename T>
        BinaryFileWriter &operator<<(T const &d){
            write_or_die(&d, sizeof(T), 1);
            return *this;
        }

        template <typename T>
        void write_many(T const *target, std::size_t n){
            write_or_die(target, sizeof(T), n);
        }

        static BinaryFileWriter *get_temporary_writer(std::string tempdir){
            std::ostringstream path;
            path << tempdir << "/" << "tmpXXXXXX";
            std::string pathstr = path.str();
            char *tmp_path = new char[pathstr.length()+1];
            std::strncpy(tmp_path, pathstr.c_str(), pathstr.length());
            tmp_path[pathstr.length()] = '\0';

            int fd = mkstemp(tmp_path);
            if(fd != -1){
                FILE *fp = fdopen(fd, "w+b");
                if(fp != NULL){
                    unlink(tmp_path);
                }else{
                    throw std::runtime_error("cannot create temp file.");
                }
                delete [] tmp_path;
                return new DALM::BinaryFileWriter(fp);
            }else{
                delete [] tmp_path;
                throw std::runtime_error("cannot open temp file.");
            }
        }


    private:
        void write_or_die(void const *target, std::size_t size, std::size_t n){
            if(fp_ == NULL) throw std::runtime_error("invalid");
            std::size_t n_write = std::fwrite(target, size, n, fp_);
            if(n_write < n){
                throw std::runtime_error("File write error.");
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

    class TextFileWriter {
    public:
        TextFileWriter(std::string path){
            fp_ = std::fopen(path.c_str(), "w");
            if(fp_ == NULL){
                throw std::runtime_error("File IO error.");
            }
        }
        ~TextFileWriter(){
            std::fclose(fp_);
        }

        TextFileWriter &operator<<(std::string const &d){
            write_or_die(d.c_str());
            return *this;
        }

        TextFileWriter &operator<<(int v){
            write_or_die(v);
            return *this;
        }

        TextFileWriter &operator<<(long v){
            write_or_die(v);
            return *this;
        }

        TextFileWriter &operator<<(float v){
            write_or_die(v);
            return *this;
        }

        TextFileWriter &operator<<(unsigned int v){
            write_or_die(v);
            return *this;
        }

    private:
        void write_or_die(const char * target){
            if(fp_ == NULL) throw std::runtime_error("invalid");

            int n_write = std::fprintf(fp_, "%s", target);
            if(n_write < 0){
                throw std::runtime_error("File write error.");
            }
        }

        void write_or_die(int target){
            if(fp_ == NULL) throw std::runtime_error("invalid");

            int n_write = std::fprintf(fp_, "%d", target);
            if(n_write < 0){
                throw std::runtime_error("File write error.");
            }
        }

        void write_or_die(long target){
            if(fp_ == NULL) throw std::runtime_error("invalid");

            int n_write = std::fprintf(fp_, "%ld", target);
            if(n_write < 0){
                throw std::runtime_error("File write error.");
            }
        }

        void write_or_die(unsigned int target){
            if(fp_ == NULL) throw std::runtime_error("invalid");

            int n_write = std::fprintf(fp_, "%u", target);
            if(n_write < 0){
                throw std::runtime_error("File write error.");
            }
        }

        void write_or_die(float target){
            if(fp_ == NULL) throw std::runtime_error("invalid");

            int n_write = std::fprintf(fp_, "%f", target);
            if(n_write < 0){
                throw std::runtime_error("File write error.");
            }
        }

        FILE *fp_;
    };
}

#endif //FILEUTIL_H_

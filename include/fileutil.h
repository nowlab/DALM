#ifndef FILEUTIL_H_
#define FILEUTIL_H_

#include <string>
#include <cstdio>

namespace DALM{
    class FileReader{
    public:
        FileReader(std::string path){
            fp_ = std::fopen(path.c_str(), "rb");
            if(fp_ == NULL){
                throw "File IO error.";
            }
        }
        virtual ~FileReader(){
            std::fclose(fp_);
        }

        template <typename T>
        FileReader &operator>>(T &d){
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
}

#endif //FILEUTIL_H_
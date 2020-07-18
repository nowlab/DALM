#ifndef PTHREADWRAPPER_H_
#define PTHREADWRAPPER_H_

#include <pthread.h>

#include <fstream>
#include <vector>

#if defined(__linux__)
    #define CPUINFO "/proc/cpuinfo"
#elif defined(__APPLE__)
    #define GET_CORE_COMMAND "sysctl hw.ncpu"
    #define READ_START 10
#endif

namespace DALM {
    class PThreadWrapper {
        public:
            PThreadWrapper(){
                thread = 0;
            }
            virtual ~PThreadWrapper(){}
            virtual void run() = 0;

            void start(){
                pthread_create(&thread, NULL, (void* (*)(void*))PThreadWrapper::run_wrapper, (void *)this);
            }

            void join(){
                if(thread != 0){
                    pthread_join(thread, NULL);
                }
            }

            static std::size_t thread_available(int n_cores){
                int cpunum = (int)get_core_num();
                if(n_cores <= 0){
                    return (std::size_t) cpunum;
                }else if(cpunum > n_cores){
                    return (std::size_t) n_cores;
                }else{
                    return (std::size_t) cpunum;
                }
            }
        private:
            static std::size_t get_core_num(){
#if defined(__linux__)
                std::ifstream cpuinfo(CPUINFO);
                if(!cpuinfo){
                    return 1;
                }

                std::size_t cpunum;
                std::string data;
                cpuinfo >> data;
                while(!cpuinfo.eof()){
                    if(data == "processor"){
                        cpuinfo >> data;
                        cpuinfo >> cpunum;
                    }
                    cpuinfo >> data;
                }
                cpuinfo.close();
                cpunum++;
                return cpunum;
#elif defined(__APPLE__)
                FILE *cmdfp = NULL;
                int count = 0;
                while(cmdfp == NULL){
                    cmdfp = popen(GET_CORE_COMMAND, "r");
                    ++count;
                    if(count >= 50){
                        throw std::runtime_error("failed to get core number.");
                    }
                }
                std::vector<char> line;
                char c;
                
                while((c=fgetc(cmdfp))!=EOF){
                    line.push_back(c);
                }
                pclose(cmdfp);

                if(line.size() <= READ_START-1){
                    return 1;
                }

                return (std::size_t)atoi(&(line[READ_START-1]));
#else
                return 1;
#endif
            }

            static int run_wrapper(void *args){
                ((PThreadWrapper *)args)->run();
                return 0;
            }

            pthread_t thread;
    };
}

#endif //PTHREADWRAPPER_H_

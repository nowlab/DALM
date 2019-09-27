#ifndef PTHREADWRAPPER_H_
#define PTHREADWRAPPER_H_

#include <pthread.h>
#include <vector>
#include <fstream>

#if defined(__linux__)
	#define CPUINFO "/proc/cpuinfo"
#elif defined(__APPLE__)
	#define GET_CORE_COMMAND "sysctl hw.ncpu"
	#define READ_START 10
#endif
#define RESERVED "/tmp/cpu_reserved"

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

			static size_t thread_available(){
				size_t cpunum = get_core_num();
				std::ifstream reserved(RESERVED);
				size_t usednum=0;
				if(reserved){
					reserved >> usednum;
					reserved.close();
				}

				return cpunum-usednum;
			}

		private:

			static size_t get_core_num(){
#if defined(__linux__)
				std::ifstream cpuinfo(CPUINFO);
				if(!cpuinfo){
					return 1;
				}

				size_t cpunum;
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
				FILE *cmdfp = popen(GET_CORE_COMMAND, "r");
				std::vector<char> line;
				char c;
				
				while((c=fgetc(cmdfp))!=EOF){
					line.push_back(c);
				}
				pclose(cmdfp);
				
				return atoi(&(line[READ_START-1]));
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

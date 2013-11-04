#ifndef PTHREADWRAPPER_H_
#define PTHREADWRAPPER_H_

#include <pthread.h>

#include <fstream>

#define CPUINFO "/proc/cpuinfo"
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
				std::ifstream cpuinfo(CPUINFO);

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

				std::ifstream reserved(RESERVED);
				size_t usednum=0;
				if(reserved){
					reserved >> usednum;
					reserved.close();
				}

				return cpunum-usednum;
			}

		private:
			static int run_wrapper(void *args){
				((PThreadWrapper *)args)->run();
				return 0;
			}

			pthread_t thread;
	};
}

#endif //PTHREADWRAPPER_H_

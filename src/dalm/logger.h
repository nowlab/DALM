#ifndef LOGGER_H_
#define LOGGER_H_

#include<cstdio>

#include<iostream>
#include<sstream>
#include<string>
#include<mutex>

namespace DALM {
	typedef enum __level {
		LOGGER_DEBUG=0,
		LOGGER_INFO,
		LOGGER_WARNING,
		LOGGER_ERROR,
		LOGGER_CRITICAL
	} MSGLEVEL;

	class Logger{
		public:
			static const MSGLEVEL endd=LOGGER_DEBUG;
			static const MSGLEVEL endi=LOGGER_INFO;
			static const MSGLEVEL endw=LOGGER_WARNING;
			static const MSGLEVEL ende=LOGGER_ERROR;
			static const MSGLEVEL endc=LOGGER_CRITICAL;

			Logger(std::FILE *fp);
			void setLevel(MSGLEVEL level);
			void debug(const char *msg);
			void info(const char *msg);
			void warning(const char *msg);
			void error(const char *msg);
			void critical(const char *msg);

			void debug(const std::string &msg){ debug(msg.c_str()); }
			void info(const std::string &msg){ info(msg.c_str()); }
			void warning(const std::string &msg){ warning(msg.c_str()); }
			void error(const std::string &msg){ error(msg.c_str()); }
			void critical(const std::string &msg){ critical(msg.c_str()); }

			Logger &operator<<(MSGLEVEL endlevel);
			Logger &operator<<(std::string msg);
			Logger &operator<<(int msg);
			Logger &operator<<(float msg);
			Logger &operator<<(double msg);
			Logger &operator<<(long msg);
			Logger &operator<<(unsigned int msg);
			Logger &operator<<(unsigned long msg);
			Logger &operator<<(const unsigned char *msg);
			Logger &operator<<(const char *msg);

		private:
			char *getTimestamp();
			void printMsg(const char *head, const char *msg);
			MSGLEVEL level;
			std::FILE *fp;
			std::ostringstream buffer;
			std::mutex buffering_mutex, write_mutex;
	};
}

#endif //LOGGER_H_

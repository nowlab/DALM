#include<cstdio>
#include<cstdlib>
#include<cstring>
#include<ctime>

#include "dalm/logger.h"

using namespace DALM;

Logger::Logger(std::FILE *fp){
	this->fp = fp;
	this->level = LOGGER_DEBUG;
}

void Logger::setLevel(MSGLEVEL level){
	this->level = level;
}

void Logger::printMsg(const char *head, const char *msg){
	char *now=getTimestamp();
	size_t chlen = strlen(now);
	char *nowmsg = (char *)malloc(sizeof(char)*chlen);
	strncpy(nowmsg, now, chlen-1);
	nowmsg[chlen-1]='\0';

	std::unique_lock<std::mutex> lock(write_mutex);
	fprintf(fp, "%s:%s - %s\n", head, nowmsg, msg);
	lock.unlock();

	free(nowmsg);
}

void Logger::debug(const char *msg){
	if(level<=LOGGER_DEBUG){
		printMsg("D", msg);
	}
}

void Logger::info(const char *msg){
	if(level<=LOGGER_INFO){
		printMsg("I", msg);
	}
}

void Logger::warning(const char *msg){
	if(level<=LOGGER_WARNING){
		printMsg("W", msg);
	}
}

void Logger::error(const char *msg){
	if(level<=LOGGER_ERROR){
		printMsg("E", msg);
	}
}

void Logger::critical(const char *msg){
	if(level<=LOGGER_CRITICAL){
		printMsg("C", msg);
	}
}

char *Logger::getTimestamp(){
	time_t now;
	char *ret;

	time(&now);
	ret = ctime(&now);

	return ret;
}

Logger &Logger::operator<<(MSGLEVEL endlevel){
    std::lock_guard<std::mutex> lock(buffering_mutex);
	switch(endlevel){
	case endd:
		debug(buffer.str());
		break;
	case endi:
		info(buffer.str());
		break;
	case endw:
		warning(buffer.str());
		break;
	case ende:
		error(buffer.str());
		break;
	case endc:
		critical(buffer.str());
		break;
	default:
		critical("BUG : Logger library error.");
		break;
	}

	buffer.str("");
	buffer.clear(std::stringstream::goodbit);

	return *this;
}

Logger &Logger::operator<<(std::string msg){
    std::lock_guard<std::mutex> lock(buffering_mutex);
	buffer << msg;

	return *this;
}

Logger &Logger::operator<<(int msg){
    std::lock_guard<std::mutex> lock(buffering_mutex);
	buffer << msg;

	return *this;
}

Logger &Logger::operator<<(float msg){
    std::lock_guard<std::mutex> lock(buffering_mutex);
	buffer << msg;

	return *this;
}

Logger &Logger::operator<<(double msg){
    std::lock_guard<std::mutex> lock(buffering_mutex);
	buffer << msg;

	return *this;
}

Logger &Logger::operator<<(long msg){
    std::lock_guard<std::mutex> lock(buffering_mutex);
	buffer << msg;

	return *this;
}

Logger &Logger::operator<<(unsigned int msg){
    std::lock_guard<std::mutex> lock(buffering_mutex);
	buffer << msg;

	return *this;
}

Logger &Logger::operator<<(unsigned long msg){
    std::lock_guard<std::mutex> lock(buffering_mutex);
	buffer << msg;

	return *this;
}

Logger &Logger::operator<<(const unsigned char *msg){
    std::lock_guard<std::mutex> lock(buffering_mutex);
	buffer << msg;

	return *this;
}

Logger &Logger::operator<<(const char *msg){
    std::lock_guard<std::mutex> lock(buffering_mutex);
	buffer << msg;

	return *this;
}


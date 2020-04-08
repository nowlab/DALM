#ifndef DALM_STOPWATCH_H_
#define DALM_STOPWATCH_H_

#include <chrono>

namespace DALM {

class StopWatch {
private:
	std::chrono::high_resolution_clock::time_point start_;
public:
	StopWatch() : start_(std::chrono::high_resolution_clock::now()) {}

	auto time() const {
		return std::chrono::high_resolution_clock::now() - start_;
	}

	double sec() const {
		return std::chrono::duration<double>(time()).count();
	}

	double milli_sec() const {
		return std::chrono::duration<double, std::milli>(time()).count();
	}

	double micro_sec() const {
		return std::chrono::duration<double, std::micro>(time()).count();
	}

};

}

#endif //DALM_STOPWATCH_H_

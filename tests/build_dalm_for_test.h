#ifndef _BUILD_DALM_FOR_TEST_H
#define _BUILD_DALM_FOR_TEST_H

#include <string>
#include <../src/dalm.h>

DALM::Model *build_dalm_for_test(std::string suffix, std::string mode, DALM::Logger &logger, std::size_t n_div);

#endif //_BUILD_DALM_FOR_TEST_H
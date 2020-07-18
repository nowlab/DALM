#ifndef DALM_BUILD_DA_UTIL_H_
#define DALM_BUILD_DA_UTIL_H_

#if !defined(DALM_NEW_XCHECK) || defined(DALM_EL_SKIP)
#define FIND_BASE_ACCESS_DA
#endif

#include <cstddef>
#include <cstdint>
#include "da.h"
#include "vocabulary.h"

namespace DALM::build_da_util {

int find_base(
#ifdef FIND_BASE_ACCESS_DA
    const DAPair* da_array,
#endif
    long array_size, const VocabId* children, size_t n_children, int initial_base, const uint64_t* validate,
#ifndef DALM_NEW_XCHECK
    uint64_t words_prefix, size_t prefix_length,
#endif
    size_t& skip_counts, size_t& loop_counts);

}

#endif //DALM_BUILD_DA_UTIL_H_

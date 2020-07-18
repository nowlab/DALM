#include "dalm/build_da_util.h"

#include <cassert>
#ifdef DALM_NEW_XCHECK
#include "dalm/bit_util.h"
#endif

using namespace DALM;

namespace {

#ifdef DALM_NEW_XCHECK
inline uint64_t validate_word_from(const uint64_t* validate, size_t array_size, size_t pos) {
    auto validate_at = [&](size_t index) -> uint64_t {
        return index*64 < array_size ? validate[index] : 0ull;
    };
    auto index = pos/64;
    auto inset = pos%64;
    if (inset == 0) {
        return validate_at(index);
    } else {
        return (validate_at(index) >> inset) | (validate_at(index+1) << (64-inset));
    }
}
#endif

}

#ifndef DALM_NEW_XCHECK

int build_da_util::find_base(const DAPair* da_array, long array_size, const VocabId* children, size_t n_children, int initial_base, const uint64_t* validate, uint64_t words_prefix, size_t prefix_length, size_t& skip_counts, size_t& loop_counts) {
    auto base = initial_base;
    while (base + children[0] < array_size) {
        skip_counts++;

        assert(da_array[base + children[0]].check.check_val < 0);
        std::size_t index = base / DALM_N_BITS_UINT64_T;
        std::size_t bit = base % DALM_N_BITS_UINT64_T;
        assert(index + 1 < array_size / DALM_N_BITS_UINT64_T + 1);

        uint64_t array_prefix;
        if(bit == 0){
            array_prefix = validate[index];
        }else{
            array_prefix = (validate[index] >> bit);
            array_prefix |= ((validate[index+1] & ((0x1ULL << bit) - 1))<<(64-bit));
        }

        if((array_prefix & words_prefix) == 0){
            std::size_t k = prefix_length + 1;
            while(k < n_children){
                loop_counts++;
                int pos = base + children[k];

                if(pos < array_size && da_array[pos].check.check_val >= 0) {
                    assert(da_array[base + children[0]].check.check_val < 0);
                    base -= da_array[base + children[0]].check.check_val;
                    k = prefix_length + 1;
                    assert(base >= 0);
                    break;
                } else {
                    ++k;
                }
            }
            if(k >= n_children) break;
        }else{
            assert(da_array[base + children[0]].check.check_val < 0);
            base -= da_array[base + children[0]].check.check_val;
            assert(base >= 0);
        }
    }
    return base;
}

#else

int build_da_util::find_base(
#ifdef DALM_EL_SKIP
    const DAPair* da_array,
#endif
    long array_size, const VocabId* children, size_t n_children, int initial_base,
    const uint64_t* validate, size_t& skip_counts, size_t& loop_counts) {
    auto base = initial_base;
    while (base + children[0] < array_size) {
        skip_counts++;

        uint64_t base_mask = 0;
        for (size_t k = 0; k < n_children; k++) {
            loop_counts++;
            base_mask |= validate_word_from(validate, array_size, base + children[k]);
            if (~base_mask == 0ull)
                break;
        }
        if (~base_mask != 0ull) {
            base += bit_util::ctz(~base_mask);
            break;
        } else {
#ifndef DALM_EL_SKIP
            base += 64;
#else
/*
 *  0           64
 * |ooxo...xoox|xxxxxo...
 *  ↑        ↑ →→→→→ ↑
 *  1        2 link  3
 * 1. Current empty head
 * 2. Back empty element of 64s alignment
 * 3. Next empty head
 */
            assert(da_array[base + children[0]].check.check_val < 0);
            auto trailing_front = base + children[0]; // 1
            if (trailing_front + 64 < array_size) {
                auto trailing_insets = 63 - bit_util::clz(~validate_word_from(validate, array_size, trailing_front)); // 2-1
                assert(da_array[trailing_front + trailing_insets].check.check_val < 0);
                auto skip_distance = trailing_insets + (-da_array[trailing_front + trailing_insets].check.check_val); // 3-1
                assert(skip_distance >= 64); // The advantage to above one.
                base += skip_distance;
            } else {
                base = array_size - children[0];
            }
#endif
        }
    }
    return base;
}

#endif

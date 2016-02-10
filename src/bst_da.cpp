#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>

#include <utility>
#include <limits>
#include <cmath>

#include "dalm.h"
#include "dalm/bst_da.h"

using namespace DALM;

BstDA::BstDA(unsigned int daid, unsigned int datotal, BstDA **neighbours, Logger &logger)
        : daid_(daid), datotal_(datotal), da_(neighbours), logger_(logger){
    array_size_ = 3;
    da_array_ = new DAPair[array_size_];

    DAPair pair;
    pair.base.base_val = -1;
    pair.check.check_val = -1;

    std::fill(da_array_, da_array_ + array_size_, pair);
    bow_max_ = 0.0f;
}

BstDA::BstDA(BinaryFileReader &reader, BstDA **neighbours, unsigned char order, Logger &logger) : da_(neighbours), logger_(logger){
    reader >> daid_;
    reader >> datotal_;
    reader >> array_size_;
    da_array_ = new DAPair[array_size_];
    reader.read_many(da_array_, (std::size_t)array_size_);
    reader >> bow_max_;
    reader >> unk_;
    logger_ << "BstDA[" << daid_ << "] da-size: " << array_size_ << Logger::endi;
    context_size = (unsigned char)(order-1);
}

BstDA::~BstDA(){
    if(da_array_ != NULL) delete [] da_array_;
}

void BstDA::make_da(std::string &pathtotreefile, ValueArrayIndex *, Vocabulary &vocab) {
    unk_ = vocab.unk();

    File::TrieFileReader file(pathtotreefile);

    std::size_t unigram_type = vocab.size();
    float *values = new float[array_size_];
    std::fill(values, values + array_size_, 0.0);

    std::size_t n_validate = array_size_ / DALM_N_BITS_UINT64_T + 1;
    uint64_t *validate = new uint64_t[n_validate];
    std::fill(validate, validate + n_validate, 0);
    uint64_t words_prefix = 0;
    std::size_t prefix_length = 0;

    resize_array((int) unigram_type * 20, values, validate);

    // mark index 0 as used.
    validate[0] = 0x1ULL;
    da_array_[0].base.base_val = -1;

    std::size_t n_entries = file.size();

    logger_ << "BstDA[" << daid_ << "] total=" << n_entries << Logger::endi;
    logger_ << "BstDA[" << daid_ << "] n_unigrams=" << unigram_type << Logger::endi;

    std::size_t n_children = 0;
    VocabId *words = new VocabId[unigram_type+1];
    DAPair *children = new DAPair[unigram_type+1];
    bool *independent_rights = new bool[unigram_type+1];
    std::size_t n_slots_used = 1;

    VocabId history[DALM_MAX_ORDER];
    int n_history = 0;

    int endmarker_pos = -1;
    bool probability_node = true;
    float node_bow = -0.0f;
    int head = 1;
    int max_slot = 0;

    std::fill(history, history + DALM_MAX_ORDER, 0);
    std::size_t ten_percent = n_entries / 10;

    for(std::size_t i = 0; i < n_entries; ++i){
        if((i+1) % ten_percent == 0){
            logger_ << "BstDA[" << daid_ << "] " << (i+1) / ten_percent << "0% done." << Logger::endi;
        }

        File::ARPAEntry entry = file.next();
        assert(entry.n > 0);
        if(entry.n == 1 && entry.words[0] == 1){
            // pass
        }else if(entry.n == 2 && entry.words[0] == 1){
            if(entry.words[1] % datotal_ != daid_) continue;
        }else{
            if(entry.words[0] % datotal_ != daid_) continue;
        }

        if(n_history != entry.n-1 || memcmp(history, entry.words, sizeof(VocabId)*(entry.n-1))!=0){
            int context = 0;
            bool dummy;
            for(int j = 0; j < n_history; j++){
                assert(context >= 0);
                context = traverse(history[j], context, dummy);
            }
            assert(context >= 0);
            assert(da_array_[context].base.base_val <= -1);
            det_base(words, children, n_children, values, context, head, max_slot, endmarker_pos, node_bow,
                     validate, words_prefix, prefix_length);
            if(!probability_node){
                int base = da_array_[context].base.base_val;
                for(std::size_t j = 0; j < n_children; ++j) {
                    if (independent_rights[j]) {
                        values[base + words[j]] = std::numeric_limits<float>::infinity();
                    }
                }
            }

            assert(da_array_[context].base.base_val >= 0);
            n_slots_used += n_children;

            n_history = entry.n-1;
            std::copy(entry.words, entry.words + n_history, history);
            n_children = 0;
            endmarker_pos = -1;
            probability_node = true;
            node_bow = -0.0f;

            words_prefix = 0;
            prefix_length = 0;
        }

        words[n_children] = entry.words[entry.n-1];
        if(entry.n > 1 && entry.words[entry.n-2] == 1) {
            // probability node
            assert(probability_node);
            // should not endmarker here.
            assert(entry.words[entry.n-1] != 1);
            independent_rights[n_children] = false;
            children[n_children].base.logprob = entry.prob;
        }else if(entry.words[entry.n-1]==1){
            // end with endmarker
            endmarker_pos = (int)n_children;
            probability_node = false;
            children[n_children].base.base_val = -1;
            node_bow = entry.bow;

            if(entry.bow > bow_max_){
                bow_max_ = entry.bow;
            }
            independent_rights[n_children] = false;
        }else{
            probability_node = false;
            children[n_children].base.base_val = -1;
            BowVal bow;
            bow.bow = entry.bow;
            independent_rights[n_children] = bow.bits == 0x80000000UL;
        }
        if(words[n_children] < DALM_N_BITS_UINT64_T){
            words_prefix |= (0x1ULL << words[n_children]);
            prefix_length = (int)n_children;
        }
        ++n_children;
    }
    int context = 0;
    bool dummy;
    for(int j = 0; j < n_history; j++){
        assert(context >= 0);
        context = traverse(history[j], context, dummy);
    }
    assert(da_array_[context].base.base_val == -1);
    det_base(words, children, n_children, values, context, head, max_slot, endmarker_pos, node_bow, validate,
             words_prefix, prefix_length);
    if(!probability_node){
        int base = da_array_[context].base.base_val;
        for(std::size_t j = 0; j < n_children; ++j) {
            if (independent_rights[j]) {
                values[base + words[j]] = std::numeric_limits<float>::infinity();
            }
        }
    }
    n_slots_used += n_children;

    bow_max_ += 1.0; // confirm to negative.
    replace_value(values);

    fit_to_size(max_slot);

    delete [] validate;
    delete [] values;
    delete [] words;
    delete [] children;
    delete [] independent_rights;

    logger_ << "BstDA[" << daid_ << "] da-size: " << array_size_ << " n_slots_used: " << n_slots_used << Logger::endi;
}

void BstDA::fill_inserted_node(std::string &path, Vocabulary &vocab){
    File::TrieFileReader file(path);

    size_t n_entries = file.size();

    size_t tenpercent = n_entries / 10;
    int context = 0;
    bool ignored;

    for(size_t i = 0; i < n_entries; i++){
        if((i+1) % tenpercent == 0){
            logger_ << "BstDA[" << daid_ << "][fill] " << (i+1)/tenpercent << "0% done." << Logger::endi;
        }

        File::ARPAEntry entry = file.next();

        if(entry.n == 1) continue;
        if(entry.words[entry.n-1] == 1) continue;
        if(entry.words[entry.n-2] != 1) continue;

        if(entry.n == 2) continue;

        if(entry.words[0] % datotal_ != daid_) continue;

        if(std::isinf(entry.prob)){
            context=0;
            for(std::size_t j = 0; j + 3 < entry.n; ++j){
                context = traverse(entry.words[j], context, ignored);
            }
            float bow;
            float prev_prob;
            bool independent_left;
            int endmarker = endmarker_node(context, bow);
            if(target_node(endmarker, entry.words[entry.n - 1], prev_prob, independent_left)){
                context = traverse(entry.words[entry.n - 3], context, ignored);
                assert(context > 0);
                endmarker = endmarker_node(context, bow);
                int prob_node = da_array_[endmarker].base.base_val + entry.words[entry.n-1];
                assert(prob_node != 1);
                da_array_[prob_node].base.logprob = prev_prob + bow;
            }else{
                throw std::runtime_error("trie corruption");
            }
        }
    }
}

void BstDA::dump(BinaryFileWriter &writer) {
    writer << daid_;
    writer << datotal_;
    writer << array_size_;
    writer.write_many(da_array_, (std::size_t) array_size_);
    writer << bow_max_;
    writer << unk_;
}

void BstDA::write_and_free(BinaryFileWriter &writer){
    writer.write_many(da_array_, (std::size_t) array_size_);
    if(da_array_ != NULL) delete [] da_array_;
    da_array_ = NULL;
}

void BstDA::reload(BinaryFileReader &reader){
    da_array_ = new DAPair[array_size_];
    reader.read_many(da_array_, (std::size_t) array_size_);
}

bool BstDA::traverse_query(VocabId word, Traverse &t) {
    int to = word + t.base();
    //std::cerr << "base=" << base << " word=" << word << " to=" << to << std::endl;
    if(to >= array_size_){
        return false;
    }else if(da_array_[to].check.check_val == t.index){
        t.base_and_independent_right.i = da_array_[to].base.base_val;
        t.index = to;
        return true;
    }else{
        return false;
    }
}

bool BstDA::get_unigram_prob(VocabId word, Target &target) {
    BstDA *da = da_[word % datotal_];
    Traverse t(da, 0);
    return da->traverse_to_target(word, t, target);
}

bool BstDA::traverse_to_target(VocabId word, const Traverse &t, Target &target) {
    if(t.index < 0){
        // emulate KenLM
        target.bow_ = -bow_max_;
        return false;
    }

    assert(t.index < array_size_);
    int endmarker_node = 1 + t.base();
    assert(endmarker_node < array_size_);
    target.bow_ = da_array_[endmarker_node].check.logbow;

    int endmarker_base = da_array_[endmarker_node].base.base_val;
    assert(endmarker_base > 0);
    target.index = endmarker_base + word;
    int check = da_array_[target.index].check.check_val;
    if(endmarker_node == check) {
        target.prob_and_independent_left.f = da_array_[target.index].base.logprob;
        return true;
    }else{
        return false;
    }
}

float BstDA::get_prob(VocabId *word, unsigned char order){
    float bow = 0.0;
    VocabId target_word = word[0];
    Traverse t(this, 0);
    Target target;
    for(unsigned int i = 1; i < order; i++){
        if(traverse_query(word[i], t)){
            if(traverse_to_target(target_word, t, target)){
                bow = 0.0;
            }else{
                bow += target.bow(bow_max_);
            }
        }else{
            break;
        }
    }

    if(target.invalid()){
        // case for backoff to unigrams.
        get_unigram_prob(target_word, target);
    }
    return target.prob() + bow;
}

void debug_print_return(int id, int daid, VocabId word, float prob, float bow, State const &state, bool independent_left, bool extend){
    std::cerr << "[" << id << "] word=" << word << " prob=" << prob << " bow=" << bow;
    std::cerr << " independent_left=" << (independent_left?"T":"F") << " extend=" << (extend?"T":"F") << " State=[ ";
    for(int i = 0; i < state.get_count(); ++i){
        std::cerr << state.get_word(i) << " ";
    }
    std::cerr << "]" << std::endl;
}

float BstDA::get_prob(VocabId word, State &state){
    float bow = 0.0f;
    int count = state.get_count();
    Traverse t;
    Target target;
    //debug_print_return(-1, daid_, word, target.prob(), bow, state, target.independent_left(), false);

    for(int i = count - 1; i >= 0; --i){
        int pos = state.get_pos(i);
        t.init(this, pos);

        if(traverse_to_target(word, t, target)){
            break;
        }else{
            bow += target.bow(bow_max_);
            //independent_left = true;
        }
    }

    if(target.invalid()){
        // case for backoff to unigrams.
        if(!get_unigram_prob(word, target)){
            //independent_left = true;

            // emulate KenLM
            state.set_count(1);
            state.set_pos(0, -1);
            state.set_word(0, word);

            //debug_print_return(1, daid_, word, target.prob(), bow, state, target.independent_left(), false);
            return target.prob() + bow;
        }
    }

    // calculate state for next query.
    state.push_word(word);
    count = std::min<unsigned char>((unsigned char)(count+1), context_size);
    state.set_count(0);

    BstDA *da = da_[word % datotal_];
    t.init(da);
    for(unsigned char i = 0; i < count; ++i){
        if(da->traverse_query(state.get_word(i), t)){
            state.set_pos(i, t.index);
            if(!t.independent_right()){
                state.set_count((unsigned char)(i+1));
            }
        }else{
            break;
        }
    }

    //debug_print_return(1, daid_, word, target.prob(), bow, state, target.independent_left(), false);
    return target.prob() + bow;
}

float BstDA::get_prob(VocabId word, State &state, Fragment &fnew){
    float bow = 0.0;
    int count = state.get_count();
    bool independent_left=false;
    //debug_print_return(-2, daid_, word, 0.0, bow, state, independent_left, false);
    fnew.status.word = word;
    Traverse t;
    Target target;

    for(int i = count - 1; i >= 0; --i){
        int pos = state.get_pos(i);
        t.init(this, pos);

        if(traverse_to_target(word, t, target)){
            independent_left = independent_left || target.independent_left();
            break;
        }else{
            bow += target.bow(bow_max_);
            independent_left = true;
        }
    }
    if(target.invalid()){
        // case for backoff to unigrams.
        if(!get_unigram_prob(word, target)){
            //independent_left = true;

            // emulate KenLM
            state.set_count(1);
            state.set_pos(0, -1);
            state.set_word(0, word);

            fnew.status.pos = 0;
            //debug_print_return(2, daid_, word, target.prob(), bow, state, true, false);
            return -(target.prob() + bow);
        }else{
            independent_left = independent_left || target.independent_left();
        }
    }

    assert(target.index != 0);

    // calculate state for next query.
    state.push_word(word);
    count = std::min<unsigned char>((unsigned char)(count+1), context_size);
    state.set_count(0);

    BstDA *da = da_[word % datotal_];
    t.init(da);
    fnew.status.pos = 0;

    for(unsigned char i = 0; i < count; ++i){
        if(da->traverse_query(state.get_word(i), t)){
            fnew.status.pos = t.index;
            state.set_pos(i, t.index);
            if(!t.independent_right()){
                state.set_count((unsigned char)(i+1));
            }
        }else{
            fnew.status.pos = -target.index;
            break;
        }
    }

    //debug_print_return(2, daid_, word, target.prob(), bow, state, independent_left, false);
    float prob = target.prob() + bow;
    assert(prob != 0.0);
    return independent_left?-prob:prob;
}

float BstDA::get_prob(const Fragment &fprev, State &state, Gap &gap){
    unsigned char depth = gap.get_gap();
    unsigned char count = state.get_count();
    bool &independent_left = gap.is_finalized();
    bool &extended = gap.is_extended();
    //debug_print_return(-3, daid_, fprev.status.word, 0, 0, state, false, false);
    //std::cerr << "count=" << (int) count << " depth=" << (int)depth << std::endl;

    assert(count > depth);
    extended = false;
    independent_left = false;

    float bow = 0.0;
    Traverse t;
    Target target_prev;

    if(depth==0){
        if(fprev.status.word!=0){
            if(!get_unigram_prob(fprev.status.word, target_prev)){
                throw std::runtime_error("Unigram not found");
            }
        }
    }else{
        int pos = state.get_pos(depth - 1);
        assert(pos >= 0);

        t.init(this, pos);
        if(traverse_to_target(fprev.status.word, t, target_prev)){
            independent_left = independent_left || target_prev.independent_left();
        }else{
            independent_left = true;
        }
    }

    Target target = target_prev;
    for(int i = count - 1; i >= depth; --i){
        int pos = state.get_pos(i);
        t.init(this, pos);
        if(traverse_to_target(fprev.status.word, t, target)){
            independent_left = independent_left || target.independent_left();
            extended = true;
            break;
        }else{
            bow += target.bow(bow_max_);
            independent_left = true;
        }
    }

    // calculate state for next query.
    int pos = fprev.status.pos;
    if(pos <= 0){
        state.set_count(0);
        //debug_print_return(3, daid_, fprev.status.word, target.prob(), bow, state, independent_left, extended);
        return target.prob() + bow - target_prev.prob();
    }

    state.push_word(fprev.status.word);
    count = std::min<unsigned char>((unsigned char)(count+1), context_size);
    state.set_count((unsigned char)(depth+1));

    BstDA *da = da_[fprev.status.word % datotal_];
    t.init(da, pos);
    state.set_pos(depth, pos);
    for(int i = depth+1; i < count; ++i){
        if(da->traverse_query(state.get_word(i), t)){
            state.set_pos(i, t.index);
            if(!t.independent_right()){
                state.set_count((unsigned char)(i+1));
            }
        }else{
            break;
        }
    }
    //debug_print_return(3, daid_, fprev.status.word, target.prob(), bow, state, independent_left, extended);
    return target.prob() + bow - target_prev.prob();
}

float BstDA::get_prob(const Fragment &fprev, State &state, Gap &gap, Fragment &fnew){
    unsigned char depth = gap.get_gap();
    unsigned char count = state.get_count();
    bool &independent_left = gap.is_finalized();
    bool &extended = gap.is_extended();
    //debug_print_return(-4, daid_, fprev.status.word, 0, 0, state, false, false);
    //std::cerr << "count=" << (int) count << " depth=" << (int)depth << std::endl;

    assert(count > depth);
    extended = false;
    independent_left = false;
    fnew = fprev;

    float bow = 0.0;
    Traverse t(this, 0);
    Target target_prev;

    if(depth==0){
        if(fprev.status.word!=0) {
            if(!get_unigram_prob(fprev.status.word, target_prev)){
                throw std::runtime_error("Unigram not found");
            }
        }
    }else{
        int pos = state.get_pos(depth-1);
        assert(pos >= 0);

        t.init(this, pos);
        if(traverse_to_target(fprev.status.word, t, target_prev)){
            independent_left = independent_left || target_prev.independent_left();
        }else{
            independent_left = true;
        }
    }

    Target target = target_prev;
    for(int i = count - 1; i >= depth; --i){
        int pos = state.get_pos(i);
        t.init(this, pos);
        if(traverse_to_target(fprev.status.word, t, target)){
            independent_left = independent_left || target.independent_left();
            extended = true;
            break;
        }else{
            bow += target.bow(bow_max_);
            independent_left = true;
        }
    }

    // calculate state for next query.
    int pos = fprev.status.pos;
    if(pos <= 0){
        fnew.status.pos = -target.index;
        state.set_count(0);
        //debug_print_return(4, daid_, fprev.status.word, target.prob(), bow, state, independent_left, extended);
        return target.prob() + bow - target_prev.prob();
    }

    state.push_word(fprev.status.word);
    count = std::min<unsigned char>((unsigned char)(count+1), context_size);
    state.set_count((unsigned char)(depth+1));

    BstDA *da = da_[fprev.status.word % datotal_];
    t.init(da, pos);
    state.set_pos(depth, pos);
    for(int i = depth+1; i < count; ++i){
        if(da->traverse_query(state.get_word(i), t)){
            fnew.status.pos = t.index;
            state.set_pos(i, t.index);
            if(!t.independent_right()){
                state.set_count((unsigned char)(i+1));
            }
        }else{
            fnew.status.pos = -target.index;
            break;
        }
    }
    //std::cerr << "context_out=" << fnew.status.pos << std::endl;
    //debug_print_return(4, daid_, fprev.status.word, target.prob(), bow, state, independent_left, extended);
    return target.prob() + bow - target_prev.prob();
}

void BstDA::init_state(VocabId *word, unsigned char order, State &state){
    /*
    std::cerr << "! ";
    for(int i = 0; i < order; ++i){
        std::cerr << word[i] << " ";
    }
    std::cerr << std::endl;
    debug_print_return(-5, daid_, 0, 0, 0, state, false, false);
    */
    state.set_count(0);
    Traverse t(this, 0);
    for(int i = 0; i < order; i++){
        state.set_word(i, word[i]);
        if(traverse_query(word[i], t)){
            state.set_pos(i, t.index);
            if (!t.independent_right()) {
                state.set_count((unsigned char) (i + 1));
            }
        }else if(i == 0){
            // Emulate KenLM
            state.set_count(1);
            state.set_pos(0, -1);
        }else{
            break;
        }
    }
    //debug_print_return(5, daid_, 0, 0, 0, state, false, false);
}

void BstDA::init_state(State &state, const State &state_prev, const Fragment *, const Gap &gap){
    //debug_print_return(-6, daid_, 0, 0, 0, state, false, false);
    //debug_print_return(-6, daid_, 0, 0, 0, state_prev, false, false);
    //std::cerr << "gap.get_gap()=" << (int)gap.get_gap() << std::endl;
    assert(state.get_count() > gap.get_gap());
    assert(state_prev.get_count() > 0);

    for(int i = 0; i < state_prev.get_count(); ++i){
        assert(state.get_word(i) == state_prev.get_word(i));
        state.set_pos(i, state_prev.get_pos(i));
    }

    int context = state_prev.get_pos(state_prev.get_count()-1);
    Traverse t(this, context);

    for(int i = state_prev.get_count(); i < gap.get_gap(); ++i){
        if(traverse_query(state.get_word(i), t)){
            state.set_pos(i, t.index);
        }else{
            std::cerr << "Prev: [ ";
            for(int j = 0; j < state_prev.get_count(); ++j){
                std::cerr << state_prev.get_word(j) << " ";
            }
            std::cerr << "]" << std::endl;
            std::cerr << "Curr: [ ";
            for(int j = 0; j < state.get_count(); ++j){
                std::cerr << state.get_word(j) << " ";
            }
            std::cerr << "]" << std::endl;
            throw std::runtime_error("[1] init failed");
        }
    }
#ifndef NDEBUG
    for(int i = gap.get_gap(); i < state.get_count(); ++i){
        if(traverse_query(state.get_word(i), t)){
            assert(state.get_pos(i) == t.index);
        }else{
            throw std::runtime_error("[2] init failed");
        }
    }
#endif
    //debug_print_return(6, daid_, 0, 0, 0, state, false, false);
}

/* depricated */
unsigned long int BstDA::get_state(VocabId *, unsigned char){
    throw std::runtime_error("unsupported");
}

bool BstDA::checkinput(unsigned short n,unsigned int *ngram,float bow,float prob,bool bow_presence){
    float limit = 0.0001;
    int context = 0;
    int next;
    bool independent_right;
    if(bow_presence){
        int bowid=((n>1)?ngram[n-1]:ngram[0]) % datotal_;
        BstDA *bow_da = da_[bowid];
        for(int i = 0; i < n; ++i){
            next = bow_da->traverse(ngram[n - i - 1], context, independent_right);
            if(next > 0){
                context = next;
            }else{
                logger_ << "[BstDa][bow] not found." << Logger::ende;
                return false;
            }
        }
        float bow_test;
        bow_da->endmarker_node(context, bow_test);
        if(std::abs(bow_test - bow) >= limit){
            logger_ << "[BstDA][bow] check failed. expected=" << bow << " but value=" << bow_test << Logger::endi;
            return false;
        }
    }

    context = 0;
    for(int i = 0; i + 1 < n; ++i){
        next = traverse(ngram[n - i - 2], context, independent_right);
        if(next > 0){
            context = next;
        }else{
            logger_ << "[BstDA][prob] not found." << Logger::ende;
            return false;
        }
    }
    float bow_test;
    float prob_test;
    bool il;
    int endmarker = endmarker_node(context, bow_test);
    if(target_node(endmarker, ngram[n - 1], prob_test, il)){
        if(prob_test != prob){
            logger_ << "[BstDA][prob] check failed. expected=" << prob << " but value=" << prob_test << Logger::endi;
            return false;

        }
    }
    return true;
}

void BstDA::replace_value(float *values) {
    for(int i=0; i < array_size_; ++i){
        bool endmarker_slot = false;
        if(da_array_[i].check.check_val >= 0) {
            int base = da_array_[da_array_[i].check.check_val].base.base_val;
            if (base < 0) base = -base;
            if(base + 1 == i){
                // endmarker slot
                assert(values[i] < bow_max_);
                da_array_[i].check.logbow = values[i] - bow_max_;
                endmarker_slot = true;
            }
        }

        if((!endmarker_slot) && values[i]==std::numeric_limits<float>::infinity()){
            da_array_[i].base.base_val = - da_array_[i].base.base_val;
        }
    }
}

void BstDA::det_base(VocabId *words, DAPair *children, std::size_t n_children, float *&values, int context, int &head,
                     int &max_slot, int endmarker_pos, float node_bow, uint64_t *&validate, uint64_t words_prefix,
                     std::size_t prefix_length) {

    int base = head - words[0];
    while(base <= 0){
        base-= da_array_[base + words[0]].check.check_val;
    }

    std::size_t k = prefix_length + 1;
    while(true){
        std::size_t index = base / DALM_N_BITS_UINT64_T;
        std::size_t bit = base % DALM_N_BITS_UINT64_T;
        assert(index + 1 < array_size_ / DALM_N_BITS_UINT64_T + 1);

        uint64_t array_prefix;
        if(bit == 0){
            array_prefix = validate[index];
        }else{
            array_prefix = (validate[index] >> bit);
            array_prefix |= ((validate[index+1] & ((0x1ULL << bit) - 1))<<(DALM_N_BITS_UINT64_T-bit));
        }

        if((array_prefix & words_prefix) == 0){
            while(k < n_children){
                int pos = base + words[k];

                if(pos < array_size_ && da_array_[pos].check.check_val >= 0){
                    assert(da_array_[base + words[0]].check.check_val < 0);
                    base -= da_array_[base + words[0]].check.check_val;
                    k = prefix_length + 1;
                    assert(base >= 0);
                    break;
                }else{
                    ++k;
                }
            }
            if(k >= n_children) break;
        }else{
            assert(da_array_[base + words[0]].check.check_val < 0);
            base -= da_array_[base + words[0]].check.check_val;
            assert(base >= 0);
        }

        if(base + words[0] >= array_size_){
            break;
        }
        assert(da_array_[base + words[0]].check.check_val < 0);
    }

    da_array_[context].base.base_val = base;

    int max_jump = base + words[n_children-1] + 1;
    if(max_jump >= array_size_){
        resize_array(array_size_ * 2 > max_jump ? array_size_ * 2 : max_jump * 2, values, validate);
    }

    for(std::size_t i = 0; i < n_children; ++i){
        int pos = base + words[i];
        assert(da_array_[pos].base.base_val < 0);
        assert(da_array_[pos].check.check_val < 0);
        int next_index = pos - da_array_[pos].check.check_val;

        if(pos==head){
            da_array_[next_index].base.base_val = std::numeric_limits<int>::min();
            head -= da_array_[head].check.check_val;
        }else{
            int pre_index = pos + da_array_[pos].base.base_val;

            da_array_[next_index].base.base_val += da_array_[pos].base.base_val;
            da_array_[pre_index].check.check_val += da_array_[pos].check.check_val;
        }

        children[i].check.check_val = context;
        da_array_[pos] = children[i];

        std::size_t index = pos / DALM_N_BITS_UINT64_T;
        std::size_t bit = pos % DALM_N_BITS_UINT64_T;
        validate[index] |= (0x1ULL << bit);
    }
    if(endmarker_pos >= 0){
        values[base + words[endmarker_pos]] = node_bow;
    }

    if(max_slot < (int)(base + words[n_children - 1])){
        max_slot = base + words[n_children - 1];
    }
}

int BstDA::traverse(VocabId word, int from, bool &independent_right) {
    int base = da_array_[from].base.base_val;
    if(base < 0) base = -base;
    int to = word + base;
    //std::cerr << "base=" << base << " word=" << word << " to=" << to << std::endl;
    if(to > array_size_){
        return -1;
    }else if(da_array_[to].check.check_val == from){
        independent_right = da_array_[to].base.base_val < 0;
        return to;
    }else{
        return -1;
    }
}

int BstDA::endmarker_node(int from, float &bow) {
    if(from >= array_size_){
        bow = 0.0f;
        return (int)array_size_; // emulate KenLM
    }
    assert(from < array_size_);
    int base = da_array_[from].base.base_val;
    if(base < 0) base = -base;
    int to = 1 + base;
    assert(to < array_size_);
    bow = da_array_[to].check.logbow + bow_max_;
    return to;
}

int BstDA::target_node(int from, VocabId word, float &prob, bool &independent_left) {
    if(word == unk_) return 0;
    else if(from >= array_size_) return 0; //emulate KenLM

    int base = da_array_[from].base.base_val;
    assert(base > 0);
    int to = base + word;
    int check = da_array_[to].check.check_val;
    if(from == check) {
        if (da_array_[to].base.logprob > 0) {
            independent_left = true;
            prob = -da_array_[to].base.logprob;
        } else {
            independent_left = false;
            prob = da_array_[to].base.logprob;
        }
        return to;
    }else{
        return 0;
    }
}

void BstDA::resize_array(long size, float *&values, uint64_t *&validate) {
    if(size > std::numeric_limits<int>::max()){
        throw std::runtime_error("array size overflow");
    }
    DAPair *temp_array = new DAPair[size];

    std::copy(da_array_, da_array_ + array_size_, temp_array);
    delete [] da_array_;
    da_array_ = temp_array;

    float *temp = new float[size];
    std::copy(values, values + array_size_, temp);
    delete [] values;
    values = temp;

    std::size_t n_validate_new = size / DALM_N_BITS_UINT64_T + 1;
    std::size_t n_validate_old = array_size_ / DALM_N_BITS_UINT64_T + 1;
    uint64_t *replacing_validate = new uint64_t[n_validate_new];
    std::copy(validate, validate + n_validate_old, replacing_validate);

    delete [] validate;
    validate = replacing_validate;
    DAPair dummy;
    dummy.base.base_val = -1;
    dummy.check.check_val = -1;

    std::fill(da_array_ + array_size_, da_array_ + size, dummy);
    std::fill(values + array_size_, values+size, 0.0);
    std::fill(validate + n_validate_old, validate + n_validate_new, 0);

    array_size_ = size;
}

void BstDA::fit_to_size(int max_slot) {
    array_size_ = max_slot + 1;
    DAPair *tmp_array = new DAPair[array_size_];
    std::copy(da_array_, da_array_ + array_size_, tmp_array);
    delete [] da_array_;
    da_array_ = tmp_array;
}

float BstDA::sum_bows(State &state, unsigned char begin, unsigned char end) {
    int context = 0;
    float bow_state;
    float bow = 0.0;
    for(int i = begin; i < end; i++){
        context = state.get_pos(i);
        endmarker_node(context, bow_state);
        bow += bow_state;
    }
    return bow;
}

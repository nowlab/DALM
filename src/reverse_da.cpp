#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>

#include <utility>
#include <limits>
#include <cmath>

#include "dalm.h"
#include "dalm/reverse_da.h"
#include "dalm/build_da_util.h"
#include "dalm/stopwatch.h"

using namespace DALM;

ReverseDA::ReverseDA(unsigned char daid, unsigned char datotal, ReverseDA **neighbours, Logger &logger)
    : daid_(daid), datotal_(datotal), da_(neighbours), logger_(logger){
    array_size_ =3;
    bow_max_ = 0.0f;
    da_array_ = new DAPair[array_size_];

    DAPair dummy;
    dummy.base.base_val = -1;
    dummy.check.check_val = -1;
    std::fill(da_array_, da_array_ + array_size_, dummy);
}

ReverseDA::ReverseDA(BinaryFileReader &reader, ReverseDA **neighbours, unsigned char order, Logger &logger): logger_(logger){
    DA::context_size = (unsigned char)(order-1);
    reader >> daid_;
    reader >> datotal_;

    da_ = neighbours;
    reader >> array_size_;
    reader >> bow_max_;

    da_array_ = new DAPair[array_size_];
    reader.read_many(da_array_, (std::size_t) array_size_);

    logger << "ReverseDA[" << daid_ << "] da-size: " << array_size_ << Logger::endi;
}

ReverseDA::~ReverseDA(){
    if(da_array_ != NULL) delete [] da_array_;
}

void ReverseDA::fill_inserted_node(std::string &pathtotreefile, Vocabulary &vocab){
    File::TrieFileReader file(pathtotreefile);

    std::size_t n_entries = file.size();
    std::size_t tenpercent = n_entries / 10;
    for(size_t i = 0; i < n_entries; i++){
        if((i+1) % tenpercent == 0){
            sprintf(log_buffer_, "ReverseDA[%u][fill] %lu0%% done.", daid_, (i+1)/tenpercent);
            logger_ << log_buffer_ << Logger::endi;
        }

        File::ARPAEntry entry = file.next();
        if(entry.words[0] % datotal_ != daid_){
            continue;
        }

        if(entry.n >= 2 && entry.words[entry.n-1] == 1 && std::isinf(entry.prob)){
            int context = 0;
            for(std::size_t j = 0; j + 2 < entry.n; ++j){
                context = traverse(entry.words[j], context);
                assert(da_array_[context].base.base_val >= 0);
            }
            float prev_prob = da_array_[endmarker_node(context)].check.logprob;
            assert(!std::isinf(prev_prob));

            // get bow;
            ReverseDA *bow_da = da_[entry.words[1] % datotal_];
            int bow_context = 0;
            for(std::size_t j = 1; j + 1 < entry.n; ++j){
                bow_context = bow_da->traverse(entry.words[j], bow_context);
            }
            float prev_bow;
            if(bow_da->da_array_[bow_context].base.base_val < 0) {
                prev_bow = -0.0f;
            }else{
                int bow_endmarker = bow_da->endmarker_node(bow_context);
                prev_bow = bow_da->da_array_[bow_endmarker].base.logbow;
                if(std::isinf(prev_bow)){
                    prev_bow = -0.0f;
                }else if(prev_bow > 0.0) {
                    prev_bow = -prev_bow + bow_da->bow_max_;
                }else{
                    prev_bow += bow_da->bow_max_;
                }
            }

            // get target context
            context = traverse(entry.words[entry.n-2], context);
            assert(da_array_[context].base.base_val >= 0);
            da_array_[endmarker_node(context)].check.logprob = prev_prob + prev_bow;
            assert(da_array_[endmarker_node(context)].check.logprob < 0.0f);
        }
    }
}

void ReverseDA::replace_value(float *&values) {
    for(int i = 0; i < array_size_; i++){
        if(values[i] != std::numeric_limits<float>::infinity()){
            // endmarker node
            if(da_array_[i].base.logbow == std::numeric_limits<float>::infinity()){
                // independent-right
                if(values[i] > 0){
                    //independent-left
                    da_array_[i].check.logprob = -values[i];
                }else{
                    da_array_[i].check.logprob = values[i];
                    da_array_[i].base.logbow = -std::numeric_limits<float>::infinity();
                }
            }else{
                da_array_[i].base.logbow -= bow_max_;
                if(values[i] > 0){
                    //independent-left
                    da_array_[i].check.logprob = -values[i];
                    da_array_[i].base.logbow = -da_array_[i].base.logbow;
                }else{
                    da_array_[i].check.logprob = values[i];
                }
                assert(!std::isnan(da_array_[i].base.logbow));
            }
        }
    }
}

void ReverseDA::make_da(std::string &pathtotreefile, ValueArrayIndex *, Vocabulary &vocab) {
    File::TrieFileReader file(pathtotreefile);

    std::size_t n_unigram = vocab.size();
    float *values = new float[array_size_];
    std::fill(values, values + array_size_, std::numeric_limits<float>::infinity());

    std::size_t n_validate = array_size_ / DALM_N_BITS_UINT64_T + 1;
    uint64_t *validate = new uint64_t[n_validate];
    std::fill(validate, validate + n_validate, 0);
    uint64_t words_prefix = 0;
    std::size_t prefix_length = 0;

    resize_array((int) n_unigram * 20, values, validate);

    // mark index 0 as used.
    validate[0] = 0x1ULL;
    da_array_[0].base.base_val = 0;

    VocabId *children = new VocabId[n_unigram + 1];
    VocabId *history = new VocabId[DALM_MAX_ORDER];
    std::fill(history, history + DALM_MAX_ORDER, 0);


#ifndef NDEBUG
    bool has_endmarker = false;
#endif
    float node_prob = 0.0f;
    Base node_bow;
    node_bow.logbow = -0.0f;

    std::size_t n_children = 0;
    std::size_t n_history = 0;
    std::size_t n_slot_used = 1;
    int head = 1;
    int max_slot = 0;

    std::size_t n_entries = file.size();
    sprintf(log_buffer_, "ReverseDA[%u] n_entries=%lu", daid_, n_entries);
    logger_ << log_buffer_ << Logger::endi;
    sprintf(log_buffer_, "ReverseDA[%u] n_unigram=%lu", daid_, n_unigram);
    logger_ << log_buffer_ << Logger::endi;

    StopWatch sw;

    std::size_t tenpercent = n_entries / 10;
    for(size_t i = 0; i < n_entries; i++){
        if((i+1) % tenpercent == 0){
            sprintf(log_buffer_, "ReverseDA[%u] %lu0%% done. %.3lf sec. %lu s checked, %lu s skipped.", daid_, (i+1)/tenpercent, sw.sec(), loop_counts_, skip_counts_);
            logger_ << log_buffer_ << Logger::endi;
        }

        File::ARPAEntry entry = file.next();
        if(entry.words[0] % datotal_ != daid_){
            continue;
        }

        if(n_history + 1 != entry.n || memcmp(history, entry.words, sizeof(VocabId)*(entry.n - 1)) != 0){
            int context = 0;
            for(std::size_t j = 0; j < n_history; j++){
                context = traverse(history[j], context);
            }
            assert(n_history == 0 || has_endmarker);
            det_base(children, node_prob, node_bow, n_children, context, values, n_slot_used, head, max_slot, validate,
                     words_prefix, prefix_length);
            if(node_bow.logbow > bow_max_){
                bow_max_ = node_bow.logbow;
            }

            n_history = (std::size_t)entry.n - 1;
            std::copy(entry.words, entry.words+n_history, history);
            n_children = 0;
            node_prob = 0.0f;
            node_bow.logbow = -0.0f;
            words_prefix = 0;
            prefix_length = 0;
#ifndef NDEBUG
            has_endmarker = false;
#endif
        }

        children[n_children] = entry.words[entry.n-1];
        if(children[n_children] == 1){ // <#>
            node_prob = entry.prob;
            node_bow.logbow = entry.bow;
#ifndef NDEBUG
            has_endmarker=true;
#endif
        }
        if(children[n_children] < DALM_N_BITS_UINT64_T){
            words_prefix |= (0x1ULL << children[n_children]);
            prefix_length = (int)n_children;
        }
        ++n_children;
    }

    int context = 0;
    for(std::size_t j = 0; j < n_history; j++){
        context = traverse(history[j], context);
    }
    assert(n_history==0 || has_endmarker);
    det_base(children, node_prob, node_bow, n_children, context, values, n_slot_used, head, max_slot, validate,
             words_prefix, prefix_length);
    if(node_bow.logbow > bow_max_){
        bow_max_ = node_bow.logbow;
    }
    bow_max_ += 1.0; // ensure negative;

    replace_value(values);
    fit_to_size(max_slot);

    delete [] validate;
    delete [] history;
    delete [] children;
    delete [] values;

    sprintf(log_buffer_, "ReverseDA[%u] array_size=%ld n_slot_used=%lu", daid_, array_size_, n_slot_used);
    logger_ << log_buffer_ << Logger::endi;
    sprintf(log_buffer_, "ReverseDA[%u] Total construction time=%lf seconds.", daid_, sw.sec());
    logger_ << log_buffer_ << Logger::endi;

    {
        std::vector<size_t> cnt_table_log_;
        for (auto [n, c] : children_cnt_table_) {
            auto log_n = 0;
            while ((1ull<<log_n) < n)
                log_n++;
            if (cnt_table_log_.size() <= log_n)
                cnt_table_log_.resize(log_n+1);
            cnt_table_log_[log_n] += c;
        }
        logger_ << "  Count children table" << Logger::endi;
        for (size_t i = 0; i < cnt_table_log_.size(); i++) {
            logger_ << "<= 2^" << i << "] " << cnt_table_log_[i] << Logger::endi;
        }
    }
    {
        std::vector<double> fbtime_table_log_;
        for (auto [n, c] : children_fb_time_table_) {
            auto log_n = 0;
            while ((1ull<<log_n) < n)
                log_n++;
            if (fbtime_table_log_.size() <= log_n)
                fbtime_table_log_.resize(log_n+1);
            fbtime_table_log_[log_n] += c;
        }
        logger_ << "  FindBase time table" << Logger::endi;
        for (size_t i = 0; i < fbtime_table_log_.size(); i++) {
            logger_ << "<= 2^" << i << "] " << fbtime_table_log_[i] << Logger::endi;
        }
    }

}

void ReverseDA::fit_to_size(int max_slot) {
    array_size_ = max_slot + 1;
    DAPair *replacing_array = new DAPair[array_size_];
    std::copy(da_array_, da_array_ + array_size_, replacing_array);
    delete [] da_array_;
    da_array_ = replacing_array;
}

void ReverseDA::dump(BinaryFileWriter &writer) {
    writer << daid_;
    writer << datotal_;
    writer << array_size_;
    writer << bow_max_;
    writer.write_many(da_array_, (std::size_t) array_size_);

    logger_ << "ReverseDA[" << daid_ << "] da-size: " << array_size_ << Logger::endi;
}

void ReverseDA::export_to_text(TextFileWriter &writer) {
    writer << daid_ << "\n";
    writer << datotal_ << "\n";
    writer << array_size_ << "\n";
    writer << bow_max_ << "\n\n";

    for(int i = 0; i < array_size_; ++i){
        int base = da_array_[i].base.base_val;
        int check = da_array_[i].check.check_val;

        if(check < 0){
            float prob = da_array_[i].check.logprob;
            float bow = da_array_[i].base.logbow;
            writer << bow << "\t" << prob << "\n";
        }else{
            if(base < 0){
                float prob = da_array_[i].base.logprob;
                writer << prob << "\t" << check << "\n";
            }else{
                writer << base << "\t" << check << "\n";
            }
        }
    }
    writer << "\n";
}

void ReverseDA::write_and_free(BinaryFileWriter &writer){
    writer.write_many(da_array_, (std::size_t) array_size_);
    delete [] da_array_;
}

void ReverseDA::reload(BinaryFileReader &reader){
    da_array_ = new DAPair[array_size_];
    reader.read_many(da_array_, (std::size_t) array_size_);
}

float ReverseDA::get_prob(VocabId *word, unsigned char order){
    float bow = 0.0f;

    int level = 0;
    Traverse t(this, 0);
    Target target;
    for(unsigned char depth=0; depth < order; ++depth){
        if(jump(t, word[depth])){
            level = depth + 1;
            target.set(this, t);
            if(target.independent_left()){
                break;
            }
        }else{
            break;
        }
    }

    if(order > 1){
        ReverseDA *da = da_[word[1] % datotal_];
        t.init(da, 0);

        Target bow_target;
        for(unsigned char hist = 1; hist < order; ++hist){
            if(t.base() >= 0 && da->jump(t, word[hist])){
                bow_target.set(da, t);
                if(!bow_target.independent_right()){
                    if(hist >= level){
                        bow += bow_target.bow(da->bow_max_);
                    }
                }
            }else{
                break;
            }
        }
    }

    return target.prob + bow;
}

bool ReverseDA::jump(Traverse &t, VocabId word) {
    int next = t.base() + word;
    if(next >= array_size_) return false;
    if(da_array_[next].check.check_val == t.index){
        t.init(this, next);
        return true;
    }else{
        return false;
    }
}

void debug_print_return_rev(int id, int daid, VocabId word, float prob, float bow, State const &state, bool independent_left, bool extend){
    std::cerr << "[" << id << "] word=" << word << " prob=" << prob << " bow=" << bow;
    std::cerr << " independent_left=" << (independent_left?"T":"F") << " extend=" << (extend?"T":"F") << " State=[ ";
    for(int i = 0; i < state.get_count(); ++i){
        std::cerr << state.get_word(i) << " ";
    }
    std::cerr << "]" << std::endl;
}

float ReverseDA::get_prob(VocabId word, State &state){
    unsigned char count = state.get_count();
    VocabId last_word = word;
    if(count > 0) last_word = state.get_word(count-1);
    //debug_print_return_rev(-1, daid_, word, 0.0, 0.0, state, false, false);
    state.push_word(word);
    state.set_count(0);

    Traverse t(this, 0);
    Target target;
    float bow = 0.0f;
    for(int depth = 0; depth < count; ++depth){
        if(jump(t, state.get_word(depth))){
            target.set(this, t);

            bow = state.get_bow(depth);
            if(target.independent_right()){
                state.set_bow(depth, 0.0f);
            }else{
                state.set_bow(depth, target.bow(bow_max_));
                state.set_count((unsigned char)(depth + 1));
            }

            if(target.independent_left()){
                bow += sum_bows(state, depth+1, count);
                //debug_print_return_rev(1, daid_, word, target.prob, bow, state, target.independent_left(), false);
                return target.prob + bow;
            }
        }else{
            bow += sum_bows(state, depth, count);
            //debug_print_return_rev(1, daid_, word, target.prob, bow, state, target.independent_left(), false);
            return target.prob + bow;
        }
    }

    // case for the last word.
    if(jump(t, last_word)){
        //std::cerr << "next=" << next << " prob=" << prob << " bow_state=" << bow_state << " independent_right=" << independent_right << std::endl;
        target.set(this, t);

        if(count < context_size && !target.independent_right()){
            state.set_bow(count, target.bow(bow_max_));
            state.set_count((unsigned char)(count + 1));
        }
        //debug_print_return_rev(1, daid_, word, target.prob, 0.0, state, target.independent_left(), false);
        return target.prob;
    }else{
        // independent_left=true;
        if(count==0){
            // emulate KenLM
            state.set_count(1);
            state.set_bow(0, 0.0f);
        }
        //debug_print_return_rev(1, daid_, word, target.prob, bow, state, false, false);
        return target.prob + bow;
    }
}

float ReverseDA::get_prob(VocabId word, State &state, Fragment &fnew){
    float bow = 0.0f;

    unsigned char count = state.get_count();
    VocabId last_word = word;
    if(count > 0) last_word = state.get_word(count-1);
    //debug_print_return_rev(-2, daid_, word, 0, 0, state, false, false);
    state.push_word(word);
    state.set_count(0);

    Traverse t(this, 0);
    Target target;

    fnew.status.word=word;
    for(int depth = 0; depth < count; ++depth){
        if(jump(t, state.get_word(depth))){
            target.set(this, t);

            bow = state.get_bow(depth);
            if(target.independent_right()){
                state.set_bow(depth, 0.0f);
            }else{
                state.set_bow(depth, target.bow(bow_max_));
                state.set_count((unsigned char)(depth + 1));
            }

            if(target.independent_left()){
                bow += sum_bows(state, depth+1, count);
                fnew.status.pos = t.index;
                //debug_print_return_rev(2, daid_, word, target.prob, bow, state, target.independent_left(), false);
                return -(target.prob + bow);
            }
        }else{
            bow += sum_bows(state, depth, count);
            //independent_left=true;
            fnew.status.pos = t.index;
            //debug_print_return_rev(2, daid_, word, target.prob, bow, state, true, false);
            return -(target.prob + bow);
        }
    }

    // case for the last word.
    if(jump(t, last_word)){
        target.set(this ,t);

        if(count < context_size && !target.independent_right()){
            state.set_bow(count, target.bow(bow_max_));
            state.set_count((unsigned char)(count + 1));
        }
        fnew.status.pos = t.index;
        //debug_print_return_rev(2, daid_, word, target.prob, 0, state, target.independent_left(), false);
        return target.independent_left()?-target.prob:target.prob;
    }else{
        //independent_left=true;
        if(count==0){
            // emulate KenLM
            state.set_count(1);
            state.set_bow(0, 0.0f);
        }
        fnew.status.pos = t.index;
        //debug_print_return_rev(2, daid_, word, target.prob, bow, state, true, false);
        return -(target.prob + bow);
    }
}

float ReverseDA::get_prob(const Fragment &f, State &state, Gap &gap){
    unsigned char g = gap.get_gap();
    unsigned char depth = g+1;
    unsigned char count = state.get_count();
    bool &independent_left = gap.is_finalized();
    bool &extended = gap.is_extended();
    VocabId last_word = f.status.word;
    if(count > 0) last_word = state.get_word(count-1);

    assert(count > g);
    assert(da_array_[f.status.pos].base.base_val >= 0);

    //debug_print_return_rev(-3, daid_, f.status.word, 0, 0, state, false, false);
    extended = false;
    independent_left = false;

    state.push_word(f.status.word);
    state.set_count(depth);

    float bow = (depth-1 < count)?state.get_bow(depth-1):0.0f;

    Traverse t(this, f.status.pos);
    Target target;
    target.set(this, t);
    float prob_prev = target.prob;

    for(; depth < count; ++depth){
        if(jump(t, state.get_word(depth))){
            target.set(this, t);
            extended = true;

            bow = state.get_bow(depth);
            if(target.independent_right()){
                state.set_bow(depth, 0.0f);
            }else{
                state.set_bow(depth, target.bow(bow_max_));
                state.set_count((unsigned char)(depth + 1));
            }

            if(target.independent_left()){
                bow += sum_bows(state, depth+1, count);
                independent_left = true;
                //debug_print_return_rev(3, daid_, f.status.word, target.prob, bow, state, independent_left, extended);
                return target.prob + bow - prob_prev;
            }
        }else{
            bow += sum_bows(state, depth, count);
            independent_left=true;
            //debug_print_return_rev(3, daid_, f.status.word, target.prob, bow, state, independent_left, extended);
            return target.prob + bow - prob_prev;
        }
    }

    // case for the last word.
    if(jump(t, last_word)){
        target.set(this, t);
        extended=true;

        if(count < context_size && !target.independent_right()){
            state.set_bow(count, target.bow(bow_max_));
            state.set_count((unsigned char)(count + 1));
        }
        independent_left = target.independent_left();

        //debug_print_return_rev(3, daid_, f.status.word, target.prob, 0.0, state, independent_left, extended);
        return target.prob - prob_prev;
    }else{
        independent_left=true;
        //debug_print_return_rev(3, daid_, f.status.word, target.prob, bow, state, independent_left, extended);
        assert(count != 0);
        return target.prob + bow - prob_prev;
    }
}

float ReverseDA::get_prob(const Fragment &f, State &state, Gap &gap, Fragment &fnew){
    unsigned char g = gap.get_gap();
    unsigned char depth = g+1;
    unsigned char count = state.get_count();
    bool &independent_left = gap.is_finalized();
    bool &extended = gap.is_extended();
    VocabId last_word = f.status.word;
    if(count > 0) last_word = state.get_word(count-1);

    assert(count > g);
    assert(da_array_[f.status.pos].base.base_val >= 0);

    //debug_print_return_rev(-4, daid_, f.status.word, 0, 0, state, false, false);
    fnew = f;
    extended = false;
    independent_left = false;

    state.push_word(f.status.word);
    state.set_count(depth);

    float bow = (depth-1 < count)?state.get_bow(depth-1):0.0f;

    Traverse t(this, f.status.pos);
    Target target;
    target.set(this, t);
    float prob_prev = target.prob;
    for(; depth < count; ++depth){
        if(jump(t, state.get_word(depth))){
            target.set(this, t);
            extended = true;

            bow = state.get_bow(depth);
            if(target.independent_right()){
                state.set_bow(depth, 0.0f);
            }else{
                state.set_bow(depth, target.bow(bow_max_));
                state.set_count((unsigned char)(depth + 1));
            }

            if(target.independent_left()){
                bow += sum_bows(state, depth+1, count);
                independent_left = true;
                fnew.status.pos = t.index;
                //debug_print_return_rev(4, daid_, f.status.word, target.prob, bow, state, independent_left, extended);
                return target.prob + bow - prob_prev;
            }
        }else{
            bow += sum_bows(state, depth, count);
            independent_left=true;
            fnew.status.pos = t.index;
            //debug_print_return_rev(4, daid_, f.status.word, target.prob, bow, state, independent_left, extended);
            return target.prob + bow - prob_prev;
        }
    }

    // case for the last word.
    if(jump(t, last_word)){
        target.set(this, t);
        extended=true;

        if(count < context_size && !target.independent_right()){
            state.set_bow(count, target.bow(bow_max_));
            state.set_count((unsigned char)(count + 1));
        }
        fnew.status.pos = t.index;
        independent_left = target.independent_left();
        //debug_print_return_rev(4, daid_, f.status.word, target.prob, 0.0, state, independent_left, extended);
        return target.prob - prob_prev;
    }else{
        independent_left=true;
        fnew.status.pos = t.index;
        assert(count != 0);
        //debug_print_return_rev(4, daid_, f.status.word, target.prob, bow, state, independent_left, extended);
        return target.prob + bow - prob_prev;
    }
}

float ReverseDA::update(VocabId word, Fragment &f, bool &extended, bool &independent_left, bool &independent_right, float &bow) {
    throw std::runtime_error("Not implemented.");
    /*
    int next;
    float prob_state = 0.0;

    extended = jump(f.status.pos, word);

    if(extended){
        prob_state -= da_array_[endmarker_node(f.status.pos)].check.logprob;
        f.status.pos = next;
    }else{
        prob_state += bow;
        independent_left = true;
    }

    return prob_state;
     */
}

void ReverseDA::init_state(VocabId *word, unsigned char order, State &state){
    /*
    std::cerr << "! ";
    for(int i = 0; i < order; ++i){
        std::cerr << word[i] << " ";
    }
    std::cerr << std::endl;
    debug_print_return_rev(-5, daid_, 0, 0, 0, state, false, false);
    */

    Traverse t(this, 0);
    Target target;
    state.set_count(0);
    for(unsigned char i = 0; i < order; i++){
        if(jump(t, word[i])){
            target.set(this, t);
            state.set_word(i, word[i]);
            if(target.independent_right()){
                state.set_bow(i, 0.0f);
            }else{
                state.set_bow(i, target.bow(bow_max_));
                state.set_count((unsigned char)(i+1));
            }
            if(target.independent_left()) break;
        }else{
            break;
        }
    }
    //debug_print_return_rev(5, daid_, 0, 0, 0, state, false, false);
}

void ReverseDA::init_state(State &state, const State &state_prev, const Fragment *, const Gap &gap){
    //debug_print_return_rev(-6, daid_, 0, 0, 0, state, false, false);
    //debug_print_return_rev(-6, daid_, 0, 0, 0, state_prev, false, false);
    //std::cerr << "gap.get_gap()=" << (int)gap.get_gap() << std::endl;
    assert(state.get_count() > gap.get_gap());
    assert(state_prev.get_count() == gap.get_gap());

    for(int i = 0; i < state_prev.get_count(); ++i){
        assert(state.get_word(i) == state_prev.get_word(i));
        state.set_bow(i, state_prev.get_bow(i));
    }
    //debug_print_return_rev(6, daid_, 0, 0, 0, state, false, false);
}

unsigned long int ReverseDA::get_state(VocabId *word, unsigned char order){
    logger_ << "ReverseDA does not support get_state()." << Logger::endc;
    throw "unsupported";
}

bool ReverseDA::checkinput(unsigned short n,unsigned int *ngram,float bow,float prob,bool bow_presence){
    float prob_state;
    float bow_state;

    Traverse t(this, 0);
    for(std::size_t i = 0; i < n; i++){
        if(!jump(t, ngram[n - 1 - i])){
            logger_ << "ReverseDA[" << daid_ << "] transition error!" << Logger::ende;
            return false;
        }
    }

    Target target;
    target.set(this, t);
    prob_state = target.prob;
    if(target.independent_right()){
        bow_state = 0.0f;
    }else{
        bow_state = target.bow(bow_max_);
    }

    if(std::isnan(prob_state) || std::isnan(bow_state) || prob != prob_state || std::abs(bow - bow_state) > 0.001){
        logger_ << "ReverseDA[" << daid_ << "] value error! answer of (prob, bow) is (" << prob << "," << bow << ") but stored is (" << prob_state << "," << bow_state << ")" << Logger::ende;
        return false;
    }

    return true;
}

void ReverseDA::det_base(VocabId *children, float node_prob, Base node_bow, std::size_t n_children, int context,
                         float *&values, std::size_t &n_slot_used, int &head, int &max_slot, uint64_t *&validate,
                         uint64_t words_prefix, std::size_t prefix_length) {
    children_cnt_table_[n_children]++;

    if(n_children ==1 && children[0]==1 && node_bow.bits==0x80000000UL){
        // leaf compression
        // if the node has only one child and the child word is <#>.
        // 0x80000000UL means independent-right.

        if(node_prob > 0.0f){ // independent-left
            da_array_[context].base.logprob = -node_prob;
        }else{
            da_array_[context].base.logprob = node_prob;
        }
        return;
    }

    StopWatch sw;
    int base = head - children[0];
    if (1 + children[0] >= array_size_) {
        base = 1;
    } else { // Find base
        while (base <= 0) {
            base -= da_array_[base + children[0]].check.check_val;
        }
        base = build_da_util::find_base(
#ifdef FIND_BASE_ACCESS_DA
            da_array_,
#endif
            array_size_, children, n_children, base, validate,
#ifndef DALM_NEW_XCHECK
            words_prefix, prefix_length,
#endif
            skip_counts_, loop_counts_);
    }
    children_fb_time_table_[n_children] += sw.milli_sec();

    da_array_[context].base.base_val = base;
    int max_jump = base + children[n_children - 1] + 1;
    if(max_jump >= array_size_){
        resize_array((array_size_ * 2 > max_jump) ? array_size_ * 2 : max_jump * 2, values, validate);
    }

    for(std::size_t i = 0; i < n_children; i++){
        int pos = base + children[i];
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
        da_array_[pos].check.check_val = context;

        std::size_t index = pos / DALM_N_BITS_UINT64_T;
        std::size_t bit = pos % DALM_N_BITS_UINT64_T;
        validate[index] |= (0x1ULL << bit);

        ++n_slot_used;
    }
    if(children[0]==1){ // word[i]==<#>
        int pos = base + children[0];
        values[pos] = node_prob;
        if(node_bow.bits==0x80000000UL){ // independent-right
            node_bow.logbow = std::numeric_limits<float>::infinity();
        }
        da_array_[pos].base.logbow = node_bow.logbow;
    }

    if(max_slot < (int)(base + children[n_children - 1])){
        max_slot = base + children[n_children - 1];
    }
}

int ReverseDA::traverse(VocabId word, int context){
    int dest = word+ da_array_[context].base.base_val;

    if(dest >= array_size_){
        return -1;
    }else if(da_array_[dest].check.check_val == context){
        return dest;
    }else{
        return -1;
    }
}

int ReverseDA::endmarker_node(int context) {
    return 1 + da_array_[context].base.base_val;
}

void ReverseDA::resize_array(int n_array, float *&values, uint64_t *&validate) {
    DAPair *replacing_array = new DAPair[n_array];
    std::copy(da_array_, da_array_ + array_size_, replacing_array);
    delete [] da_array_;
    da_array_ = replacing_array;

    float *replacing_values = new float[n_array];
    std::copy(values, values + array_size_, replacing_values);
    delete [] values;
    values = replacing_values;

    std::size_t n_validate_new = n_array / DALM_N_BITS_UINT64_T + 1;
    std::size_t n_validate_old = array_size_ / DALM_N_BITS_UINT64_T + 1;
    uint64_t *replacing_validate = new uint64_t[n_validate_new];
    std::copy(validate, validate + n_validate_old, replacing_validate);
    delete [] validate;
    validate = replacing_validate;

    DAPair dummy;
    dummy.base.base_val = -1;
    dummy.check.check_val = -1;
    assert(da_array_[array_size_-1].check.check_val < 0);
    std::fill(da_array_ + array_size_, da_array_ + n_array, dummy);
    std::fill(values+ array_size_, values + n_array, std::numeric_limits<float>::infinity());
    std::fill(validate + n_validate_old, validate + n_validate_new, 0);

    array_size_ = n_array;
}


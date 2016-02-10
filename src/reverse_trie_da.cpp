#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <cassert>
#include <utility>
#include "dalm.h"
#include "dalm/reverse_trie_da.h"

using namespace DALM;

ReverseTrieDA::ReverseTrieDA(unsigned int daid, unsigned int datotal, ReverseTrieDA **neighbours, Logger &logger)
    : daid_(daid), datotal_(datotal), da_(neighbours), logger_(logger){

    array_size_ = 3;
    da_array_ = new DABody[array_size_];

    DABody dummy;
    dummy.base = -1;
    dummy.check = -1;
    dummy.logbow = 0.0;
    dummy.logprob = 0.0;

    std::fill(da_array_, da_array_ + array_size_, dummy);

    n_leaves_ = 3;
    leaves_ = new DATail[n_leaves_];
    DATail dummy2;
    dummy2.check = -1;
    dummy2.logprob.base_val = -1;

    std::fill(leaves_, leaves_ + n_leaves_, dummy2);
}

ReverseTrieDA::ReverseTrieDA(BinaryFileReader &reader, ReverseTrieDA **neighbours, unsigned char order, Logger &logger): da_(neighbours), logger_(logger){
    reader >> daid_;
    reader >> datotal_;
    reader >> array_size_;
    da_array_ = new DABody[array_size_];
    reader.read_many(da_array_, (std::size_t)array_size_);

    reader >> n_leaves_;
    leaves_ = new DATail[n_leaves_];
    reader.read_many(leaves_, (std::size_t)n_leaves_);
    context_size = (unsigned char)(order - 1);
    logger_ << "ReverseTrieDA[" << daid_ << "] da-size: " << array_size_ << Logger::endi;
    logger_ << "ReverseTrieDA[" << daid_ << "] leaves-size: " << n_leaves_ << Logger::endi;
}

ReverseTrieDA::~ReverseTrieDA(){
    if(da_array_ != NULL) delete [] da_array_;
    if(leaves_ != NULL) delete [] leaves_;
}

void ReverseTrieDA::fill_inserted_node(std::string &path, Vocabulary &vocab){
    File::TrieFileReader file(path);

    size_t n_entries = file.size();

    size_t tenpercent = n_entries / 10;
    int context = 0;

    for(size_t i = 0; i < n_entries; i++){
        if((i+1) % tenpercent == 0){
            logger_ << "ReverseTrieDA[" << daid_ << "][fill] " << (i+1)/tenpercent << "0% done." << Logger::endi;
        }

        File::ARPAEntry entry = file.next();

        if(entry.words[0] % datotal_ != daid_) continue;

        if(std::isinf(entry.prob)){
            context=0;
            for(std::size_t j = 0; j + 1 < entry.n; ++j){
                context = traverse(entry.words[j], context);
            }
            float prev_prob = da_array_[context].logprob;
            int backoff_context = 0;
            float prev_bow = -std::numeric_limits<float>::infinity();
            if(entry.n > 1){
                ReverseTrieDA *bow_da = da_[entry.words[1] % datotal_];
                for(size_t j = 1; j < entry.n; j++){
                    int next = bow_da->da_array_[backoff_context].base + entry.words[j];
                    if(next < 0){
                        prev_bow = 0.0;
                        break;
                    }else{
                        if(next < bow_da->array_size_ && bow_da->da_array_[next].check == backoff_context){
                            backoff_context = next;
                        }else{
                            prev_bow = 0.0;
                            break;
                        }
                    }
                }
                if(prev_bow != 0.0) prev_bow = bow_da->da_array_[backoff_context].base;
            }else{
                prev_bow = 0.0;
            }

            if(entry.n > 0) context = traverse(entry.words[entry.n - 1], context);
            da_array_[context].logprob = prev_prob + prev_bow;

#ifndef NDEBUG
            BowVal val;
            val.bow = da_array_[context].logbow;
            assert(val.bits == 0x80000000UL);
#endif
        }
    }
}

void ReverseTrieDA::make_da(std::string &pathtotreefile, ValueArrayIndex *, Vocabulary &vocab) {
    File::TrieFileReader file(pathtotreefile);
    int n_unigram = (int)vocab.size();

    resize_array(n_unigram * 20);
    resize_leaves(n_unigram * 20);
    da_array_[0].base = 0;

    std::size_t n_entries = file.size();

    logger_ << "ReverseTrieDA[" << daid_ << "] n_entries=" << n_entries << Logger::endi;
    logger_ << "ReverseTrieDA[" << daid_ << "] n_unigram=" << n_unigram << Logger::endi;

    VocabId *history = new VocabId[DALM_MAX_ORDER];
    DABody *children = new DABody[n_unigram];
    VocabId *words = new VocabId[n_unigram];

    std::size_t n_history = 0;
    std::size_t n_children = 0;
    std::size_t n_slots_used = 1;
    std::size_t n_leaves_used = 1;

    std::fill(history, history + DALM_MAX_ORDER, 0);

    int context = 0;
    std::size_t tenpercent = n_entries / 10;
    int head = 1;
    int max_slot = 0;
    int leaf_head = 1;
    int leaf_max_slot = 0;
    bool leaves = true;

    for(std::size_t i = 0; i < n_entries; ++i){
        if((i+1) % tenpercent == 0){
            logger_ << "ReverseTrieDA[" << daid_ << "] " << (i+1)/tenpercent << "0% done." << Logger::endi;
        }

        File::ARPAEntry entry = file.next();
        if(entry.words[0] % datotal_ != daid_) continue;
        /*
        for(std::size_t j = 0; j < entry.n; j++){
            std::cerr << entry.words[j] << " ";
        }
        std::cerr << "prob=" << entry.prob << " bow=" << entry.bow << std::endl;
        */

        if(n_history + 1 != entry.n
                || memcmp(history, entry.words, sizeof(VocabId)*n_history)!=0){

            context = 0;
            for(std::size_t j = 0; j < n_history; j++){
                assert(da_array_[context].base >= 0);
                context = traverse(history[j], context);
                assert(context > 0);
            }
            if(leaves){
                det_base_leaves(words, children, n_children, context, leaf_head, leaf_max_slot);
                n_leaves_used += n_children;
            }else{
                det_base(words, children, n_children, context, head, max_slot);
                n_slots_used += n_children;
            }

            n_history = (std::size_t)entry.n-1;
            std::copy(entry.words, entry.words + n_history, history);
            n_children =0;
            leaves = true;
        }

        DABody &node = children[n_children];
        node.logprob = entry.prob;
        node.logbow = entry.bow;
        node.base = -1;
        node.check = -1;
        words[n_children] = entry.words[entry.n - 1];
        BowVal bow_bits;
        bow_bits.bow = entry.bow;

        // confirm either independent_left and independent_right
        if(entry.prob <= 0 || bow_bits.bits != 0x80000000UL){
            leaves = false;
        }

        ++n_children;
    }

    context = 0;
    for(std::size_t j = 0; j < n_history; j++){
        context = traverse(history[j], context);
    }
    if(leaves){
        det_base_leaves(words, children, n_children, context, leaf_head, leaf_max_slot);
    }else{
        det_base(words, children, n_children, context, head, max_slot);
    }

    fit_to_size(max_slot, leaf_max_slot);

    delete [] history;
    delete [] children;
    delete [] words;

    logger_ << "ReverseTrieDA[" << daid_ << "] da-size: " << array_size_
            << " leaves-size: " << n_leaves_ << " n_slots_used: " << n_slots_used
            << " n_leaves_used: " << n_leaves_used << Logger::endi;
}

void ReverseTrieDA::dump(BinaryFileWriter &writer) {
    writer << daid_;
    writer << datotal_;
    writer << array_size_;
    writer.write_many(da_array_, (std::size_t)array_size_);
    writer << n_leaves_;
    writer.write_many(leaves_, (std::size_t)n_leaves_);
}

void ReverseTrieDA::write_and_free(BinaryFileWriter &writer){
    if(da_array_ != NULL){
        writer.write_many(da_array_, (std::size_t)array_size_);
        delete [] da_array_;
        da_array_=NULL;
    }

    if(leaves_ != NULL){
        writer.write_many(leaves_, (std::size_t)n_leaves_);
        delete [] leaves_;
        leaves_=NULL;
    }
}

void ReverseTrieDA::reload(BinaryFileReader &reader){
    da_array_ = new DABody[array_size_];
    leaves_ = new DATail[n_leaves_];
    reader.read_many(da_array_, (std::size_t)array_size_);
    reader.read_many(leaves_, (std::size_t)n_leaves_);
}

float ReverseTrieDA::get_prob(VocabId *word, unsigned char order){
    float prob = DALM_OOV_PROB;
    float bow = 0.0;

    int context = 0;
    int next;
    bool independent_right_ignored = false;
    bool independent_left_ignored = false;

    for(unsigned char depth=0; depth < order; depth++){
        bool success = jump(context, word[depth], next, prob, bow, independent_left_ignored, independent_right_ignored);
        next = da_array_[context].base + word[depth];
        if(success) {
            context = next;
        }else{
            break;
        }
    }

    if(order > 1){
        ReverseTrieDA *bowda = da_[word[1] % datotal_];
        context = 0;
        for(unsigned char hist = 1; hist < order; hist++){
            bool success = bowda->jump(context, word[hist], next, prob, bow, independent_left_ignored, independent_right_ignored);
            if(success) {
                prob += bow;
            }else{
                break;
            }
        }
    }

    return prob;
}

bool ReverseTrieDA::jump(int context, VocabId word, int &next, float &prob, float &bow, bool &independent_left, bool &independent_right) {
    if(da_array_[context].base < 0){
        next = -da_array_[context].base + word;
        if(leaves_[next].check == context){
            prob = leaves_[next].logprob.logprob;
            bow = -0.0f;
            independent_left = true;
            independent_right = true;
            return true;
        }else{
            return false;
        }
    }

    next = da_array_[context].base + word;
    if(next < array_size_ && da_array_[next].check == context){
        prob = da_array_[next].logprob;
        bow = da_array_[next].logbow;
        BowVal bow_bits;
        bow_bits.bow = bow;
        independent_right = bow_bits.bits == 0x80000000UL;
        if(prob > 0){
            independent_left = true;
            prob = -prob;
        }else{
            independent_left = false;
        }

        return true;
    }
    return false;
}

float ReverseTrieDA::get_prob(VocabId word, State &state){
    float prob = DALM_OOV_PROB;
    float bow = 0.0;

    float prob_state;
    float bow_state;

    int context = 0;
    int next;
    unsigned char depth = 0;
    unsigned char count = state.get_count();
    bool independent_left = false;
    bool independent_right = false;

    state.set_count(0);

    //case for the first word.
    bool success = jump(context, word, next, prob_state, bow_state, independent_left, independent_right);
    if(success){
        if(count > 0) bow = state.get_bow(0);
        else bow = 0.0;
        prob = prob_state;
        
        state.set_bow(0, bow_state);
        if(!independent_right){
            state.set_count(1);
        }
    }
    
    if(!success || independent_left || count==0){
        if(success) depth++;
        bow += sum_bows(state, depth, count);
        state.push_word(word);
        prob += bow;
        return prob;
    }
    context = next;
    depth++;

    // case for the rests except for the last word.
    for(; depth < count; depth++){
        success = jump(context, state.get_word(depth - 1), next, prob_state, bow_state, independent_left, independent_right);
        if(success){
            prob = prob_state;
            bow = state.get_bow(depth);

            state.set_bow(depth, bow_state);
            if(!independent_right){
                state.set_count((unsigned char)(depth+1));
            }
        }

        if(!success || independent_left){
            if(independent_left) depth++;
            bow += sum_bows(state, depth, count);
            state.push_word(word);
            prob += bow;
            return prob;
        }
        context = next;
    }
    
    // case for the last word.
    if(count > 0){
        success = jump(context, state.get_word(depth - 1), next, prob_state, bow_state, independent_left, independent_right);

        if(success){
            prob = prob_state;
            bow = 0;
            if(count < context_size){
                state.set_bow(depth, bow_state);
                if(!independent_right){
                    state.set_count((unsigned char)(depth+1));
                }
            }
        }

        if(!success || independent_left){
            state.push_word(word);
            prob += bow;
            return prob;
        }
        //context = next;
        //depth++;
    }

    state.push_word(word);
    prob += bow;
    return prob;
}

float ReverseTrieDA::get_prob(VocabId word, State &state, Fragment &fnew){
    float bow = 0.0;
    float bow_state;
    float prob = DALM_OOV_PROB;
    float prob_state;

    int &context = fnew.status.pos;
    int next;
    unsigned char depth = 0;
    unsigned char count = state.get_count();
    bool independent_left = false;
    bool independent_right = false;

    state.set_count(0);
    fnew.status.word=word;
    context = 0;

    // case for the first word.
    bool success = jump(context, word, next, prob_state, bow_state, independent_left, independent_right);
    if(success){
        if(count > 0) bow = state.get_bow(0);
        else bow = 0.0;
        prob = prob_state;
        
        state.set_bow(0, bow_state);
        if(!independent_right){
            state.set_count(1);
        }
    }

    if(!success || independent_left || count==0){
        if(success){
            depth++;
            context = next;
        }
        bow += sum_bows(state, depth, count);
        state.push_word(word);
        prob += bow;
        
        return success ?(independent_left ?-prob:prob):-prob;
    }
    context = next;
    depth++;

    // case for the rests except for the last word.
    for(; depth < count; depth++){
        success = jump(context, state.get_word(depth - 1), next, prob_state, bow_state, independent_left, independent_right);
        if(success){
            prob = prob_state;
            bow = state.get_bow(depth);

            state.set_bow(depth, bow_state);
            if(!independent_right){
                state.set_count((unsigned char)(depth+1));
            }
        }

        if(!success || independent_left){
            if(independent_left){
                depth++;
                context = next;
            }
            bow += sum_bows(state, depth, count);
            state.push_word(word);
            prob += bow;
            return -prob;
        }
        context = next;
    }
    
    // case for the last word.
    success = jump(context, state.get_word(depth - 1), next, prob_state, bow_state, independent_left, independent_right);

    if(success){
        prob = prob_state;
        bow = 0;
        if(count < context_size){
            state.set_bow(depth, bow_state);
            if(!independent_right){
                state.set_count((unsigned char)(depth+1));
            }
        }
    }

    if(!success || independent_left){
        if(independent_left){
            //depth++;
            context = next;
        }
        state.push_word(word);
        prob += bow;
        return -prob;
    }
    context = next;
    //depth++;

    state.push_word(word);

    prob += bow;

    return prob;
}

float ReverseTrieDA::get_prob(const Fragment &f, State &state, Gap &gap){
    unsigned char g = gap.get_gap();
    unsigned char depth = g+1; //TODO bug?
    unsigned char count = state.get_count();
    bool &independent_left = gap.is_finalized();
    bool &extended = gap.is_extended();
    state.set_count(depth);

    if(count <= g){
        independent_left =true;
        extended=false;
        return 0.0;
    }
    extended = false;
    independent_left = false;
    bool independent_right = false;

    int context = f.status.pos;
    float prob_prev = da_array_[context].logprob;

    int next;
    float prob = prob_prev;
    float bow = (depth-1<count)?state.get_bow(depth-1):0.0f;
    float prob_state;
    float bow_state;
    bool success;

    for(; depth < count; depth++){
        success = jump(context, state.get_word(depth - 1 - g), next, prob_state, bow_state, independent_left, independent_right);

        if(success){
            prob = prob_state;
            bow = state.get_bow(depth);
            extended = true;

            state.set_bow(depth, bow_state);
            if(!independent_right){
                state.set_count((unsigned char)(depth+1));
            }
        }

        if(!success || independent_left){
            if(independent_left) depth++;
            independent_left = true;
            bow += sum_bows(state, depth, count);
            prob += bow;
            return prob-prob_prev;
        }
        context = next;
    }

    // case for the last word.
    if(count > 0){
        success = jump(context, state.get_word(depth - 1 - g), next, prob_state, bow_state, independent_left, independent_right);

        if(success){
            prob = prob_state;
            bow = 0;
            extended = true;

            if(count < context_size){
                state.set_bow(depth, bow_state);
                if(!independent_right){
                    state.set_count((unsigned char)(depth+1));
                }
            }
        }
        
        if(!success || independent_left){
            independent_left =true;
            prob += bow;
            return prob-prob_prev;
        }

        //context = next;
        //depth++;
    }
    prob += bow;
    return prob-prob_prev;
}

float ReverseTrieDA::get_prob(const Fragment &f, State &state, Gap &gap, Fragment &fnew){
    unsigned char g = gap.get_gap();
    unsigned char depth = g+1; //TODO bug?
    unsigned char count = state.get_count();
    bool &independent_left = gap.is_finalized();
    bool &extended=gap.is_extended();

    fnew = f;
    state.set_count(depth);

    if(count <= g){
        independent_left = true;
        extended = false;
        return 0.0;
    }
    extended = false;
    independent_left = false;

    bool independent_right = false;
    int &context = fnew.status.pos;
    float prob_prev = da_array_[context].logprob;

    int next;
    float prob = prob_prev;
    float bow = (depth-1<count)?state.get_bow(depth-1):0.0f;
    float prob_state;
    float bow_state;
    bool success;

    for(; depth < count; depth++){
        success = jump(context, state.get_word(depth - 1 - g), next, prob_state, bow_state, independent_left, independent_right);

        if(success){
            prob = prob_state;
            bow = state.get_bow(depth);
            extended = true;

            state.set_bow(depth, bow_state);
            if(!independent_right){
                state.set_count((unsigned char)(depth+1));
            }
        }

        if(!success || independent_left){
            if(independent_left){
                depth++;
                context = next;
            }
            independent_left =true;
            bow += sum_bows(state, depth, count);
            prob += bow;
            return prob-prob_prev;
        }
        context = next;
    }

    // case for the last word.
    if(count > 0){
        success = jump(context, state.get_word(depth - 1 - g), next, prob_state, bow_state, independent_left, independent_right);
        if(success){
            prob = prob_state;
            bow = 0;
            extended = true;

            if(count < context_size){
                state.set_bow(depth, bow_state);
                if(!independent_right){
                    state.set_count((unsigned char)(depth+1));
                }
            }
        }
        
        if(!success || independent_left){
            if(independent_left){
                //depth++;
                context = next;
            }
            independent_left =true;
            prob += bow;
            return prob-prob_prev;
        }

        context = next;
        //depth++;
    }

    prob += bow;
    return prob-prob_prev;
}

void ReverseTrieDA::init_state(VocabId *word, unsigned char order, State &state){
    int context = 0;
    int next;
    float prob_state;
    bool finalized = false;
    bool independent_right = false;
    float bow_state;
    state.set_count(0);
    for(unsigned char i = 0; i < order; i++){
        if(jump(context, word[i], next, prob_state, bow_state, finalized, independent_right)){
            state.set_word(i, word[i]);
            state.set_bow(i, bow_state);
            if(!independent_right){
                state.set_count((unsigned char)(i+1));
            }
            context = next;
        }else{
            break;
        }
  }
}

void ReverseTrieDA::init_state(State &state, const State &state_prev, const Fragment *, const Gap &gap){
    state = state_prev;
}

unsigned long int ReverseTrieDA::get_state(VocabId *, unsigned char){
    logger_ << "ReverseTrieDA does not support get_state()." << Logger::endc;
    throw "unsupported";
}

bool ReverseTrieDA::checkinput(unsigned short n,unsigned int *ngram,float bow,float prob,bool bow_presence){
    int context = 0;
    float prob_lookup = 0.0;
    float bow_lookup = 0.0;
    assert(n > 0);
    for(size_t i = 0; i < n; i++){
        int next;
        bool independent_left;
        bool independent_right;

        bool success = jump(context, ngram[n - 1 - i], next, prob_lookup, bow_lookup, independent_left, independent_right);
        if(!success){
            logger_ << "ReverseTrieDA[" << daid_ << "] transition error!" << Logger::ende;
            return false;
        }
        context = next;
    }

    if(((!bow_presence)|| bow_lookup == bow) && prob_lookup == prob){
        return true;
    }else{
        logger_ << "ReverseTrieDA[" << daid_ << "] value error! answer of (prob, bow) is (" << prob << "," << bow << ") but stored is (" << da_array_[context].logprob << "," << da_array_[context].logbow << ")" << Logger::ende;
        return false;
    }
}

void ReverseTrieDA::det_base(VocabId *words, DABody *children, std::size_t n_children, int context, int &head, int &max_slot) {
    int *pos = new int[n_children];
    int base = head - *words;
    while(base < 0){
        base -= da_array_[base + *words].check;
    }

    unsigned k=0; 
    while(k < n_children){
        pos[k] = base + words[k];

        if(pos[k] < array_size_ && da_array_[pos[k]].check >= 0){
            base -= da_array_[base + *words].check;
            k=0;
        }else{
            k++;
        }
    }

    assert(0 <= context && context < array_size_);
    //std::cerr << "context =" << context << " base=" << base << std::endl;
    da_array_[context].base = base;
    
    int max_jump = base + *(words + n_children -1) + 1;
    if(max_jump > array_size_){
        resize_array(array_size_ * 2 > max_jump ? array_size_ * 2 : max_jump*2);
    }

    for(std::size_t i=0; i < n_children; i++){
        int next_index = pos[i] - da_array_[pos[i]].check;

        if(pos[i] == head){
            da_array_[next_index].base = std::numeric_limits<int>::max();
            head = head - da_array_[head].check;
        }else{
            int previous_index = pos[i] + da_array_[pos[i]].base;
            
            da_array_[next_index].base += da_array_[pos[i]].base;
            da_array_[previous_index].check += da_array_[pos[i]].check;
        }
        children[i].check = context;
        da_array_[pos[i]] = children[i];
    }

    if(max_slot < *(pos + n_children - 1)){
        max_slot = *(pos + n_children - 1);
    }

    delete [] pos;
}

void ReverseTrieDA::det_base_leaves(VocabId *words, DABody *children, std::size_t n_children, int context, int &leaf_head, int &leaf_max_slot) {
    int *pos = new int[n_children];

    int base = leaf_head - *words;
    while(base <= 0){
        base -= leaves_[base + *words].check;
    }

    std::size_t k = 0;
    while(k < n_children){
        pos[k] = base + words[k];

        if(pos[k] < n_leaves_ && leaves_[pos[k]].check >= 0){
            base -= leaves_[base + *words].check;
            k=0;
        }else{
            k++;
        }
    }

    assert(0 <= context && context < array_size_);
    //std::cerr << "[leaf] context =" << context << " base=" << -base << std::endl;
    da_array_[context].base = -base;

    int max_jump = base + *(words + n_children - 1) + 1;
    if(max_jump > n_leaves_){
        resize_leaves(n_leaves_ * 2 > max_jump ? n_leaves_ * 2 : max_jump * 2);
    }

    for(std::size_t i=0; i < n_children; i++){
        int next_index = pos[i] - leaves_[pos[i]].check;

        if(pos[i] == leaf_head){
            leaf_head -= leaves_[leaf_head].check;
        }else{
            int previous_index = pos[i] + leaves_[pos[i]].logprob.base_val;

            leaves_[next_index].logprob.base_val += leaves_[pos[i]].logprob.base_val;
            leaves_[previous_index].check += leaves_[pos[i]].check;
        }

        assert(children[i].logprob >= 0.0);
        leaves_[pos[i]].logprob.logprob = -children[i].logprob;
        leaves_[pos[i]].check = context;
    }

    if(leaf_max_slot < *(pos + n_children -1)){
        leaf_max_slot = *(pos + n_children -1);
    }

    delete [] pos;
}

int ReverseTrieDA::traverse(VocabId word, int from){
    int to = da_array_[from].base + word;
    //std::cerr << "from=" << from << " to=" << to << " array_size_=" << array_size_ << " check[" << to << "]=" << da_array_[to].check << std::endl;
    if(to >= array_size_){
        return -1;
    }else if(da_array_[to].check==from){
        return to;
    }else{
        return -1;
    }
}

void ReverseTrieDA::resize_array(long n_array){
    if(n_array > std::numeric_limits<int>::max()){
        throw std::runtime_error("array size overflow");
    }
    DABody *tmp_array = new DABody[n_array];
    std::copy(da_array_, da_array_ + array_size_, tmp_array);
    delete [] da_array_;
    da_array_ = tmp_array;

    DABody dummy;
    dummy.base = -1;
    dummy.check = -1;
    dummy.logbow = 0.0;
    dummy.logprob = 0.0;
    std::fill(da_array_ + array_size_, da_array_ + n_array, dummy);
    array_size_ = n_array;

}

void ReverseTrieDA::resize_leaves(long n_leaves){
    DATail *tmp_leaves = new DATail[n_leaves];
    std::copy(leaves_, leaves_ + n_leaves_, tmp_leaves);
    delete [] leaves_;
    leaves_ = tmp_leaves;

    DATail dummy;
    dummy.check = -1;
    dummy.logprob.base_val = -1;
    std::fill(leaves_ + n_leaves_, leaves_ + n_leaves, dummy);
    n_leaves_ = n_leaves;
}

void ReverseTrieDA::fit_to_size(int max_slot, int leaf_max_slot){
    array_size_ = max_slot + 1;
    DABody *tmp_array = new DABody[array_size_];
    std::copy(da_array_, da_array_ + array_size_, tmp_array);
    delete [] da_array_;
    da_array_ = tmp_array;

    n_leaves_ = leaf_max_slot + 1;
    DATail *tmp_leaves = new DATail[n_leaves_];
    std::copy(leaves_, leaves_ + n_leaves_, tmp_leaves);
    delete [] leaves_;
    leaves_ = tmp_leaves;
}

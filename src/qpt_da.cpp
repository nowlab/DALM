#ifndef QUAIL_DECODER_QUANTIZATION_PARTLY_TRANSPOSED_DA_CPP
#define QUAIL_DECODER_QUANTIZATION_PARTLY_TRANSPOSED_DA_CPP

#include "dalm.h"
#include "dalm/qpt_da.h"


using namespace DALM;

template<typename T,typename S>
QuantPartlyTransposedDA<T,S>::QuantPartlyTransposedDA(unsigned char daid, unsigned char datotal, QuantPartlyTransposedDA<T,S> **neighbours, Logger &logger, int transpose):PartlyTransposedDA(logger){
    logger_ << "QuantPartlyTransposedDA[" << daid_ << "]" << Logger::endi;
    daid_ = daid;
    datotal_ = datotal;
    da_ = neighbours;
    transposed_entries = transpose;
    array_size_ = 3;
    bow_max_ = 0.0f;
    most_right = 0;
    resize_array(3);
}


template<typename PROB, typename BOW>
QuantPartlyTransposedDA<PROB,BOW>::QuantPartlyTransposedDA(BinaryFileReader &reader, QuantPartlyTransposedDA<PROB, BOW> **neighbours,
    unsigned char order, Logger &logger):PartlyTransposedDA(logger){
    DA::context_size = (unsigned char)(order-1);

    reader >> daid_;
    reader >> datotal_;

    da_ = neighbours;
    reader >> array_size_;
    reader >> bow_max_;
    reader >> rightmost_base_;
    reader >> rightmost_bow_;

    
    base_.resize(rightmost_base_, 0);
    check_.resize(array_size_, 0);
    prob_.resize(array_size_, 0);
    bow_.resize(rightmost_bow_, 0);

    reader.read_many(base_.data(), rightmost_base_);
    reader.read_many(check_.data(), array_size_);
    reader.read_many(prob_.data(), array_size_);
    reader.read_many(bow_.data(), rightmost_bow_);



    reader >> transposed_entries;
    reader >> word_offset_size_;
    transposed_offset.reset(new int[word_offset_size_]);
    reader.read_many(transposed_offset.get(), word_offset_size_); 

    {
        std::lock_guard<std::mutex> g {mtx};
        logger << "QuantPartlyTransposedDA[" << daid_ << "] da-size: " << array_size_ << Logger::endi;
    }
}

template<typename PROB, typename BOW>
void QuantPartlyTransposedDA<PROB, BOW>::write_and_free(DALM::BinaryFileWriter& writer){
}

template<typename PROB, typename BOW>
void QuantPartlyTransposedDA<PROB, BOW>::dump(DALM::BinaryFileWriter& writer){
    writer << daid_;
    writer << datotal_;
    writer << array_size_;
    writer << bow_max_;
    writer << rightmost_base_;
    writer << rightmost_bow_;

    writer.write_many(base_.data(), rightmost_base_);
    writer.write_many(check_.data(), array_size_);
    writer.write_many(prob_.data(), array_size_);
    writer.write_many(bow_.data(), rightmost_bow_);

    writer << transposed_entries;
    writer << word_offset_size_;
    writer.write_many(transposed_offset.get(), word_offset_size_); 
    {
        std::lock_guard<std::mutex> g {mtx};
        logger_ << "QuantPartlyTransposedDA[" << daid_ << "] da-size: " << array_size_  << " ::slot_used:: " << n_slot_used << Logger::endi;
    }
}

template<typename PROB, typename BOW>
float QuantPartlyTransposedDA<PROB, BOW>::get_prob(VocabId* words, unsigned char order){
    int pos = 0;
    int offset = base_[pos];
    int next;
    int level = -1;
    float prob = 0;
    float bow = 0.0f;

    for(int index = 0; index < order; ++index){
        if(offset < 0){
            next = abs(offset) + transposed_offset[words[index]];
        }else{
            next = offset + words[index];
        }
        if(next >= array_size_ || (CHECK_BITS&check_[next]) != pos){
            break;
        }
        level = index;
        pos = next;
        prob = DALM::QUANT_INFO::prob_bin[index][DALM::QUANT_INFO::prob_mask&prob_[pos]];

        if((DALM::QUANT_INFO::independent_left_mask & prob_[pos])) break;
        if(rightmost_base_ > pos) offset = base_[pos];
        else offset = 1000000000;
    }

    
    if(order > 1){
      QuantPartlyTransposedDA<PROB, BOW>* bow_da = da_[words[1]%datotal_];
        pos = 0;
        
        offset = bow_da->base_[pos];
        for(int index = 1; index < order; ++index){
            if(offset < 0){
                next = abs(offset) + bow_da->transposed_offset[words[index]];
            }else{
                next = offset + words[index];
            }
            if(next >= bow_da->array_size_ || (CHECK_BITS&bow_da->check_[next]) != pos){
                break;
            }

            pos = next;
            if(rightmost_base_ > pos) offset = bow_da->base_[pos];
            else offset = 1000000000;

            if(index > level){
                if(!(bow_da->check_[pos] & INDEPENDENT_RIGHT_BITS)){
                  if(rightmost_bow_ > pos){
                    if(DALM::QUANT_INFO::bow_bin[index-1][bow_da->bow_[pos]] < 0)
                      bow += DALM::QUANT_INFO::bow_bin[index-1][bow_da->bow_[pos]];
                  }
                }
            }
        }
    }

    return prob + bow;
}


template<typename PROB, typename BOW>
float QuantPartlyTransposedDA<PROB, BOW>::get_prob(VocabId word, DALM::State& state){
    unsigned char count = state.get_count();
    VocabId last_word = word;
    if(count > 0) last_word = state.get_word(count-1);
    state.push_word(word);
    state.set_count(0);

    int pos = 0;
    int offset = base_[pos];
    int next;
    int prob_index = 0;
    float prob = 0.0f;
    float bow = 0.0f;

    for(int depth = 0; depth < count; ++depth){
        if(offset < 0){
            next = abs(offset) + transposed_offset[state.get_word(depth)];
        }else{
            next = offset + state.get_word(depth);
        }
        if(next >= array_size_ || (CHECK_BITS&check_[next]) != pos){
            bow += sum_bows(state, depth, count);
            return prob + bow;
        }

        pos = next;
        prob_index = prob_[pos];
        if(rightmost_base_ > pos){
          offset = base_[pos];
        }else{
          offset = 1000000000;
        }

        bow = state.get_bow(depth);

        //independent right
        if(check_[pos] & INDEPENDENT_RIGHT_BITS){
          state.set_bow(depth, 0.0f);
        }else{
          if( rightmost_bow_ > pos)
            state.set_bow(depth, DALM::QUANT_INFO::bow_bin[depth][bow_[pos]]);
          else
            state.set_bow(depth, 0.0f);

          state.set_count((unsigned char)(depth + 1));
        }

        //independent left
        if((DALM::QUANT_INFO::independent_left_mask & prob_index)){
          bow += sum_bows(state, depth+1, count);
          prob = DALM::QUANT_INFO::prob_bin[depth][DALM::QUANT_INFO::prob_mask & prob_index];
          return prob + bow;
        }else{
          prob = DALM::QUANT_INFO::prob_bin[depth][prob_index];
        }

        if(rightmost_base_ > pos) offset = base_[pos];
        else offset = 1000000000;
    }


    // case for the last word.
    if(offset < 0){
        next = abs(offset) + transposed_offset[last_word];
    }else{
        next = offset + last_word;
    }

    if(next >= array_size_ || (CHECK_BITS&check_[next]) != pos){
        // independent_left=true;
        if(count==0){
            // emulate KenLM
            state.set_count(1);
            state.set_bow(0, 0.0f);
        }
        prob = DALM::QUANT_INFO::prob_bin[count-1][DALM::QUANT_INFO::prob_mask&prob_index];
        return prob + bow;
    }
    pos = next;
    prob_index = prob_[pos];
    if(rightmost_base_ > pos){
      offset = base_[pos];
    }else{
      offset = 1000000000;
    }

    if(count < context_size && !(check_[pos] & INDEPENDENT_RIGHT_BITS)){
      if( rightmost_bow_ > pos){
        state.set_bow(count, DALM::QUANT_INFO::bow_bin[count][bow_[pos]]);
      }else{
        state.set_bow(count, 0);
      }
      state.set_count((unsigned char)(count + 1));
    }

    prob = DALM::QUANT_INFO::prob_bin[count][DALM::QUANT_INFO::prob_mask & prob_index];
    return prob;
}

template<typename PROB, typename BOW>
void QuantPartlyTransposedDA<PROB, BOW>::init_state(unsigned int* words, unsigned char order , DALM::State& state){

    int pos = 0;
    int offset = base_[pos];
    int next;
    int prob_index = 0;
    state.set_count(0);

    for(int index = 0; index < order; ++index){
        if(offset < 0){
            next = abs(offset) + transposed_offset[words[index]];
        }else{
            next = offset + words[index];
        }
        if(next >= array_size_ || (CHECK_BITS&check_[next]) != pos){
            break;
        }
        pos = next;
        prob_index = prob_[pos];
        state.set_word(index, words[index]);

        if(check_[pos] & INDEPENDENT_RIGHT_BITS){
            state.set_bow(index, 0.0f);
        }else{
            if( rightmost_bow_ > pos)
              state.set_bow(index, DALM::QUANT_INFO::bow_bin[index][bow_[pos]]);
            else
              state.set_bow(index, 0.0f);

            state.set_count((unsigned char)(index+1));
        }

        if((DALM::QUANT_INFO::independent_left_mask & prob_index)){
          break;
        }
        offset = base_[pos];
    }
}

template<typename PROB, typename BOW>
void QuantPartlyTransposedDA<PROB, BOW>::fill_inserted_node(std::string& path_to_tree, DALM::Vocabulary& vocab){

    prob_.resize(da_array_.size(), 0);
    bow_.resize(da_array_.size(), 0);
    base_.resize(da_array_.size(), -1);
    check_.resize(da_array_.size(), 0);


    std::vector<int> independent_rights;

    File::TrieNodeFileReader file(path_to_tree);
    std::size_t n_entries = file.size();
    int ten_percent = n_entries / 10;
    //File::TrieNode root = file.next();
    for(std::size_t i = 0; i < n_entries; ++i){
        File::TrieNode entry = file.next();

        if((i+1) % ten_percent == 0){
            std::lock_guard<std::mutex> g {mtx};
            logger_ << "QuantPartlyTransposedDA[" << daid_ << "][fill_inserted_node] " << (i+1)/ten_percent << "0% done." << Logger::endi;
        }
        if(entry.unique_id == 1 || entry.words[0] % datotal_ != daid_){
            continue;
        }
        int ind = da_n_pos_[entry.unique_id];
        assert(ind >= 0);

        if(entry.independent_left) prob_[ind] = DALM::QUANT_INFO::independent_left_mask|entry.prob.index;
        else prob_[ind] = entry.prob.index;

        bow_[ind] = entry.bow.index;
        if(entry.independent_right){
            independent_rights.push_back(ind);
        }

    }

    da_n_pos_[0] = 0;
    da_n_pos_[1] = 0;
    rightmost_bow_ = 0;
    rightmost_base_ = 0;

    for(int i = 0; i < (int)da_array_.size(); ++i){
        if(da_array_[i].base<= transposed_entries){
            base_[i] = -da_array_[i].base;
        }else{
            base_[i] = da_offset_[da_array_[i].base];
        }
        check_[i] = da_n_pos_[da_array_[i].check];

        if(base_[i] != -1) rightmost_base_ = i;
        if(bow_[i] != 0) rightmost_bow_ = i;
    }
    
    // independent lefts
    for(size_t i = 0; i < independent_rights.size(); ++i) check_[i] |= INDEPENDENT_RIGHT_BITS;
    {
        std::lock_guard<std::mutex> g {mtx};
        logger_ << "QuantPartlyTransposedDA[" << daid_ << "][da_size] " << da_array_.size() << Logger::endi;
        logger_ << "QuantPartlyTransposedDA[" << daid_ << "][rightmost_base_] " << rightmost_base_ << Logger::endi;
        logger_ << "QuantPartlyTransposedDA[" << daid_ << "][rightmost_bow_] " << rightmost_bow_ << Logger::endi;
    }
}

template<typename PROB, typename BOW>
float QuantPartlyTransposedDA<PROB, BOW>::get_prob(VocabId word, DALM::State& state, DALM::Fragment& f){
    unsigned char count = state.get_count();
    VocabId last_word = word;
    if(count > 0) last_word = state.get_word(count-1);
    state.push_word(word);
    state.set_count(0);

    int pos = 0;
    int offset = base_[pos];
    int next;
    int prob_index = 0;
    float prob = 0.0f;
    float bow = 0.0f;

    f.status.word = word;
    for(int depth = 0; depth < count; ++depth){
        if(offset < 0){
            next = abs(offset) + transposed_offset[state.get_word(depth)];
        }else{
            next = offset + state.get_word(depth);
        }
        if(next >= array_size_ || (CHECK_BITS&check_[next]) != pos){
            bow += sum_bows(state, depth, count);
            f.status.pos = pos;
            return -(prob + bow);
        }

        pos = next;
        prob_index = prob_[pos];
        if(rightmost_base_ > pos){
          offset = base_[pos];
        }else{
          offset = 1000000000;
        }

        bow = state.get_bow(depth);

        //independent right
        if(check_[pos] & INDEPENDENT_RIGHT_BITS){
          state.set_bow(depth, 0.0f);
        }else{
          if( rightmost_bow_ > pos)
            state.set_bow(depth, DALM::QUANT_INFO::bow_bin[depth][bow_[pos]]);
          else
            state.set_bow(depth, 0.0f);

          state.set_count((unsigned char)(depth + 1));
        }

        //independent left
        if((DALM::QUANT_INFO::independent_left_mask & prob_index)){
          bow += sum_bows(state, depth+1, count);
          prob = DALM::QUANT_INFO::prob_bin[depth][DALM::QUANT_INFO::prob_mask&prob_index];
          f.status.pos = pos;
          return -(prob + bow);
        }else{
          prob = DALM::QUANT_INFO::prob_bin[depth][prob_index];
        }
    }


    // case for the last word.
    if(offset < 0){
        next = abs(offset) + transposed_offset[last_word];
    }else{
        next = offset + last_word;
    }

    if(next >= array_size_ || (CHECK_BITS&check_[next]) != pos){
        // independent_left=true;
        if(count==0){
            state.set_count(1);
            state.set_bow(0, 0.0f);
        }
        prob = DALM::QUANT_INFO::prob_bin[count-1][DALM::QUANT_INFO::prob_mask&prob_index];
        f.status.pos = pos;
        return -(prob + bow);
    }
    pos = next;
    prob_index = prob_[pos];

    if(count < context_size && !(check_[pos] & INDEPENDENT_RIGHT_BITS)){
      if( rightmost_bow_ > pos){
        state.set_bow(count, DALM::QUANT_INFO::bow_bin[count][bow_[pos]]);
      }else{
        state.set_bow(count, 0);
      }
      state.set_count((unsigned char)(count + 1));
    }

    prob = DALM::QUANT_INFO::prob_bin[count][DALM::QUANT_INFO::prob_mask & prob_index];
    f.status.pos=pos;
    return (DALM::QUANT_INFO::independent_left_mask & prob_index)?-prob:prob;
}

template<typename PROB, typename BOW>
float QuantPartlyTransposedDA<PROB, BOW>::get_prob(DALM::Fragment const& f, DALM::State& state, DALM::Gap& gap){
    unsigned char g = gap.get_gap();
    unsigned char depth = g+1;
    unsigned char count = state.get_count();
    bool &independent_left = gap.is_finalized();
    bool &extended = gap.is_extended();


    VocabId last_word = f.status.word;
    if(count > 0) last_word = state.get_word(count-1);

    extended = false;
    independent_left = false;


    state.push_word(f.status.word);
    state.set_count(depth);


    int pos = f.status.pos;
    int offset = base_[pos];
    int next;
    float bow = (depth-1 <count)?state.get_bow(depth-1):0.0f;
    float prob_prev = DALM::QUANT_INFO::prob_bin[depth-1][DALM::QUANT_INFO::prob_mask & prob_[pos]];


    for(; depth < count; ++depth){
        if(offset < 0){
            next = abs(offset) + transposed_offset[state.get_word(depth)];
        }else{
            next = offset + state.get_word(depth);
        }
        if(next >= check_.size() || (CHECK_BITS&check_[next]) != pos){
            bow += sum_bows(state, depth, count);
            independent_left = true;
            return DALM::QUANT_INFO::prob_bin[depth-1][DALM::QUANT_INFO::prob_mask&prob_[pos]] + bow - prob_prev;
        }
        extended = true;

        pos = next;
        if(base_.size() > pos) offset = base_[pos];
        else offset = 1000000000;

        bow = state.get_bow(depth);

        //independent right
        if(check_[pos] & INDEPENDENT_RIGHT_BITS){
          state.set_bow(depth, 0.0f);
        }else{
          if(bow_.size() > pos)state.set_bow(depth, DALM::QUANT_INFO::bow_bin[depth][bow_[pos]]);
          else state.set_bow(depth, 0);
          state.set_count((unsigned char)(depth + 1));
        }

        //independent left
        if(DALM::QUANT_INFO::independent_left_mask&prob_[pos]){
          bow += sum_bows(state, depth+1, count);
          independent_left = true;
          return -DALM::QUANT_INFO::prob_bin[depth][DALM::QUANT_INFO::prob_mask&prob_[pos]] + bow - prob_prev;
        }
    }


    // case for the last word.
    if(offset < 0){
        next = abs(offset) + transposed_offset[last_word];
    }else{
        next = offset + last_word;
    }
    if(next >= check_.size() || (CHECK_BITS&check_[next]) != pos){
        // independent_left=true;
        independent_left = true;
        return DALM::QUANT_INFO::prob_bin[count-1][DALM::QUANT_INFO::prob_mask&prob_[pos]] + bow - prob_prev;
    }
    pos = next;
    //if(base_.size() > pos) offset = base_[pos];
    //else offset = 1000000000;
    extended = true;

    if(count < context_size && !(check_[pos]&INDEPENDENT_RIGHT_BITS)){
      if(bow_.size() > pos)state.set_bow(count, DALM::QUANT_INFO::bow_bin[count][bow_[pos]]);
      else state.set_bow(count, 0);
      state.set_count((unsigned char)(count + 1));
    }
    independent_left = true;

    if(DALM::QUANT_INFO::independent_left_mask & prob_[pos])return -DALM::QUANT_INFO::prob_bin[count][DALM::QUANT_INFO::prob_mask & prob_[pos]] + bow - prob_prev;
    else return DALM::QUANT_INFO::prob_bin[count][prob_[pos]] + bow - prob_prev;
}
template<typename PROB, typename BOW>
float QuantPartlyTransposedDA<PROB, BOW>::get_prob(DALM::Fragment const& f, DALM::State& state, DALM::Gap& gap, DALM::Fragment& fnew){
    unsigned char g = gap.get_gap();
    unsigned char depth = g+1;
    unsigned char count = state.get_count();
    bool &independent_left = gap.is_finalized();
    bool &extended = gap.is_extended();


    VocabId last_word = f.status.word;
    if(count > 0) last_word = state.get_word(count-1);

    fnew = f;
    extended = false;
    independent_left = false;


    state.push_word(f.status.word);
    state.set_count(depth);


    int pos = f.status.pos;
    int offset = base_[pos];
    int next;
    float bow = (depth-1 < count)?state.get_bow(depth-1):0.0f;
    float prob_prev = DALM::QUANT_INFO::prob_bin[depth-1][DALM::QUANT_INFO::prob_mask & prob_[pos]];

    float prob = 0.0f;


    for(; depth < count; ++depth){
        if(offset < 0){
            next = abs(offset) + transposed_offset[state.get_word(depth)];
        }else{
            next = offset + state.get_word(depth);
        }
        if(next >= check_.size() || (CHECK_BITS&check_[next]) != pos){
            bow += sum_bows(state, depth, count);
            independent_left = true;
            fnew.status.pos = pos;
            return prob + bow - prob_prev;
        }
        extended = true;

        pos = next;
        prob = DALM::QUANT_INFO::prob_bin[depth][DALM::QUANT_INFO::prob_mask & prob_[pos]];
        if(base_.size() > pos) offset = base_[pos];
        else offset = 1000000000;

        bow = state.get_bow(depth);

        //independent right
        if(check_[pos] & INDEPENDENT_RIGHT_BITS){
          state.set_bow(depth, 0.0f);
        }else{
          if(bow_.size() > pos)state.set_bow(depth, DALM::QUANT_INFO::bow_bin[depth][bow_[pos]]);
          else state.set_bow(depth, 0);
          state.set_count((unsigned char)(depth + 1));
        }

        //independent left
        if(DALM::QUANT_INFO::independent_left_mask&prob_[pos]){
          bow += sum_bows(state, depth+1, count);
          independent_left = true;
          fnew.status.pos = pos;
          return -prob + bow - prob_prev;
        }
    }


    // case for the last word.
    if(offset < 0){
        next = abs(offset) + transposed_offset[last_word];
    }else{
        next = offset + last_word;
    }
    if(next >= check_.size() || (CHECK_BITS&check_[next]) != pos){
        // independent_left=true;
        independent_left = true;
        fnew.status.pos = pos;
        return prob + bow - prob_prev;
    }
    pos = next;
    prob =DALM::QUANT_INFO::prob_bin[count][ DALM::QUANT_INFO::prob_mask & prob_[pos]];
    //if(base_.size() > pos) offset = base_[pos];
    //else offset = 1000000000;
    extended = true;

    if(count < context_size && !(check_[pos]&INDEPENDENT_RIGHT_BITS)){
      if(bow_.size() > pos)state.set_bow(count, DALM::QUANT_INFO::bow_bin[count][bow_[pos]]);
      else state.set_bow(count, 0);
      state.set_count((unsigned char)(count + 1));
    }
    independent_left = true;

    fnew.status.pos = pos;
    if(DALM::QUANT_INFO::independent_left_mask&prob_[pos])prob = -prob;
    return prob + bow - prob_prev;
}

#endif //QUAIL_DECODER_QUANTIZATION_PARTLY_TRANSPOSED_DA_CPP

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cmath>

#include <utility>
#include <limits>
#include <mutex>
#include <thread>

#include "dalm.h"
#include "dalm/pt_da.h"

using namespace DALM;
// for multi threading
std::mutex PartlyTransposedDA::mtx;

PartlyTransposedDA::PartlyTransposedDA(unsigned char daid, unsigned char datotal, PartlyTransposedDA **neighbours,
    Logger &logger, int transpose):daid_(daid), datotal_(datotal), da_(neighbours), logger_(logger), transposed_entries(transpose){
    array_size_ = 1024;
    bow_max_ = 0.0f;
    most_right = 0;

    resize_array(3);
}

PartlyTransposedDA::PartlyTransposedDA(Logger &logger):logger_(logger){}

PartlyTransposedDA::PartlyTransposedDA(BinaryFileReader &reader, PartlyTransposedDA **neighbours,
    unsigned char order, Logger &logger): logger_(logger){
    DA::context_size = (unsigned char)(order-1);
    DABody dummy;
    dummy.base = -1;
    dummy.check = -1;
    dummy.logprob = 0.0f;
    dummy.logbow = 0.0f;

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
    
    reader.read_many(base_, rightmost_base_);
    reader.read_many(check_, array_size_);
    reader.read_many(prob_, array_size_);
    reader.read_many(bow_, rightmost_bow_);


    reader >> transposed_entries;
    reader >> word_offset_size_;
    transposed_offset.reset(new int[word_offset_size_]);
    reader.read_many(transposed_offset.get(), word_offset_size_); 

    {
        std::lock_guard<std::mutex> g {mtx};
        logger << "PartlyTransposedDA[" << daid_ << "] da-size: " << array_size_ << Logger::endi;
    }
}

PartlyTransposedDA::~PartlyTransposedDA(){
}

void PartlyTransposedDA::make_da(std::string &path_to_tree_file, ValueArrayIndex *value_array_index, Vocabulary &vocab){

    File::TrieNodeFileReader file(path_to_tree_file);
    BinaryFileReader reader(path_to_tree_file + ".sorted_nodes");
    File::TrieNode entry;
    File::NodeInfo ni;

    size_t n_entries = file.size();
    {
        std::lock_guard<std::mutex> g {mtx};
        logger_ << "Partly_transposedDA[" << daid_ << "]file=" << path_to_tree_file << Logger::endi;
        logger_ << "Partly_transposedDA[" << daid_ << "]transposed_entry=" << transposed_entries << Logger::endi;
        logger_ << "PartlyTransposedDA[" << daid_ << "]n_entries=" << n_entries << Logger::endi;
        logger_ << "PartlyTransposedDA[" << daid_ << "][make_da]" << path_to_tree_file << Logger::endi;
    }


    size_t n_unigram = vocab.size() + 1;
    std::vector<std::pair<int,int>> transpose_word_nodes[n_unigram];

    read_transposed(file, reader, transpose_word_nodes);


    resize_array(n_entries/datotal_);

    //  mark da[0] is used!
    da_array_[0].base = 1;
    da_array_[0].check = 0;

    int head = 1;
    int offset = 0;
    transposed_offset.reset(new int[n_unigram]);
    for(int i = 0; i < n_unigram; ++i) transposed_offset[i] = 1000000000;
    word_offset_size_ = n_unigram;


    size_t ten_percent = n_unigram / 10;

    for(size_t i = 0; i < n_unigram; ++i){
        if((i+1) % ten_percent == 0){
            std::lock_guard<std::mutex> g {mtx};
            logger_ << "PartlyTransposedDA[" << daid_ << "][transpose_word_node] " << (i+1)/ten_percent << "0% done." << Logger::endi;
            logger_ << "PartlyTransposedDA[" << daid_ << "][transpose_word_node] " << n_slot_used << "/" << most_right << " (size)" << Logger::endi;
        }
        if(transpose_word_nodes[i].size() > (size_t)0){
            offset = det_base(transpose_word_nodes[i], head, -(i+1), (int)transpose_word_nodes[i].size());
            assert(offset < (int)n_entries);
            transposed_offset[i] = offset;
        }
    }
    
    std::vector<std::pair<int,int>> children_ids;

    n_entries -= transposed_entries;
    ten_percent = n_entries /10;
    {
        std::lock_guard<std::mutex> g {mtx};
        logger_ << "PartlyTransposedDA[" << daid_ << "][children_ids] start" << Logger::endi;
    }

    for(int i = 0; i < n_entries; ++i){
        entry = file.next();
        if(i == 0){
            children_ids.resize(entry.n_children);
        }

        if((i+1) % ten_percent == 0){
            std::lock_guard<std::mutex> g {mtx};
            logger_ << "PartlyTransposedDA[" << daid_ << "][children_ids] " << (i+1)/ten_percent << "0% done." << Logger::endi;
            logger_ << "PartlyTransposedDA[" << daid_ << "][children_ids] " << n_slot_used << "/" << most_right << " (size)" << Logger::endi;
        }
        if(entry.words[0] % datotal_ == daid_ && entry.n_children != 0){
            if(entry.n_children > children_ids.size()){
                children_ids.resize(entry.n_children);
            }

            for(size_t j = 0; j < entry.n_children; ++j){
                reader >> ni;
                children_ids[j] = std::make_pair(ni.word, ni.node_id);
                assert(entry.unique_id == ni.parent_id);
            }
            int index = entry.unique_id;
            assert(da_offset_[index] == -1);
            da_offset_[index] = det_base(children_ids, head, index, entry.n_children);
        }else{
            for(size_t j = 0; j < entry.n_children; ++j){
                reader >> ni;
            }
        }
    }

    resize_array(max_slot+1);
    {
        std::lock_guard<std::mutex> g {mtx};
        logger_ << "DA:: " << daid_ << "   :size: " << da_array_.size() << "   head::" << head << Logger::endi;
    }
}

void leaf_last(std::vector<int>& insert_num, std::vector<std::pair<int,int>> children_ids[]){

}


void PartlyTransposedDA::resize_array(int n_array){
    DASimple dummy;
    dummy.base = -1;
    dummy.check = -1;

    da_array_.resize(n_array, dummy);

    array_size_ = n_array;
}

void PartlyTransposedDA::read_transposed(File::TrieNodeFileReader& file, BinaryFileReader& reader, std::vector<std::pair<int,int>> transpose_word_nodes[]){
    File::TrieNode entry = file.next();
    File::NodeInfo ni;

    // root
    for(int i = 0; i < entry.n_children; ++i){
        reader >> ni;
        if(ni.word % datotal_ == daid_){
            transpose_word_nodes[ni.word].push_back(std::make_pair(ni.parent_id, ni.node_id));
        }
        assert(entry.unique_id == ni.parent_id);
    }

    for(int i = 1; i < transposed_entries; ++i){
        entry = file.next();

        if(entry.words[0] % datotal_ == daid_){
            for(size_t j = 0; j < entry.n_children; ++j){
                reader >> ni;
                transpose_word_nodes[ni.word].push_back(std::make_pair(ni.parent_id, ni.node_id));
                assert(entry.unique_id == ni.parent_id);
            }
        }else{
            for(size_t j = 0; j < entry.n_children; ++j){
                reader >> ni;
                assert(entry.unique_id == ni.parent_id);
            }
        }
    }
}

void PartlyTransposedDA::read_parents(std::string path, std::vector<std::pair<int,int>> children_ids[], std::vector<std::pair<int,int>> transpose_word_nodes[]){
    File::TrieNodeFileReader file(path);
    BinaryFileReader reader(path + ".sorted_nodes");

    std::size_t n_entries = file.size();
    size_t ten_percent = n_entries / 100;
    File::TrieNode root = file.next();
    File::NodeInfo ni;

    for(size_t i = 0; i < root.n_children; ++i){
        reader >> ni;
        assert(root.unique_id == ni.parent_id);
    }


    for(std::size_t i = 1; i < n_entries; ++i){
        if((i+1) % ten_percent == 0){
            std::lock_guard<std::mutex> g {mtx};
            logger_ << "PartlyTransposedDA[" << daid_ << "][read_children] " << (i+1)/ten_percent << "% done." << Logger::endi;
        }

        File::TrieNode entry = file.next();
        for(int j = 0; j < entry.n_children; ++j){
            reader >> ni;
            assert(entry.unique_id == ni.parent_id);
        }
        assert(entry.unique_id != 1);
        if(entry.words[0] % datotal_ != daid_){
            continue;
        }

        if(entry.parent_id <= transposed_entries){
            transpose_word_nodes[entry.words[entry.n_words-1]].push_back(std::make_pair(entry.parent_id, entry.unique_id));
        }else{
            children_ids[entry.parent_id].push_back(std::make_pair(entry.words[entry.n_words-1], entry.unique_id));
        }
    }
}

bool PartlyTransposedDA::check_pt_offset(const int parent_id, const int offset){
    // transposed entry: parent_id < 0
    if(parent_id >= 0) return true;
    if(pt_offset_used_[offset] != 0)return false;
    pt_offset_used_[offset] = 1;
    return true;
}

int PartlyTransposedDA::det_base(const std::vector<std::pair<int,int>>& node, int& head, const int parent_id, int array_size){
    int base = head;
    int max_right = 0, offset;
    max_right = node[array_size-1].first;
    bool x = false;

    int loop_count = 0;
    
    while(++loop_count > 0){
        bool ok = true;
        offset = base - node[0].first;
        if(offset < 0){
            assert(base < (int)da_array_.size());
            base -= da_array_[base].check;
            if(base >= (int)da_array_.size()) resize_array(da_array_.size()*2);
        }
        if(offset < 0)continue;

        for(size_t i = 0; i < array_size; ++i){
            if((int)da_array_.size() > offset+node[i].first &&  da_array_[offset+node[i].first].check >= 0){
                ok = false;
                break;
            }
        }
        if(ok && offset >= 0 && check_pt_offset(parent_id, offset)){
            int new_array_size = (int)da_array_.size();
            while(new_array_size <= offset+max_right+2) new_array_size *= 2;
            resize_array(new_array_size);
            insert_node(node, offset, parent_id, head, array_size);
            break;

        }else{
            assert(base < (int)da_array_.size());
            base -= da_array_[base].check;
            if(base >= (int)da_array_.size()) resize_array(da_array_.size()*2);
        }
    }
    return offset;
}

void PartlyTransposedDA::printNode(const std::vector<std::pair<int,int>>& node, const int parent){
    if(false){
      std::lock_guard<std::mutex> g {mtx};
      logger_ << "node info   ----" << parent << "----  (daid:" << daid_ << ")" << Logger::endi;
      for(size_t i = 0; i < node.size(); ++i){
          logger_ << "first: " << node[i].first << "  second: " << node[i].second << Logger::endi;
      }
    }
}

void PartlyTransposedDA::insert_node(const std::vector<std::pair<int,int>>& node, const int offset,const int parent_id, int& head, int array_size){
    for(int i = 0; i < array_size; ++i){
        n_slot_used++;
        int pos = offset + node[i].first;
        assert(pos < (int)da_array_.size());
        int next_index = pos - da_array_[pos].check;
        int prev_index = pos + da_array_[pos].base;

        max_slot = std::max(max_slot, pos);
        assert(next_index < (int)da_array_.size());

        //headも更新する
        if( head == pos ){
            da_array_[next_index].base = std::numeric_limits<int>::min();
            head -=da_array_[pos].check;
        }else{
            assert(da_array_[prev_index].base < 0);
            assert(da_array_[next_index].check < 0);
            assert(prev_index < (int)da_array_.size());
            da_array_[prev_index].check += da_array_[pos].check;
            da_array_[next_index].base += da_array_[pos].base;
            assert(da_array_[prev_index].base < 0);
            assert(da_array_[next_index].check < 0);
        }

        assert(da_array_[pos].base < 0);
        assert(da_array_[pos].check < 0);

        most_right = std::max(most_right,pos);
        da_n_pos_[node[i].second] = pos;
        if(parent_id < 0){
            //transposed
            da_array_[pos].base = node[i].second;
            da_array_[pos].check = node[i].first;
        }else{
            //children_ids
            da_array_[pos].base = node[i].second;
            da_array_[pos].check = parent_id;
        }

    }
}


void PartlyTransposedDA::dump(DALM::BinaryFileWriter& writer){
    writer << daid_;
    writer << datotal_;
    writer << array_size_;
    writer << bow_max_;
    writer << rightmost_base_;
    writer << rightmost_bow_;
    for(int i = 0; i < rightmost_base_; ++i)  writer << da_array_[i].base;
    for(int i = 0; i < array_size_; ++i)  writer << da_array_[i].check;
    for(int i = 0; i < array_size_; ++i)  writer << val_array_[i].logp;
    for(int i = 0; i < rightmost_bow_; ++i)  writer << val_array_[i].bow;

    writer << transposed_entries;
    writer << word_offset_size_;
    writer.write_many(transposed_offset.get(), word_offset_size_); 
    {
        std::lock_guard<std::mutex> g {mtx};
        logger_ << "PartlyTransposedDA[" << daid_ << "] da-size: " << array_size_  << " ::slot_used:: " << n_slot_used << Logger::endi;
    }
  
}

void PartlyTransposedDA::export_to_text(DALM::TextFileWriter& writer){
    writer << daid_ << "\n";
    writer << datotal_ << "\n";
    writer << array_size_;
}
void PartlyTransposedDA::write_and_free(DALM::BinaryFileWriter& writer){
    writer.write_many(da_array_.data(), (std::size_t) array_size_);
    writer.write_many(val_array_.data(), (std::size_t) val_array_.size());
}

void PartlyTransposedDA::reload(DALM::BinaryFileReader&){}

float PartlyTransposedDA::get_prob(VocabId* words, unsigned char order){
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
        if(next >= check_.size() || (CHECK_BITS&check_[next]) != pos){
            break;
        }
        level = index;
        pos = next;
        prob = prob_[pos];
        if(prob>0){
          prob = -prob;
          break;
        }
        if(base_.size() > pos) offset = base_[pos];
        else offset = 1000000000;
    }

    
    if(order > 1){
        PartlyTransposedDA* bow_da = da_[words[1]%datotal_];
        pos = 0;
        
        offset = bow_da->base_[pos];
        for(int index = 1; index < order; ++index){
            if(offset < 0){
                next = abs(offset) + bow_da->transposed_offset[words[index]];
            }else{
                next = offset + words[index];
            }
            if(next >= bow_da->base_.size() || (CHECK_BITS&bow_da->check_[next]) != pos){
                break;
            }

            pos = next;
            if(bow_da->base_.size() > pos) offset = bow_da->base_[pos];
            else offset = 1000000000;

            if(index > level){
                if(!(bow_da->check_[pos] & INDEPENDENT_RIGHT_BITS)){
                    if(bow_da->bow_.size() > pos) bow += bow_da->bow_[pos];
                }
            }
        }
    }
    return prob + bow;
}

float PartlyTransposedDA::get_prob(VocabId word, DALM::State& state){
    unsigned char count = state.get_count();
    VocabId last_word = word;
    if(count > 0) last_word = state.get_word(count-1);
    state.push_word(word);
    state.set_count(0);

    int pos = 0;
    int offset = base_[pos];
    int next;
    float prob = 0.0f;
    float bow = 0.0f;

    for(int depth = 0; depth < count; ++depth){
        if(offset < 0){
            next = abs(offset) + transposed_offset[state.get_word(depth)];
        }else{
            next = offset + state.get_word(depth);
        }
        if(next >= (int)check_.size() || (CHECK_BITS&check_[next]) != pos){
            bow += sum_bows(state, depth, count);
            return prob + bow;
        }
        pos = next;
        prob = prob_[pos];
        if((int)base_.size() > pos) offset = base_[pos];
        else offset = 1000000000;

        bow = state.get_bow(depth);

        //independent right
        if(check_[pos] & INDEPENDENT_RIGHT_BITS){
          state.set_bow(depth, 0.0f);
        }else{
          if((int)bow_.size() > pos)state.set_bow(depth, bow_[pos]);
          else state.set_bow(depth, 0);
          state.set_count((unsigned char)(depth + 1));
        }

        //independent left
        if(prob>0){
          bow += sum_bows(state, depth+1, count);
          prob = -prob;
          return prob + bow;
        }
    }


    // case for the last word.
    if(offset < 0){
        next = abs(offset) + transposed_offset[last_word];
    }else{
        next = offset + last_word;
    }
    if(next >= (int)check_.size() || (CHECK_BITS&check_[next]) != pos){
        // independent_left=true;
        if(count==0){
            // emulate KenLM
            state.set_count(1);
            state.set_bow(0, 0.0f);
        }
        if(prob>0) prob = -prob;
        return prob + bow;
    }
    pos = next;
    prob = prob_[pos];
    if((int)base_.size() > pos) offset = base_[pos];
    else offset = 1000000000;

    if(count < context_size && !(check_[pos]&INDEPENDENT_RIGHT_BITS)){
      if((int)bow_.size() > pos)state.set_bow(count, bow_[pos]);
      else state.set_bow(count, 0);
      state.set_count((unsigned char)(count + 1));
    }

    if(prob > 0) return -prob;
    return prob;
}


void PartlyTransposedDA::init_state(unsigned int* words, unsigned char order , DALM::State& state){

    int pos = 0;
    int offset = base_[pos];
    int next;
    float prob = 0;
    float bow = 0.0f;
    state.set_count(0);

    for(int index = 0; index < order; ++index){
        if(offset < 0){
            next = abs(offset) + transposed_offset[words[index]];
        }else{
            next = offset + words[index];
        }
        if(next >= (int)check_.size() || (CHECK_BITS&check_[next]) != pos){
            break;
        }
        pos = next;
        prob = prob_[pos];
        state.set_word(index, words[index]);

        if(check_[pos] & INDEPENDENT_RIGHT_BITS){
            state.set_bow(index, 0.0f);
        }else{
            if((int)bow_.size() > pos)state.set_bow(index, bow_[pos]);
            else state.set_bow(index, 0);
            state.set_count((unsigned char)(index+1));
        }

        if(prob>0){
          break;
        }
        if((int)base_.size() > pos) offset = base_[pos];
        else offset = 1000000000;
    }
}

float PartlyTransposedDA::get_prob(unsigned int word, DALM::State& state, DALM::Fragment& f){
    unsigned char count = state.get_count();
    VocabId last_word = word;
    if(count > 0) last_word = state.get_word(count-1);
    state.push_word(word);
    state.set_count(0);

    int pos = 0;
    int offset = base_[pos];
    int next;
    float prob = 0.0f;
    float bow = 0.0f;

    f.status.word = word;
    for(int depth = 0; depth < count; ++depth){
        if(offset < 0){
            next = abs(offset) + transposed_offset[state.get_word(depth)];
        }else{
            next = offset + state.get_word(depth);
        }
        if(next >= (int)check_.size() || (CHECK_BITS&check_[next]) != pos){
            bow += sum_bows(state, depth, count);
            f.status.pos = pos;
            return -(prob + bow);
        }
        pos = next;
        prob = prob_[pos];
        if((int)base_.size() > pos) offset = base_[pos];
        else offset = 1000000000;

        bow = state.get_bow(depth);

        //independent right
        if(check_[pos] & INDEPENDENT_RIGHT_BITS){
          state.set_bow(depth, 0.0f);
        }else{
          if((int)bow_.size() > pos)state.set_bow(depth, bow_[pos]);
          else state.set_bow(depth, 0);
          state.set_count((unsigned char)(depth + 1));
        }

        //independent left
        if(prob>0){
          bow += sum_bows(state, depth+1, count);
          f.status.pos = pos;
          prob = -prob;
          return -(prob + bow);
        }
    }


    // case for the last word.
    if(offset < 0){
        next = abs(offset) + transposed_offset[last_word];
    }else{
        next = offset + last_word;
    }
    if(next >= (int)check_.size() || (CHECK_BITS&check_[next]) != pos){
        // independent_left=true;
        if(count==0){
            state.set_count(1);
            state.set_bow(0, 0.0f);
        }
        f.status.pos = pos;
        return -(prob + bow);
    }
    pos = next;
    prob = prob_[pos];

    if(count < context_size && !(check_[pos]&INDEPENDENT_RIGHT_BITS)){
      if((int)bow_.size() > pos)state.set_bow(count, bow_[pos]);
      else state.set_bow(count, 0);
      state.set_count((unsigned char)(count + 1));
    }

    f.status.pos = pos;
    return prob;
}
float PartlyTransposedDA::get_prob(DALM::Fragment const& f, DALM::State& state, DALM::Gap& gap){
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
    float prob_prev = prob_[pos];

    for(; depth < count; ++depth){
        if(offset < 0){
            next = abs(offset) + transposed_offset[state.get_word(depth)];
        }else{
            next = offset + state.get_word(depth);
        }
        if(next >= (int)check_.size() || (CHECK_BITS&check_[next]) != pos){
            bow += sum_bows(state, depth, count);
            independent_left = true;
            return prob_[pos] + bow - prob_prev;
        }
        extended = true;

        pos = next;
        if((int)base_.size() > pos) offset = base_[pos];
        else offset = 1000000000;

        bow = state.get_bow(depth);

        //independent right
        if(check_[pos] & INDEPENDENT_RIGHT_BITS){
          state.set_bow(depth, 0.0f);
        }else{
          if((int)bow_.size() > pos)state.set_bow(depth, bow_[pos]);
          else state.set_bow(depth, 0);
          state.set_count((unsigned char)(depth + 1));
        }

        //independent left
        if(prob_[pos]>0){
          bow += sum_bows(state, depth+1, count);
          independent_left = true;
          return -prob_[pos] + bow - prob_prev;
        }
    }


    // case for the last word.
    if(offset < 0){
        next = abs(offset) + transposed_offset[last_word];
    }else{
        next = offset + last_word;
    }
    if(next >= (int)check_.size() || (CHECK_BITS&check_[next]) != pos){
        // independent_left=true;
        independent_left = true;
        return prob_[pos] + bow - prob_prev;
    }
    pos = next;
    extended = true;

    if(count < context_size && !(check_[pos]&INDEPENDENT_RIGHT_BITS)){
      if(bow_.size() > pos)state.set_bow(count, bow_[pos]);
      else state.set_bow(count, 0);
      state.set_count((unsigned char)(count + 1));
    }
    independent_left = true;

    if(prob_[pos] > 0)return -prob_[pos] + bow - prob_prev;
    else return prob_[pos] + bow - prob_prev;
}
float PartlyTransposedDA::get_prob(DALM::Fragment const& f, DALM::State& state, DALM::Gap& gap, DALM::Fragment& fnew){
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
    float prob_prev = prob_[pos];

    float prob = 0.0f;

    for(; depth < count; ++depth){
        if(offset < 0){
            next = abs(offset) + transposed_offset[state.get_word(depth)];
        }else{
            next = offset + state.get_word(depth);
        }
        if(next >= (int)check_.size() || (CHECK_BITS&check_[next]) != pos){
            bow += sum_bows(state, depth, count);
            independent_left = true;
            fnew.status.pos = pos;
            return prob + bow - prob_prev;
        }
        extended = true;

        pos = next;
        prob = prob_[pos];
        if((int)base_.size() > pos) offset = base_[pos];
        else offset = 1000000000;

        bow = state.get_bow(depth);

        //independent right
        if(check_[pos] & INDEPENDENT_RIGHT_BITS){
          state.set_bow(depth, 0.0f);
        }else{
          if((int)bow_.size() > pos)state.set_bow(depth, bow_[pos]);
          else state.set_bow(depth, 0);
          state.set_count((unsigned char)(depth + 1));
        }

        //independent left
        if(prob>0){
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
    if(next >= (int)check_.size() || (CHECK_BITS&check_[next]) != pos){
        // independent_left=true;
        independent_left = true;
        fnew.status.pos = pos;
        return prob + bow - prob_prev;
    }
    pos = next;
    prob = prob_[pos];
    extended = true;

    if(count < context_size && !(check_[pos]&INDEPENDENT_RIGHT_BITS)){
      if((int)bow_.size() > pos)state.set_bow(count, bow_[pos]);
      else state.set_bow(count, 0);
      state.set_count((unsigned char)(count + 1));
    }
    independent_left = true;

    fnew.status.pos = pos;
    if(prob > 0)prob = -prob;
    return prob + bow - prob_prev;
}

float PartlyTransposedDA::update(unsigned int, DALM::Fragment&, bool&, bool&, bool&, float&){
  throw std::runtime_error("Not implemented");
}

unsigned long int PartlyTransposedDA::get_state(unsigned int*, unsigned char){
    logger_ << "ReverseDA does not support get_state()." << Logger::endc;
    throw "unsupported";
}

bool PartlyTransposedDA::checkinput(unsigned short n, unsigned int* ngram, float bow, float prob, bool bow_presence){
    float prob_state;
    float bow_state;

    int pos = 0;
    int offset = base_[pos];
    int next;

    for(int index = 0; index < n; ++index){
        if(offset < 0){
            next = abs(offset) + transposed_offset[ngram[n - index - 1]];
        }else{
            next = offset + ngram[n-index-1];
        }
        if(next >= check_.size() || (CHECK_BITS&check_[next]) != pos){
          logger_ << "PartlyTransposedDA[" << daid_ << "] transition error!" << Logger::ende;
          return false;
        }
        pos = next;
        if(base_.size() > pos) offset = base_[pos];
        else offset = 1000000000;
    }


    if(bow_.size() > pos) bow_state = bow_[pos];
    else bow_state = 0;
    prob_state = -fabs(prob_[pos]);

    if(std::isnan(prob_state) || std::isnan(bow_state) || prob != prob_state || std::abs(bow - bow_state) > 0.001){
        logger_ << "PartlyTransposedDA[" << daid_ << "] value error! answer of (prob, bow) is (" << prob << "," << bow << ") but stored is (" << prob_state << "," << bow_state << ")" << Logger::ende;
        return false;
    }

    return true;
}
  

void PartlyTransposedDA::fill_inserted_node(std::string& path_to_tree, DALM::Vocabulary& vocab){
    DAVal dummy;
    dummy.logp = -std::numeric_limits<float>::infinity();
    dummy.bow = -std::numeric_limits<float>::infinity();
    val_array_.resize(da_array_.size(), dummy);
    std::vector<int> independent_rights;
    da_n_pos_[0] = 0;
    da_n_pos_[1] = 0;

    File::TrieNodeFileReader file(path_to_tree);
    std::size_t n_entries = file.size();
    int ten_percent = n_entries / 10;
    for(std::size_t i = 0; i < n_entries; ++i){
        File::TrieNode entry = file.next();

        if((i+1) % ten_percent == 0){
            std::lock_guard<std::mutex> g {mtx};
            logger_ << "PartlyTransposedDA[" << daid_ << "][fill_inserted_node] " << (i+1)/ten_percent << "0% done." << Logger::endi;
        }
        if(entry.unique_id == 1 || entry.words[0] % datotal_ != daid_){
            continue;
        }
        int ind = da_n_pos_[entry.unique_id];

        assert(ind >= 0);

        if(entry.independent_left) val_array_[ind].logp = -entry.prob.value;
        else val_array_[ind].logp = entry.prob.value;

        val_array_[ind].bow = entry.bow.value;
        if(entry.independent_right){
            independent_rights.push_back(ind);
        }

    }
    rightmost_bow_ = 0;
    rightmost_base_ = 0;

    for(int i = 0; i < (int)da_array_.size(); ++i){
        if(da_array_[i].base <= transposed_entries){
            da_array_[i].base = -da_array_[i].base;
        }else{
            da_array_[i].base = da_offset_[da_array_[i].base];
        }
        da_array_[i].check = da_n_pos_[da_array_[i].check];
        if(da_array_[i].base != -1) rightmost_base_ = i + 1;
        if(fabs(val_array_[i].bow) != 0.0f) rightmost_bow_ = i + 1;
    }
    
    // independent lefts
    for(int i = 0; i < (int)independent_rights.size(); ++i) da_array_[i].check |= INDEPENDENT_RIGHT_BITS;


    {
        std::lock_guard<std::mutex> g {mtx};
        logger_ << "PartlyTransposedDA[" << daid_ << "][da_size] " << da_array_.size() << Logger::endi;
        logger_ << "PartlyTransposedDA[" << daid_ << "][val_siz] " << val_array_.size() << Logger::endi;
        logger_ << "PartlyTransposedDA[" << daid_ << "][rightmost_base_] " << rightmost_base_ << Logger::endi;
        logger_ << "PartlyTransposedDA[" << daid_ << "][rightmost_bow_] " << rightmost_bow_ << Logger::endi;
        logger_ << "PartlyTransposedDA[" << daid_ << "][base][0] " << da_array_[0].base << Logger::endi;
    }
}

void PartlyTransposedDA::init_state(DALM::State& state, DALM::State const& state_prev, DALM::Fragment const* fragment, DALM::Gap const& gap){
    for(int i = 0; i < state_prev.get_count(); ++i){
        state.set_bow(i, state_prev.get_bow(i));
    }
}


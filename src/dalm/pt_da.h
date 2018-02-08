#ifndef QUAIL_DECODER_PARTLY_TRANSPOSED_DA_H
#define QUAIL_DECODER_PARTLY_TRANSPOSED_DA_H

#include <cmath>
#include <limits>
#include <mutex>
#include <thread>
#include <memory>
#include <unordered_map>
#include "bst_da.h"
#include "da.h"

namespace DALM{
  const int INDEPENDENT_RIGHT_BITS = 0x80000000;
  const int CHECK_BITS = 0x7fffffff;

class PartlyTransposedDA : public DA{
public:
    PartlyTransposedDA(unsigned char daid, unsigned char datotal, PartlyTransposedDA **neighbours, Logger &logger, int transpose);
    PartlyTransposedDA(BinaryFileReader &reader, PartlyTransposedDA **neighbours, unsigned char order, Logger &logger);
    PartlyTransposedDA(Logger &logger);
    virtual ~PartlyTransposedDA();

    virtual void make_da(std::string &path_to_tree_file, ValueArrayIndex *value_array_index, Vocabulary &vocab);
    virtual void dump(BinaryFileWriter &writer);
    virtual void export_to_text(TextFileWriter &writer);

    virtual void write_and_free(BinaryFileWriter &writer);
    virtual void reload(BinaryFileReader& reader);

    virtual float get_prob(VocabId *word, unsigned char order);
    virtual float get_prob(VocabId word, State &state);
    virtual float get_prob(VocabId word, State &state, Fragment &F);
    virtual float get_prob(const Fragment &F, State &state, Gap &gap);
    virtual float get_prob(const Fragment &fprev, State &state, Gap &gap, Fragment &fnew);
    virtual float update(VocabId word, Fragment &F, bool &extended, bool &independent_left, bool &independent_right, float &bow);

    virtual void init_state(VocabId *word, unsigned char order, State &state);
    virtual void init_state(State &state, const State &state_prev, const Fragment *fragments, const Gap &cap);

    virtual unsigned long int get_state(VocabId *word, unsigned char order);

    virtual bool checkinput(unsigned short n, unsigned int *ngram, float bow, float prob, bool bow_presence);
    virtual void fill_inserted_node(std::string &path_to_tree_file, Vocabulary &vocab);

protected:
    int det_base(const std::vector<std::pair<int,int>>& node, int& head, const int parent_id, int array_size);
    void insert_node(const std::vector<std::pair<int,int>>& node, const int offset, const int parent_id, int& head, int array_size);
    bool check_pt_offset(const int parent_id, const int offset);
    int traverse(VocabId word, int context);
    int endmarker_node(int context);
    void resize_array(int n_array);

    void replace_value(float *&values);
    void fit_to_size(int max_slot);
    void read_transposed(File::TrieNodeFileReader& file, BinaryFileReader& reader, std::vector<std::pair<int,int>> transpose_word_nodes[]);
    void read_parents(std::string path, std::vector<std::pair<int,int>> children_ids[], std::vector<std::pair<int,int>> transpose_word_nodes[]);
    virtual void printNode(const std::vector<std::pair<int,int>>& node, const int parent_id);

    static std::mutex mtx;
    std::unordered_map<int,int> pt_offset_used_;
    long array_size_;
    std::vector<DASimple> da_array_;
    std::unique_ptr<int[]> transposed_offset;

    unsigned int daid_;
    unsigned int datotal_;
    int most_right;
    int rightmost_bow_;
    int rightmost_base_;
    int transposed_entries;
    float bow_max_;
    int max_slot = 0;
    int n_slot_used = 0;
    int word_offset_size_ = 0;
    Logger &logger_;
public:
    static std::vector<int> da_offset_;
    static std::vector<int> da_n_pos_;

private:
    std::vector<int> base_;
    std::vector<int> check_;
    std::vector<float> prob_;
    std::vector<float> bow_;
    PartlyTransposedDA **da_;
    std::vector<DAVal> val_array_;
};

class PartlyTransposedDAHandler : public DAHandler{
public:
    int transpose;
    PartlyTransposedDAHandler(){}
    PartlyTransposedDAHandler(int tp):transpose(tp){}
    PartlyTransposedDAHandler(BinaryFileReader &reader, unsigned char order, Logger &logger){
        reader >> danum;
        da = new DA*[danum];
        for(size_t i = 0; i < danum; ++i){
            da[i] = new PartlyTransposedDA(reader, (PartlyTransposedDA **) da, order, logger);
        }
    }
    
    virtual ~PartlyTransposedDAHandler(){}

    void build(unsigned int danum, std::string path_to_arpa, std::string &path_to_node_file, Vocabulary &vocab,
                std::string &tmpdir, unsigned int n_cores, Logger &logger){
        DAHandler::danum=danum;
        da = new DA*[danum];
        DABuilder **builder = new DABuilder*[danum];

        for(size_t i = 0; i < danum; i++){
            da[i] = new PartlyTransposedDA(i, danum, (PartlyTransposedDA**)da, logger, transpose);
            builder[i] = new DABuilder(da[i], path_to_node_file, NULL, vocab, tmpdir);
        }

        logger << "[PartlyTransposedDAHandler::build] Building Double Array." << Logger::endi;
        run(builder, n_cores, logger);

        for(size_t i = 0; i < danum; i++){
            delete builder[i];
        }
        delete [] builder;

        NodeFiller **fixer = new NodeFiller *[danum];
        for(size_t i = 0; i < danum; i++){
            fixer[i] = new NodeFiller(da[i], path_to_node_file, vocab);
        }
        size_t threads = PThreadWrapper::thread_available(n_cores);
        logger << "[PartlyTransposedDAHandler::build] threads available=" << threads << Logger::endi;
        size_t running = 0;
        for(size_t i = 0; i < danum; i++){
            fixer[i]->start();
            running++;
            if(running>=threads){
                while(true){
                    usleep(500);
                    size_t runcounter = 0;
                    for(size_t j = 0; j <= i; j++){
                        if(!fixer[j]->is_finished()) runcounter++;
                    }
                    if(runcounter<threads){
                        running = runcounter;
                        break;
                    }
                }
            }
        }
        for(size_t i = 0; i < danum; i++){
            fixer[i]->join();
            delete fixer[i];
        }
        delete [] fixer;
        logger << "[PartlyTransposedDAHandler::build] Building Double Array." << Logger::endi;
    }

    virtual float sum_bows(State &state, unsigned char begin, unsigned char end){
        return da[0]->sum_bows(state, begin, end);
    }

protected:
    virtual size_t get_daid(VocabId *ngram, size_t n){
        return ngram[0]%danum;
    }

    virtual size_t get_daid(VocabId word, State &state){
        return word%danum;
    }
    
    virtual size_t get_daid(const Fragment &f, State &state_prev, Gap &gap){
        return f.status.word%danum;
    }

    virtual size_t get_daid(const Fragment &f){
        return f.status.word%danum;
    }

    virtual size_t get_daid(State &state, const State &state_prev){
        if(state_prev.has_context())
            return state_prev.get_word(0)%danum;
        else
            return 0;
    }

    virtual size_t get_state_daid(VocabId *ngram, size_t n){
        return ngram[0]%danum;
    }

    virtual size_t get_errorcheck_daid(VocabId *ngram, size_t n){
        return ngram[n-1]%danum;
    }

    virtual void dump_method_dependents(BinaryFileWriter & fp){}
};

}// namespace DALM
#endif //QUAIL_DECODER_PARTLY_TRANSPOSED_DA_H

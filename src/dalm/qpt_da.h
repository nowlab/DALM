#ifndef QUAIL_DECODER_QUANTIZATION_PARTLY_TRANSPOSED_DA_H
#define QUAIL_DECODER_QUANTIZATION_PARTLY_TRANSPOSED_DA_H

#include "pt_da.h"
#include "da.h"


namespace DALM{


template<typename PROB, typename BOW>
class QuantPartlyTransposedDA : public PartlyTransposedDA{
public:
    QuantPartlyTransposedDA(unsigned char daid, unsigned char datotal, QuantPartlyTransposedDA<PROB,BOW> **neighbours, Logger &logger, int transpose);
    QuantPartlyTransposedDA(BinaryFileReader &reader, QuantPartlyTransposedDA<PROB,BOW> ** neighbours, unsigned char order, Logger &logger);
    float get_prob(VocabId *word, unsigned char order) override;
    float get_prob(VocabId word, State &state) override;
    float get_prob(VocabId word, State &state, Fragment& F) override;
    float get_prob(const Fragment &F, State &state, Gap &gap) override;
    float get_prob(const Fragment &fprev, State &state, Gap &gap, Fragment &fnew) override;
    void init_state(VocabId *word, unsigned char order, State &state) override;
    void fill_inserted_node(std::string &path_to_tree_file, Vocabulary &vocab) override;
    void dump(BinaryFileWriter &writer) override;
    void write_and_free(BinaryFileWriter &writer) override;
    


private:
    QuantPartlyTransposedDA<PROB, BOW> **da_;
    std::vector<int> base_;
    std::vector<int> check_;
    std::vector<PROB> prob_;
    std::vector<BOW> bow_;
};

template<typename PROB, typename BOW>
class QuantPartlyTransposedDAHandler : public PartlyTransposedDAHandler{
public:
  int transpose;
    QuantPartlyTransposedDAHandler(int tp):transpose(tp){}


    QuantPartlyTransposedDAHandler(BinaryFileReader &reader, unsigned char order, Logger &logger){

        reader >> danum;
        da = new DA*[danum];
        for(size_t i = 0; i < danum; ++i){
            da[i] = new QuantPartlyTransposedDA<PROB,BOW>(reader, (QuantPartlyTransposedDA<PROB,BOW> **) da, order, logger);
        }
    }

    void build(unsigned int danum, std::string path_to_arpa, std::string &path_to_node_file, Vocabulary &vocab,
                std::string &tmpdir, unsigned int n_cores, Logger &logger){
        DAHandler::danum=danum;
        da = new DA*[danum];
        DABuilder **builder = new DABuilder*[danum];

        for(size_t i = 0; i < danum; i++){
            da[i] = new QuantPartlyTransposedDA<PROB,BOW>(i, danum, (QuantPartlyTransposedDA<PROB,BOW>**)da, logger, transpose);
            builder[i] = new DABuilder(da[i], path_to_node_file, NULL, vocab, tmpdir);
        }

        logger << "[QuantPartlyTransposedDAHandler::build] Building Double Array." << Logger::endi;
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
        logger << "[QuantPartlyTransposedDAHandler::build] threads available=" << threads << Logger::endi;
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
        logger << "[QuantPartlyTransposedDAHandler::build] Building Double Array." << Logger::endi;
    }
};
}

#include "../qpt_da.cpp"

#endif //QUAIL_DECODER_QUANTIZATION_PARTLY_TRANSPOSED_DA_H

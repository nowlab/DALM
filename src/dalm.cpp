#include <cmath>

#include <unistd.h>
#include <vector>
#include <glob.h>
#include <sys/stat.h>

#include "dalm.h"
#include "dalm/embedded_da.h"
#include "dalm/reverse_da.h"
#include "dalm/bst_da.h"
#include "dalm/pt_da.h"
#include "dalm/qpt_da.h"

namespace DALM{
  namespace QUANT_INFO{
    int prob_bits;
    int bow_bits;
    int independent_left_mask;
    int prob_mask;
    std::vector<float> prob_bin[DALM_MAX_ORDER];
    std::vector<float> bow_bin[DALM_MAX_ORDER];
  }
}

using namespace DALM;

LM::LM(std::string pathtoarpa, std::string pathtotree, Vocabulary &vocab, unsigned int dividenum, unsigned int opt, std::string tmpdir, unsigned int n_cores, Logger &logger, int transpose, int prob_bit, int bow_bit)
        :vocab(vocab), v(Version(opt, logger)), logger(logger) {
    logger << "[LM::LM] TEXTMODE begin." << Logger::endi;
    logger << "[LM::LM] pathtoarpa=" << pathtoarpa << Logger::endi;
    logger << "[LM::LM] pathtotree=" << pathtotree << Logger::endi;

    build(pathtoarpa, pathtotree, dividenum, tmpdir, n_cores, transpose, prob_bit, bow_bit);
    std::string stagstart = "<s>";
    stagid = vocab.lookup(stagstart.c_str());
    logger << "[LM::LM] end." << Logger::endi;
}

LM::LM(std::string dumpfilepath,Vocabulary &vocab, unsigned char order, Logger &logger)
    :vocab(vocab), logger(logger){

    logger << "[LM::LM] BINMODE begin." << Logger::endi;
    logger << "[LM::LM] dumpfilepath=" << dumpfilepath << Logger::endi;

    BinaryFileReader reader(dumpfilepath);
    v = Version(reader, logger); // Version check.
    readParams(reader, order);
    std::string stagstart = "<s>";
    stagid = vocab.lookup(stagstart.c_str());
    logger << "[LM::LM] end." << Logger::endi;
}

LM::~LM() {
    delete handler;
}

float LM::query(VocabId *ngram, unsigned char n){
    return handler->get_prob(ngram, n);
}

float LM::query(VocabId word, State &state){
    return handler->get_prob(word, state);
}

float LM::query(VocabId word, State &state, Fragment &f){
    return handler->get_prob(word, state, f);
}

float LM::query(const Fragment &f, State &state_prev, Gap &gap){
    return handler->get_prob(f, state_prev, gap);
}

float LM::query(const Fragment &fprev, State &state_prev, Gap &gap, Fragment &fnew){
    return handler->get_prob(fprev, state_prev, gap, fnew);
}

float LM::update(VocabId word, Fragment &f, bool &extended, bool &independent_left, bool &independent_right, float &bow) {
    return handler->update(word, f, extended, independent_left, independent_right, bow);
}

void LM::init_state(State &state){
    handler->init_state(&stagid, 1, state);
}

void LM::set_state(VocabId *ngram, unsigned char n, State &state){
    handler->init_state(ngram, n, state);
}

void LM::set_state(State &state, const State &state_prev, const Fragment *fragments, const Gap &gap){
    handler->init_state(state, state_prev, fragments, gap);
}

/* depricated */
StateId LM::get_state(VocabId *ngram, unsigned char n){
    for(unsigned char i=0;i<n;i++){
        if(ngram[i] == vocab.unk()){
            n=i;
            break;
        }
    }
    return handler->get_state(ngram, n);
}

std::size_t LM::errorcheck(std::string &pathtoarpa){
    ARPAFile *arpafile = new ARPAFile(pathtoarpa, vocab);
    logger << "[LM::errorcheck] start." << Logger::endi;
    std::size_t error_num = 0;
    size_t total = arpafile->get_totalsize();
    for(size_t i=0;i<total;i++){
        unsigned short n;
        VocabId *ngram;
        float prob;
        float bow;
        bool bow_presence = arpafile->get_ngram(n,ngram,prob,bow);
    
        bool builderror = handler->errorcheck(n, ngram, bow, prob, bow_presence);
        if(!builderror){
            logger << "[LM::build] Error: line=" << i << Logger::ende;
            error_num++;
        }
        delete [] ngram;
    }
    delete arpafile;
    logger << "[LM::build] # of errors: " << error_num << Logger::endi;
    return error_num;
}

void LM::build(std::string &pathtoarpa, std::string &pathtotreefile, unsigned int dividenum, std::string &tmpdir, unsigned int n_cores, int transposed, int prob_bit, int bow_bit) {
    logger << "[LM::build] LM build begin." << Logger::endi;

    if(v.get_opt()==DALM_OPT_REVERSE){
        handler = new ReverseDAHandler();
    }else if(v.get_opt()==DALM_OPT_EMBEDDING){
        handler = new EmbeddedDAHandler();
    }else if(v.get_opt()== DALM_OPT_BST){
        handler = new BstDAHandler();
    }else if(v.get_opt()== DALM_OPT_PT){
        handler = new PartlyTransposedDAHandler(transposed);
    }else if(v.get_opt()== DALM_OPT_QPT){

      if(prob_bit == 0) prob_bit = 32;
      if(bow_bit == 0) bow_bit = 32;

        if(prob_bit < 8){
            if(bow_bit < 9){
                handler = new QuantPartlyTransposedDAHandler<char,char>(transposed);
                logger << "[LM::build][Quant] char char " << Logger::endc;
            }else if(bow_bit < 17){
                handler = new QuantPartlyTransposedDAHandler<char,short>(transposed);
                logger << "[LM::build][Quant] char short" << Logger::endc;
            }else{
                handler = new QuantPartlyTransposedDAHandler<char,int>(transposed);
                logger << "[LM::build][Quant] char int " << Logger::endc;
            }
        }else if(prob_bit < 16){
            if(bow_bit < 9){
                handler = new QuantPartlyTransposedDAHandler<short,char>(transposed);
                logger << "[LM::build][Quant] short char " << Logger::endc;
            }else if(bow_bit < 17){
                handler = new QuantPartlyTransposedDAHandler<short,short>(transposed);
                logger << "[LM::build][Quant] short short " << Logger::endc;
            }else{
                handler = new QuantPartlyTransposedDAHandler<short,int>(transposed);
                logger << "[LM::build][Quant] short int " << Logger::endc;
            }
        }else{
            if(bow_bit < 9){
                handler = new QuantPartlyTransposedDAHandler<int,char>(transposed);
                logger << "[LM::build][Quant] int char " << Logger::endc;
            }else if(bow_bit < 17){
                handler = new QuantPartlyTransposedDAHandler<int,short>(transposed);
                logger << "[LM::build][Quant] int short " << Logger::endc;
            }else{
                handler = new QuantPartlyTransposedDAHandler<int,int>(transposed);
                logger << "[LM::build][Quant] int int " << Logger::endc;
            }
        }
    }else{
        logger << "[LM::build] DALM unknown type." << Logger::endc;
        throw "type error";
    }
    handler->build(dividenum, pathtoarpa, pathtotreefile, vocab, tmpdir, n_cores, logger);

    logger << "[LM::build] LM build end." << Logger::endi;
}

void LM::dumpParams(BinaryFileWriter &writer){
    handler->dump(writer);
}
void LM::exportParams(TextFileWriter &writer){
    handler->export_to_text(writer);
}

void LM::readParams(BinaryFileReader &reader, unsigned char order){
    if(v.get_opt() == DALM_OPT_REVERSE){
        handler = new ReverseDAHandler(reader, order, logger);
    }else if(v.get_opt() == DALM_OPT_EMBEDDING){
        handler = new EmbeddedDAHandler(reader, order, logger);
    }else if(v.get_opt() == DALM_OPT_BST){
        handler = new BstDAHandler(reader, order, logger);
    }else if(v.get_opt()== DALM_OPT_PT){
        handler = new PartlyTransposedDAHandler(reader, order, logger);
    }else if(v.get_opt()== DALM_OPT_QPT){
        reader >> DALM::QUANT_INFO::prob_bits;
        reader >> DALM::QUANT_INFO::bow_bits;

        int prob_bit = DALM::QUANT_INFO::prob_bits;
        int bow_bit = DALM::QUANT_INFO::bow_bits;

        for(int i = 0; i < DALM_MAX_ORDER-1; ++i){
            DALM::QUANT_INFO::prob_bin[i].resize(1<<prob_bit, 0);
            DALM::QUANT_INFO::bow_bin[i].resize(1<<bow_bit, 0);
            reader.read_many(DALM::QUANT_INFO::prob_bin[i].data(), 1<<prob_bit);
            reader.read_many(DALM::QUANT_INFO::bow_bin[i].data(), 1<<bow_bit);
        }


        QUANT_INFO::independent_left_mask = 1 << prob_bit;
        QUANT_INFO::prob_mask = (1<<prob_bit)-1;


        if(prob_bit < 8){
            if(bow_bit < 9){
                handler = new QuantPartlyTransposedDAHandler<char,char>(reader, order, logger);
                logger << "[LM::build][Quant] char char " << Logger::endc;
            }else if(bow_bit < 17){
                handler = new QuantPartlyTransposedDAHandler<char,short>(reader, order, logger);
                logger << "[LM::build][Quant] char short " << Logger::endc;
            }else{
                handler = new QuantPartlyTransposedDAHandler<char,int>(reader, order, logger);
                logger << "[LM::build][Quant] char int " << Logger::endc;
            }
        }else if(prob_bit < 16){
            if(bow_bit < 9){
                handler = new QuantPartlyTransposedDAHandler<short,char>(reader, order, logger);
                logger << "[LM::build][Quant] short char " << Logger::endc;
            }else if(bow_bit < 17){
                handler = new QuantPartlyTransposedDAHandler<short,short>(reader, order, logger);
                logger << "[LM::build][Quant] short short " << Logger::endc;
            }else{
                handler = new QuantPartlyTransposedDAHandler<short,int>(reader, order, logger);
                logger << "[LM::build][Quant] short int " << Logger::endc;
            }
        }else{
            if(bow_bit < 9){
                handler = new QuantPartlyTransposedDAHandler<int,char>(reader, order, logger);
                logger << "[LM::build][Quant] int char " << Logger::endc;
            }else if(bow_bit < 17){
                handler = new QuantPartlyTransposedDAHandler<int,short>(reader, order, logger);
                logger << "[LM::build][Quant] int short " << Logger::endc;
            }else{
                handler = new QuantPartlyTransposedDAHandler<int,int>(reader, order, logger);
                logger << "[LM::build][Quant] int int " << Logger::endc;
            }
        }
    }else{
        logger << "[LM::build] DALM unknown type." << Logger::endc;
        throw "type error";
    }
}

void Model::load(){
    struct stat s;
    if(stat(modeldir.c_str(), &s) != 0){
        throw std::runtime_error("Model Directory Not Found.");
    }
    std::string ini_path = modeldir + "/dalm.ini";
    std::ifstream ifs(ini_path.c_str());
    std::string line;

    std::getline(ifs, line);
    while(!ifs.eof()){
        unsigned long pos = line.find("=");
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos+1, line.size()-pos);
        if(key=="MODEL"){
            model = value;
        }else if(key=="WORDS"){
            words = value;
        }else if(key=="WORDSTXT"){
            wordstxt = value;
        }else if(key=="ORDER"){
            if(order != 0){
                assert(order == std::atoi(value.c_str()));
            }else{
                order = (unsigned char)std::atoi(value.c_str());
            }
        }
        std::getline(ifs, line);
    }

    model = modeldir + "/" + model;
    words = modeldir + "/" + words;
    wordstxt = modeldir + "/" + wordstxt;

    vocab = new Vocabulary(words, logger);
    lm = new LM(model, *vocab, order, logger);
    vocab_file = new VocabularyFile(wordstxt);
}

Model::Model(std::string modeldir, unsigned char order, Logger &logger): order(order), modeldir(modeldir), logger(logger) {
    load();
}

Model::Model(std::string modeldir, Logger &logger): modeldir(modeldir), logger(logger){
    order = 0;
    load();
}

float LM::sum_bows(State &state, unsigned char begin, unsigned char end) {
    return handler->sum_bows(state, begin, end);
}

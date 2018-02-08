#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>

#include <string>
#include <iostream>
#include <limits>

#include "dalm.h"
#include "dalm/sortutil.h"
#include "dalm/buildutil.h"
#include "dalm/qpt_da.h"

namespace DALM {
    int File::TrieNode::static_unique_id = 0;
    int File::NodeInfo::tp = 0;
    std::vector<int> PartlyTransposedDA::da_offset_;
    std::vector<int> PartlyTransposedDA::da_n_pos_;

    struct ARPAEntryOrderCompare {
        bool operator()(File::ARPAEntry const &e1, File::ARPAEntry const &e2) const {
            std::size_t n = std::min(e1.n, e2.n);
            for(std::size_t i = 0; i < n; ++i){
                if(e1.words[i] < e2.words[i]) return true;
                else if(e1.words[i] > e2.words[i]) return false;
            }
            return e1.n < e2.n;
        }
    };

    template<typename T>
    struct DefaultCompare{
        bool operator()(T const &a, T const &b) const {
            return a < b;
        }
    };

    struct DefaultPairRCompare{
        bool operator()(std::pair<float,int> const &a, std::pair<float,int> const &b) const {
            return a > b;
        }
    };

    struct DefaultPairCompare{
        template<typename T, typename S>
        bool operator()(std::pair<T,S> const &a, std::pair<T,S> const &b) const {
            return a < b;
        }
    };

    struct ARPAEntryTrieOrderCompare {
        bool operator()(File::ARPAEntry const &e1, File::ARPAEntry const &e2) const{
            assert(e1.n != 0);
            assert(e2.n != 0);

            // compare context
            bool cmp = std::lexicographical_compare(
                    e1.words, e1.words + (e1.n-1),
                    e2.words, e2.words + (e2.n-1));
            if(cmp) return true;

            cmp = std::lexicographical_compare(
                    e2.words, e2.words + (e2.n-1),
                    e1.words, e1.words + (e1.n-1));
            if(cmp) return false;

            assert(e1.n == e2.n);
            return e1.words[(uint32_t)(e1.n-1)] < e2.words[(uint32_t)(e2.n-1)];

        }
    };
    struct ARPAEntryLexicographicOrderCompare {
        bool operator()(File::ARPAEntry const &e1, File::ARPAEntry const &e2) const{
            assert(e1.n != 0);
            assert(e2.n != 0);

            // compare context
            return std::lexicographical_compare(
                    e1.words, e1.words + e1.n,
                    e2.words, e2.words + e2.n);

        }
    };

    struct TrieNodeNumberOfChildrenCompare {
        bool operator()(File::TrieNode const &e1, File::TrieNode const &e2) const{
            if(e1.n_children == e2.n_children){
                return e1.unique_id < e2.unique_id;
            }
            return e1.n_children > e2.n_children;
        }
    };

    struct TrieNodeId {
        bool operator()(File::TrieNode const &e1, File::TrieNode const &e2) const{
            return e1.unique_id < e2.unique_id;
        }
    };

    std::size_t sort_reverse_order(
            std::string tempdir, std::string arpafile, BinaryFileWriter &writer, Vocabulary &vocab, Logger &logger, bool fill, std::size_t memory_limit){
#ifndef NDEBUG
        std::size_t vocab_max = vocab.size();
#endif

        logger << "Sorting in Reverse Order." << Logger::endi;
        ARPAFile arpa(arpafile, vocab);
        Sort::Sorter<File::ARPAEntry, ARPAEntryOrderCompare> sorter(tempdir, memory_limit);

        std::size_t n_entry = arpa.get_totalsize();
        logger << "# of entries in ARPA=" << n_entry << Logger::endi;
        for(std::size_t i = 0; i < n_entry; ++i){
            unsigned short order;
            VocabId *ngram;
            float prob, bow;
            bool bow_presence = arpa.get_ngram(order, ngram, prob, bow);

#ifndef NDEBUG
            for(std::size_t j = 0; j < order; ++j){
                assert(ngram[j] != 0);
                assert(ngram[j] <= vocab_max);
            }
#endif

            File::ARPAEntry entry(ngram, (unsigned char)order, prob, bow, bow_presence);
            sorter.push(entry);
            delete [] ngram;
        }

        sorter.freeze();

        logger << "Optimizing Left State" << Logger::endi;
        File::ARPAEntry prev;
        std::fill(prev.words, prev.words + (DALM_MAX_ORDER), 0);
        prev.n = 0;
        prev.prob = -std::numeric_limits<float>::infinity();
        prev.bow = 0.0;
        std::size_t n_inserted = 0;

        for(std::size_t i = 0; i < n_entry; ++i){
            File::ARPAEntry entry = sorter.pop();
            unsigned char common = std::min(prev.n, entry.n);
            for(unsigned char j = 0; j < common; ++j){
                if(prev.words[j] != entry.words[j]){
                    common = j;
                    break;
                }
            }

            if(prev.n != 0){
                if(common < prev.n){
                    // independent left
                    prev.prob = -prev.prob;
                }
                writer << prev;
            }

            if(fill){
                for(unsigned char j = (unsigned char)(common + 1); j < entry.n; ++j){
                    assert(j != 0);
                    writer << File::ARPAEntry(entry.words, j, -std::numeric_limits<float>::infinity(), -0.0f, false, false);
                    ++n_inserted;
                }
            }
            prev = entry;
        }

        //must be independent left.
        prev.prob = -prev.prob;
        writer << prev;

        logger << "# of total N-grams=" << sorter.size() + n_inserted << Logger::endi;

        return sorter.size() + n_inserted;
    }

    void sort_forward_order(BinaryFileReader &reader, std::size_t n_entries, BinaryFileWriter &writer, std::string tempdir, std::size_t memory_limit, Logger &logger){
        logger << "Sorting in Forward Order." << Logger::endi;
        Sort::Sorter<File::ARPAEntry, ARPAEntryOrderCompare> sorter(tempdir, memory_limit);
        for(std::size_t i = 0; i < n_entries; ++i){
            File::ARPAEntry rev_entry;
            reader >> rev_entry;

            // reverse entry.
            File::ARPAEntry entry(rev_entry.words, rev_entry.n, rev_entry.prob, rev_entry.bow, rev_entry.bow_presence);
            sorter.push(entry);
        }
        sorter.freeze();

        logger << "Optimizing Right State" << Logger::endi;
        File::ARPAEntry prev;
        std::fill(prev.words, prev.words + DALM_MAX_ORDER, 0);
        prev.n = 0;
        prev.prob = -std::numeric_limits<float>::infinity();
        prev.bow = 0.0;
        for(std::size_t i = 0; i < n_entries; ++i){
            File::ARPAEntry entry = sorter.pop();
            if(prev.n != 0){
                if(prev.bow == 0.0){
                    unsigned char common = std::min(prev.n, entry.n);
                    for(unsigned char j = 0; j < common; ++j){
                        if(prev.words[j] != entry.words[j]){
                            common = j;
                            break;
                        }
                    }

                    if(common < prev.n){
                        // optimizing right state.
                        prev.bow = -0.0;
                    }
                }
                writer << prev;
            }

            prev = entry;
        }
        if(prev.bow == 0.0) {
            prev.bow = -0.0;
        }
        writer << prev;
    }

    void sort_lexicographic_order(BinaryFileReader &reader, std::size_t n_entries, BinaryFileWriter &writer, std::string tempdir, Logger &logger, std::size_t memory_limit){
        logger << "Sorting in Lexicographic Order." << Logger::endi;
        Sort::Sorter<File::ARPAEntry, ARPAEntryLexicographicOrderCompare> sorter(tempdir, memory_limit);
        for(std::size_t i = 0; i < n_entries; ++i){
            File::ARPAEntry entry;
            reader >> entry;

            // reverse entry.
            File::ARPAEntry rev_entry(entry.words, entry.n, entry.prob, entry.bow, entry.bow_presence);
            //rev_entry.prob = -std::numeric_limits<float>::infinity(); //ignored
            //rev_entry.bow = -std::numeric_limits<float>::infinity(); //ignored
            sorter.push(rev_entry);

        }
        sorter.freeze();

        logger << "Writing data." << Logger::endi;
        writer << sorter.size();
        for(std::size_t i = 0; i < sorter.size(); ++i){
            File::ARPAEntry entry = sorter.pop();
            writer << entry;
        }
    }


    struct NodeInfoCompare {
        bool operator()(File::NodeInfo const &e1, File::NodeInfo const &e2) const {
            if( e1.parent_id == e2.parent_id){
                if(e1.parent_id > File::NodeInfo::tp){
                    return e1.word < e2.word;
                }
                return e1.node_id < e2.node_id;
            }
            return e1.parent_id < e2.parent_id;
        }
    };

    void sort_parent_node_id(std::string path, std::string tempdir, int memory_limit){
        Sort::Sorter<File::NodeInfo, NodeInfoCompare> sorter(tempdir, memory_limit);
        BinaryFileReader reader(path);
        BinaryFileWriter writer(path+".sorted_nodes");
        std::size_t n_entries;
        reader >> n_entries;
        File::TrieNode root;
        reader >> root;
        File::TrieNode node;
        
        for(size_t i = 1; i < n_entries; ++i){
            reader >> node;
            File::NodeInfo ni(node.parent_id, node.unique_id, node.words[node.n_words-1], node.n_words);
            sorter.push(ni);
        }
        sorter.freeze();
        size_t n_total = sorter.size();
        for(std::size_t i = 0; i < n_total; ++i){
            File::NodeInfo ni = sorter.pop();
            writer << ni;
        }
    }

    void sort_rev_order(BinaryFileReader &reader, std::size_t n_entries, BinaryFileWriter &writer, std::string tempdir, Logger &logger, std::size_t memory_limit, bool need_endmarker=true){
        logger << "Sorting in Reverse Order." << Logger::endi;
        Sort::Sorter<File::ARPAEntry, ARPAEntryTrieOrderCompare> sorter(tempdir, memory_limit);
        for(std::size_t i = 0; i < n_entries; ++i){
            File::ARPAEntry entry;
            reader >> entry;

            // reverse entry.
            File::ARPAEntry rev_entry(entry.words, entry.n, entry.prob, entry.bow, entry.bow_presence);
            if(need_endmarker){
                File::ARPAEntry endmarker_entry = rev_entry;
                endmarker_entry.words[endmarker_entry.n] = 1;
                ++endmarker_entry.n;
                sorter.push(endmarker_entry);
            }

            rev_entry.prob = -std::numeric_limits<float>::infinity(); //ignored
            rev_entry.bow = -std::numeric_limits<float>::infinity(); //ignored
            sorter.push(rev_entry);

        }
        sorter.freeze();

        logger << "Writing data." << Logger::endi;
        writer << sorter.size();
        for(std::size_t i = 0; i < sorter.size(); ++i){
            File::ARPAEntry entry = sorter.pop();
            writer << entry;
        }
    }

    void sort_rev_order(BinaryFileReader &reader, std::size_t n_entries, std::string outpath, std::string tempdir, Logger &logger, std::size_t memory_limit, bool need_endmarker=true){
        BinaryFileWriter writer(outpath);
        sort_rev_order(reader, n_entries, writer, tempdir, logger, memory_limit, need_endmarker);
    }


    //partlyTransposedDA
    void sort_pt_order(BinaryFileReader &reader, std::size_t n_entries, std::string outpath, std::string tempdir, Logger &logger, std::size_t memory_limit, int tp){
        logger << "Sort by Number of Children." << Logger::endi;
        File::TrieNode node;

        Sort::Sorter<File::TrieNode, TrieNodeNumberOfChildrenCompare> sorter(tempdir, memory_limit);
        for(size_t i = 0; i < n_entries; ++i){
            reader >> node;
            sorter.push(node);
        }

        sorter.freeze();
        std::vector<int> new_ids;
        new_ids.resize(n_entries+1, -1);
        PartlyTransposedDA::da_offset_.resize(n_entries+2, -1);
        PartlyTransposedDA::da_n_pos_.resize(n_entries+2, -1);
        {
            logger << "Writing data. " << Logger::endi;
            BinaryFileWriter tmp(outpath+".tmp");
            tmp << sorter.size();
            for(size_t i = 0; i < sorter.size(); ++i){
                node = sorter.pop();
                tmp << node;
                assert(new_ids[node.unique_id] == -1); 
                new_ids[node.unique_id] = i;
            }
            assert(new_ids[0] == 0);
        }
        int gc = 0; // have grandchild
        int bc = 0; // have bow_child
        int nc = 0; // have no bow child
        int mo = 0; // max_order

        {
            BinaryFileWriter writer(outpath + ".tmp2");
            BinaryFileReader read(outpath + ".tmp");
            size_t n;
            read >> n;
            writer << n;
            for(size_t i = 0; i < n; ++i){
                read >> node;
                node.unique_id = new_ids[node.unique_id];
                node.parent_id = new_ids[node.parent_id];

                if(node.n_words > 1)assert(node.parent_id != 0);
                writer << node;

                if(node.unique_id < tp || node.has_grandchild){
                    gc++;
                }else if(!node.has_grandchild && node.children_has_bow){
                    bc++;
                }else if( node.n_words+1 == DALM_MAX_ORDER){
                    mo++;
                }else{
                    nc++;
                }
            }
        }

        logger << "Sort by Insert Order." << Logger::endi;
        int gcc = 1;
        int bcc = 0;
        int ncc = 0;
        int moc = 0;
        {
            File::TrieNodeFileReader file(outpath + ".tmp2");
            std::size_t n_entries = file.size();
            logger << "n_entries: " << n_entries << Logger::endi;
            File::TrieNode root = file.next();
            new_ids[0] = 0;
            
            for(std::size_t i = 1; i < n_entries; ++i){
                File::TrieNode entry = file.next();
                if(entry.unique_id < tp || entry.has_grandchild){
                    new_ids[i] = gcc;
                    gcc++;
                }else if(!entry.has_grandchild && entry.children_has_bow){
                    new_ids[i] = gc + bcc;
                    bcc++;
                }else if( node.n_words+1 == DALM_MAX_ORDER){
                    new_ids[i] = gc + bc + nc + moc;
                    moc++;
                }else{
                    new_ids[i] = gc + bc + ncc;
                    ncc++;
                }
            }
        }

        logger << "Sort by Insert Order. (push)" << Logger::endi;
        logger << "gc:bc:nc:mo ->" << gc << " : " << bc << " : " << nc << " : " << mo << Logger::endi;
        Sort::Sorter<File::TrieNode, TrieNodeId> s(tempdir, memory_limit);
        {
            BinaryFileReader read(outpath + ".tmp2");
            size_t n;
            read >> n;
            for(size_t i = 0; i < n; ++i){
                read >> node;
                // 1-indexedに
                node.unique_id = new_ids[node.unique_id] + 1;
                node.parent_id = new_ids[node.parent_id] + 1;

                if(node.n_words > 1)assert(node.parent_id != 0);
                s.push(node);
            }
        }
        s.freeze();
        logger << "Sort by Insert Order. (pop)" << Logger::endi;
        {
            BinaryFileWriter writer(outpath);
            size_t si = s.size();
            writer << si;
            for(size_t i = 0; i < si; ++i){
                File::TrieNode node = s.pop();
                writer << node;
            }
        }

        unlink((outpath + ".tmp").c_str());
        unlink((outpath + ".tmp2").c_str());
    }


    void sort_bst_order(BinaryFileReader &reader, std::size_t n_entries, std::string outpath, std::string tempdir, Logger &logger, std::size_t memory_limit){
        logger << "Sorting in BST Order." << Logger::endi;
        Sort::Sorter<File::ARPAEntry, ARPAEntryTrieOrderCompare> sorter(tempdir, memory_limit);
        File::ARPAEntry endmarker;
        endmarker.n = 1;
        endmarker.words[0] = 1;
        endmarker.prob = -std::numeric_limits<float>::infinity(); //ignored
        endmarker.bow = 0.0f;
        sorter.push(endmarker);
        for(std::size_t i = 0; i < n_entries; ++i){
            File::ARPAEntry entry;
            reader >> entry;

            // reverse entry.
            File::ARPAEntry prob_entry;
            for(std::size_t j = 0; j + 1 < entry.n; ++j) {
                prob_entry.words[j] = entry.words[entry.n - j - 2];
            }
            prob_entry.words[entry.n-1] = 1; //<#>
            prob_entry.words[entry.n] = entry.words[entry.n-1];
            prob_entry.n = entry.n+1;
            prob_entry.prob = entry.prob;
            prob_entry.bow = entry.bow; //ignored
            sorter.push(prob_entry);

            if(entry.bow_presence){
                File::ARPAEntry reverse_entry(entry.words, entry.n, entry.prob, entry.bow, entry.bow_presence);
                File::ARPAEntry bow_entry = reverse_entry;
                bow_entry.words[reverse_entry.n] = 1; // <#>
                bow_entry.n = reverse_entry.n + 1;
                bow_entry.prob = -std::numeric_limits<float>::infinity(); //ignored
                sorter.push(bow_entry);

                reverse_entry.prob = -std::numeric_limits<float>::infinity(); //ignored
                sorter.push(reverse_entry);
            }
        }
        std::size_t n_total = sorter.size();
        sorter.freeze();

        logger << "Writing data." << Logger::endi;
        BinaryFileWriter writer(outpath);
        writer << n_total;
        for(std::size_t i = 0; i < n_total; ++i){
            writer << sorter.pop();
        }
        logger << "Done." << Logger::endi;
    }

    void build_rev(
            std::string tempdir, std::string arpafile, std::string outpath, Vocabulary &vocab, Logger &logger, std::size_t memory_limit, int tp
    ){

        BinaryFileWriter *rev_writer = BinaryFileWriter::get_temporary_writer(tempdir);
        std::size_t n_total = sort_reverse_order(tempdir, arpafile, *rev_writer, vocab, logger, true, memory_limit);

        FILE *fp = rev_writer->pointer();
        delete rev_writer;
        std::rewind(fp);
        BinaryFileReader rev_reader(fp);

        BinaryFileWriter *forward_writer = BinaryFileWriter::get_temporary_writer(tempdir);
        sort_forward_order(rev_reader, n_total, *forward_writer, tempdir, memory_limit, logger);

        fp = forward_writer->pointer();
        delete forward_writer;
        std::rewind(fp);
        BinaryFileReader forward_reader(fp);
        sort_rev_order(forward_reader, n_total, outpath, tempdir, logger, memory_limit);
    }

    void build_bst(
            std::string tempdir, std::string arpafile, std::string outpath, Vocabulary &vocab, Logger &logger, std::size_t memory_limit, int tp 
    ){

        BinaryFileWriter *rev_writer = BinaryFileWriter::get_temporary_writer(tempdir);
        std::size_t n_total = sort_reverse_order(tempdir, arpafile, *rev_writer, vocab, logger, true, memory_limit);

        FILE *fp = rev_writer->pointer();
        delete rev_writer;
        std::rewind(fp);
        BinaryFileReader rev_reader(fp);

        BinaryFileWriter *forward_writer = BinaryFileWriter::get_temporary_writer(tempdir);
        sort_forward_order(rev_reader, n_total, *forward_writer, tempdir, memory_limit, logger);

        fp = forward_writer->pointer();
        delete forward_writer;
        std::rewind(fp);
        BinaryFileReader forward_reader(fp);
        sort_bst_order(forward_reader, n_total, outpath, tempdir, logger, memory_limit);
    }

    int node_builder(BinaryFileReader &reader, std::size_t n_entries, std::string outpath, std::string tempdir, Logger &logger, std::size_t memory_limit){
        BinaryFileWriter writer(outpath);
        logger << "transform ARPAEntry to TrieNode." << Logger::endi;
        std::vector<File::TrieNode> parents;
        File::ARPAEntry arpa;
        int node_count = 0;
        size_t size;
        reader >> size;

        //root node
        parents.push_back( File::TrieNode{} );
        assert(parents[0].unique_id == 0);


        for(size_t i = 0; i < n_entries; ++i){
            reader >> arpa;
            File::TrieNode node{arpa};
            while( !parents.back().is_ancestor_of(node)){
                writer << parents.back();
                parents.pop_back();
                node_count++;
            }
            while( !parents.back().is_parent_of(node)){
                File::TrieNode tmp{parents.back().words, parents.back().n_words};
                tmp.add_word(node.words[parents.back().n_words]);
                tmp.parent_id = parents.back().unique_id;
                parents.push_back(tmp);
                assert(parents.size() < DALM_MAX_ORDER+1);
            }
            parents.back().add_child(node.unique_id);
            node.parent_id = parents.back().unique_id;
            if(node.n_words > 1) assert(node.parent_id != 0);

            if(node.bow_presence) parents.back().children_has_bow = true;

            parents.push_back(node);
            for(size_t i = 0; i < parents.size() - 2; ++i){
                parents[i].has_grandchild = true;
            }
        }
        while(!parents.empty()){
            writer << parents.back();
            parents.pop_back();
            node_count++;
        }
        logger << "Done. (node_counter is " << node_count << ")" << Logger::endi;
        return node_count;
    }



    void build_pt(
            std::string tempdir, std::string arpafile, std::string outpath, Vocabulary &vocab, Logger &logger, std::size_t memory_limit, int tp
    ){

        File::NodeInfo::tp = tp;
        BinaryFileWriter *rev_writer = BinaryFileWriter::get_temporary_writer(tempdir);
        std::size_t n_total = sort_reverse_order(tempdir, arpafile, *rev_writer, vocab, logger, true, memory_limit);

        FILE *fp = rev_writer->pointer();
        delete rev_writer;
        std::rewind(fp);
        BinaryFileReader rev_reader(fp);

        BinaryFileWriter *forward_writer = BinaryFileWriter::get_temporary_writer(tempdir);
        sort_forward_order(rev_reader, n_total, *forward_writer, tempdir, memory_limit, logger);

        fp = forward_writer->pointer();
        delete forward_writer;
        std::rewind(fp);
        BinaryFileReader forward_reader(fp);

        BinaryFileWriter *reverse_writer = BinaryFileWriter::get_temporary_writer(tempdir);
        sort_lexicographic_order(forward_reader, n_total, *reverse_writer, tempdir, logger, memory_limit);

        fp = reverse_writer->pointer();
        delete reverse_writer;
        std::rewind(fp);
        BinaryFileReader reverse_trie_reader(fp);
        int num_node;
        num_node = node_builder(reverse_trie_reader, n_total, outpath+".node", tempdir, logger, memory_limit);

        {
            BinaryFileReader node_reader(outpath+".node");
            logger << "new reader" << Logger::endi;
            // qunatization
            if(tp < 0)
                sort_pt_order(node_reader, num_node, outpath, tempdir, logger, memory_limit, -(tp+1));
            else
                sort_pt_order(node_reader, num_node, outpath, tempdir, logger, memory_limit, tp);

            if(tp >= 0)  sort_parent_node_id(outpath, tempdir, memory_limit);
        }
        unlink((outpath + ".node").c_str());

    }

    void binning(std::string outpath, std::string tempdir, size_t memory_limit, Logger &logger){

        std::vector<Sort::Sorter<std::pair<float, int>, DefaultPairCompare >> bow_sorters;
        std::vector<Sort::Sorter<std::pair<float, int>, DefaultPairCompare >> prob_sorters;
        Sort::Sorter<std::pair<int, int>, DefaultPairCompare > bow_index(tempdir, memory_limit);
        Sort::Sorter<std::pair<int, int>, DefaultPairCompare > prob_index(tempdir, memory_limit);

        for(int i = 0; i < DALM_MAX_ORDER-1; ++i){
            bow_sorters.push_back(Sort::Sorter<std::pair<float, int>, DefaultPairCompare >(tempdir, memory_limit));
            prob_sorters.push_back(Sort::Sorter<std::pair<float, int>, DefaultPairCompare >(tempdir, memory_limit));
        }
        


        logger << "binning" << Logger::endi;
        size_t n;
        BinaryFileReader reader(outpath+".before_bin");
        reader >> n;
        File::TrieNode node;
        std::pair<float, int> entry;

        for(size_t i = 0; i < n; ++i){
            reader >> node;
            size_t order = node.n_words;
            if(order > 0)order--;
            prob_sorters[order].push(std::make_pair(node.prob.value, i));
            if(fabs(node.bow.value) == 0.0f){
                bow_index.push(std::make_pair(i, 0));
            }else{
                bow_sorters[order].push(std::make_pair(node.bow.value, i));
            }
        }

        for(int i = 0; i < DALM_MAX_ORDER-1; ++i){
            
            logger << "[binning] prob" << i << " " << prob_sorters[i].size() << Logger::endi;
            prob_sorters[i].freeze();
            if(i != DALM_MAX_ORDER -2){
                logger << "[binning] bow " << i << " " << bow_sorters[i].size() << Logger::endi;
                bow_sorters[i].freeze();
            }
        }

        logger << "[binning] bin freeze" << Logger::endi;

        {
            float prob_tmp;
            std::vector<float> probs[DALM_MAX_ORDER];

            for(int order = 0; order < DALM_MAX_ORDER-1; ++order){
                logger << "[binning] prob sort(" << order << ")" << Logger::endi;
                int nn = (int)prob_sorters[order].size();
                {
                    BinaryFileWriter prob_writer(outpath + ".prob");

                    for(size_t i = 0; i < prob_sorters[order].size(); ++i){
                        prob_writer << prob_sorters[order].pop();
                    }
                    prob_writer << std::make_pair(std::numeric_limits<float>::infinity(), -1);
                }

                logger << "[binning] prob uniq1" << Logger::endi;
                {
                    BinaryFileReader prob_reader(outpath + ".prob");
                    prob_reader >> entry;
                    probs[order].push_back(entry.first);
                    for(size_t i = 1; i < prob_sorters[order].size(); ++i){
                        prob_reader >> entry;
                        if(probs[order].back() != entry.first){
                            probs[order].push_back(entry.first);
                        }
                    }
                }
                {
                    BinaryFileReader prob_reader(outpath + ".prob");
                    int prob_bins = 1 << DALM::QUANT_INFO::prob_bits;
                    int prob_d = probs[order].size()/prob_bins;
                    int prob_mod = probs[order].size()%prob_bins;
                    int probs_index = -1;
                    logger << "probs size " << probs[order].size() << Logger::endi;
                    // index, bin_index
                    logger << "[binning] make prob bin(" << order << ")" << Logger::endi;
                    prob_reader >> entry;
                    for(int i = 0; i < prob_bins; ++i){
                        probs_index += prob_d;
                        if( i < prob_mod) probs_index++;
                        int cnt = 0;
                        prob_tmp = 0.0f;

                        while(nn){
                            //entry = prob_sorter.pop();
                            if(entry.first > probs[order][probs_index]) break;
                            nn--;
                            cnt++;
                            prob_tmp += entry.first;
                            prob_index.push(std::make_pair(entry.second, i));
                            prob_reader >> entry;
                        }
                        DALM::QUANT_INFO::prob_bin[order][i] = prob_tmp / cnt;
                        assert(cnt != 0);
                    }
                    logger << "prob_bin nn " << nn << Logger::endi;
                }
                unlink((outpath + ".prob").c_str());
            }
        }
        {
            float bow_tmp;
            int nn = 0;
            std::vector<float> bows[DALM_MAX_ORDER];

            for(int order = 0; order < DALM_MAX_ORDER-2; ++order){
                nn = bow_sorters[order].size();
                int bow_n = bow_sorters[order].size();
                {
                    BinaryFileWriter bow_writer(outpath + ".bow");

                    for(size_t i = 0; i < bow_n; ++i){
                        bow_writer << bow_sorters[order].pop();
                    }
                    bow_writer << std::make_pair(std::numeric_limits<float>::infinity(), -1);
                }

                logger << "[binning] prob uniq" << Logger::endi;
                {
                    BinaryFileReader bow_reader(outpath + ".bow");
                    bow_reader >> entry;
                    bows[order].push_back(entry.first);
                    for(size_t i = 1; i < bow_n; ++i){
                        bow_reader >> entry;
                        if(bows[order].back() != entry.first){
                            bows[order].push_back(entry.first);
                        }
                    }
                }
                
                {
                    BinaryFileReader bow_reader(outpath + ".bow");
                    
                    int bow_bins = 1 << DALM::QUANT_INFO::bow_bits;
                    bow_bins--; //0 -> 0
                    int bow_d = bows[order].size()/bow_bins;
                    int bow_mod = bows[order].size()%bow_bins;
                    int bows_index = 0;
                    logger << "bows size " << bows[order].size() << Logger::endi;
                    // index, bin_index
                    logger << "[binning] make bow bin" << Logger::endi;
                    bow_reader >> entry;
                    DALM::QUANT_INFO::bow_bin[order][0] = 0;
                    for(int i = 1; i <= bow_bins; ++i){
                        bows_index += bow_d;
                        if( i < bow_mod) bows_index++;
                        int cnt = 0;
                        bow_tmp = 0.0f;

                        while(nn){
                            if(entry.first > bows[order][bows_index]) break;
                            nn--;
                            cnt++;
                            bow_tmp += entry.first;
                            bow_index.push(std::make_pair(entry.second, i));
                            bow_reader >> entry;
                        }
                        assert(cnt != 0);
                        DALM::QUANT_INFO::bow_bin[order][i] = bow_tmp / cnt;
                    }
                    logger << "bow_bin nn " << nn << Logger::endi;
                }
                unlink((outpath + ".bow").c_str());
            }
        }

        bow_index.freeze();
        prob_index.freeze();

        BinaryFileReader update(outpath+".before_bin");
        BinaryFileWriter writer(outpath);
        logger << "[binning] update nodes" << Logger::endi;
        logger << "[binning] update nodes prob " << prob_index.size() << Logger::endi;
        logger << "[binning] update nodes bow  " << bow_index.size() << Logger::endi;

        update >> n;
        writer << n;
        for(size_t i = 0; i < n; ++i){
            update >> node;
            entry = prob_index.pop();
            if(i < 100 ){
                logger << "[binning][update][prob][" << i << "]" << entry.first << Logger::endi;
                logger << "[binning][update][prob][" << i << "]" << entry.second << Logger::endi;
                if(node.n_words != 0)
                logger << "[binning][update][prob][" << i << "]" << DALM::QUANT_INFO::prob_bin[node.n_words-1][entry.second] << Logger::endi;
                logger << "[binning][update][prob][" << i << "]" << node.prob.value << Logger::endi;
            }
            node.prob.index = entry.second;
            entry = bow_index.pop();
            if(i < 100 ){
                logger << "[binning][update][bow][" << i << "]" << entry.first << Logger::endi;
                logger << "[binning][update][bow][" << i << "]" << entry.second << Logger::endi;
                if(node.n_words != 0)
                logger << "[binning][update][bow][" << i << "]" << DALM::QUANT_INFO::bow_bin[node.n_words-1][entry.second] << Logger::endi;
                logger << "[binning][update][bow][" << i << "]" << node.bow.value << Logger::endi;
            }
            node.bow.index = entry.second;
            writer << node;
        }

    }



    void build_qpt(std::string tempdir, std::string arpafile, std::string outpath, Vocabulary &vocab, Logger &logger, std::size_t memory_limit, int tp){
        build_pt(tempdir, arpafile, outpath+".before_bin", vocab, logger, memory_limit, -tp-1);
        logger << "qpt" << Logger::endi;
        binning(outpath, tempdir, memory_limit, logger);
        unlink((outpath + ".before_bin").c_str());
        if(tp >= 0)  sort_parent_node_id(outpath, tempdir, memory_limit);
    }

    void build_words(char const *arpa, char const *worddict, char const *wordids, bool need_endmarker){
        std::vector<std::string> words;
        std::vector<VocabId> ids;
        ARPAFile::create_vocab_and_order(arpa, words, ids, need_endmarker);

        std::ofstream wfs(worddict);
        std::ofstream ifs(wordids);
        for(std::size_t i = 0; i < words.size(); ++i){
            wfs << words[i] << std::endl;
            ifs << ids[i] << std::endl;
        }
        wfs.close();
        ifs.close();
    }

    std::size_t build_DALM_core(std::string opt, std::string arpa, std::string outdir, unsigned char n_div, int n_cores, size_t memory_limit, Logger &logger, int transpose, int prob_bit, int bow_bit) {
        unsigned int method;

        bool need_endmarker;
        void (*builder)(std::string, std::string, std::string, Vocabulary &, Logger &, std::size_t, int);
        std::string worddict = outdir + "/words.txt";
        std::string wordids = outdir + "/ids.txt";
        std::string word_da = outdir + "/words.bin";

        std::string optmethod(opt);
        std::string trie = outdir + "/trie.bin";

        if(optmethod=="embedding"){
            method = DALM_OPT_EMBEDDING;
            throw std::runtime_error("currently disabled");
        }else if(optmethod=="reverse") {
            method = DALM_OPT_REVERSE;
            need_endmarker = true;
            builder = build_rev;
        }else if(optmethod=="bst"){
            method = DALM_OPT_BST;
            need_endmarker = true;
            builder = build_bst;
        }else if(optmethod=="pt"){
            need_endmarker = false;
            if(prob_bit == 0 &&bow_bit == 0){
                method = DALM_OPT_PT;
                builder = build_pt;
            }else{
                method = DALM_OPT_QPT;
                builder = build_qpt;
                DALM::QUANT_INFO::prob_bits = prob_bit;
                DALM::QUANT_INFO::bow_bits = bow_bit;
                DALM::QUANT_INFO::independent_left_mask = 1 << prob_bit;
                DALM::QUANT_INFO::prob_mask = (1<<prob_bit)-1;
                for(int i = 0; i < DALM_MAX_ORDER-1; ++i){
                    DALM::QUANT_INFO::prob_bin[i].resize(1<<prob_bit,0);
                    DALM::QUANT_INFO::bow_bin[i].resize(1<<bow_bit,0);
                }
            }
        }else if(optmethod=="reversetrie"){
            throw std::runtime_error("removed");
        }else{
            throw std::runtime_error("unknown optimization");
        }

        build_words(arpa.c_str(), worddict.c_str(), wordids.c_str(), need_endmarker);
        Vocabulary vocab(worddict, word_da, wordids, 0, logger);
        builder(outdir, arpa, trie, vocab, logger, memory_limit, transpose);

        logger << "Building DALM." << Logger::endi;
        LM dalm(arpa, trie, vocab, n_div, method, outdir, n_cores, logger, transpose, prob_bit, bow_bit);

        logger << "Dumping DALM." << Logger::endi;
        std::string binmodel = outdir + "/dalm.bin";
        dalm.dump(binmodel);

        logger << "Dumping DALM end." << Logger::endi;
        unlink(trie.c_str());
        if(optmethod == "pt"){
          unlink((outdir + "/trie.bin.sorted_nodes").c_str());
        }

        ARPAFile arpa_file(arpa, vocab);

        return arpa_file.get_ngramorder();
    }

    void write_ini(std::string outdir, std::string arpa, std::size_t order){
        std::string ini = outdir + "/dalm.ini";
        std::ofstream file(ini.c_str());

        file << "TYPE=2" << std::endl;
        file << "MODEL=dalm.bin" << std::endl;
        file << "WORDS=words.bin" << std::endl;
        file << "ARPA=" << arpa << std::endl;
        file << "WORDSTXT=words.txt" << std::endl;
        file << "ORDER=" << order << std::endl;
        file.close();
    }

    void build_dalm(std::string outdir, std::string arpa, std::string opt, unsigned char n_div, int n_cores, std::size_t memory_limit, Logger &logger, int transpose, int prob_bit, int bow_bit) {
        struct stat s;
        if(stat(outdir.c_str(), &s) == 0){
            throw std::runtime_error("directory already exists");
        }
        if(mkdir(outdir.c_str(), 0777) != 0){
            throw std::runtime_error("mkdir faild");
        }

        std::size_t order = build_DALM_core(opt, arpa, outdir, n_div, n_cores, memory_limit, logger, transpose, prob_bit, bow_bit);
        write_ini(outdir, arpa, order);
    }
}

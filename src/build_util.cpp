#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>

#include <string>
#include <iostream>
#include <limits>

#include "dalm.h"
#include "dalm/sortutil.h"
#include "dalm/buildutil.h"

namespace DALM {
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

    void sort_rev_order(BinaryFileReader &reader, std::size_t n_entries, std::string outpath, std::string tempdir, Logger &logger, std::size_t memory_limit){
        logger << "Sorting in Reverse Order." << Logger::endi;
        Sort::Sorter<File::ARPAEntry, ARPAEntryTrieOrderCompare> sorter(tempdir, memory_limit);
        for(std::size_t i = 0; i < n_entries; ++i){
            File::ARPAEntry entry;
            reader >> entry;

            // reverse entry.
            File::ARPAEntry rev_entry(entry.words, entry.n, entry.prob, entry.bow, entry.bow_presence);
            File::ARPAEntry endmarker_entry = rev_entry;

            rev_entry.prob = -std::numeric_limits<float>::infinity(); //ignored
            rev_entry.bow = -std::numeric_limits<float>::infinity(); //ignored
            sorter.push(rev_entry);

            endmarker_entry.words[endmarker_entry.n] = 1;
            ++endmarker_entry.n;
            sorter.push(endmarker_entry);
        }
        sorter.freeze();

        logger << "Writing data." << Logger::endi;
        BinaryFileWriter writer(outpath);
        writer << sorter.size();
        for(std::size_t i = 0; i < sorter.size(); ++i){
            File::ARPAEntry entry = sorter.pop();
            writer << entry;
        }
    }

    void sort_reverse_trie_order(BinaryFileReader &reader, std::size_t n_entries, std::string outpath, std::string tempdir, Logger &logger, std::size_t memory_limit){
        logger << "Sorting in Reverse Trie Order." << Logger::endi;
        Sort::Sorter<File::ARPAEntry, ARPAEntryTrieOrderCompare> sorter(tempdir, memory_limit);
        for(std::size_t i = 0; i < n_entries; ++i){
            File::ARPAEntry entry;
            reader >> entry;

            // reverse entry.
            File::ARPAEntry rev_entry(entry.words, entry.n, entry.prob, entry.bow, entry.bow_presence);
            sorter.push(rev_entry);
        }
        sorter.freeze();

        logger << "Writing data." << Logger::endi;
        BinaryFileWriter writer(outpath);
        writer << n_entries;
        for(std::size_t i = 0; i < n_entries; ++i){
            File::ARPAEntry entry = sorter.pop();
            writer << entry;
        }
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
            std::string tempdir, std::string arpafile, std::string outpath, Vocabulary &vocab, Logger &logger, std::size_t memory_limit
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

    void build_reverse_trie(
            std::string tempdir, std::string arpafile, std::string outpath, Vocabulary &vocab, Logger &logger, std::size_t memory_limit
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
        sort_reverse_trie_order(forward_reader, n_total, outpath, tempdir, logger, memory_limit);
    }

    void build_bst(
            std::string tempdir, std::string arpafile, std::string outpath, Vocabulary &vocab, Logger &logger, std::size_t memory_limit
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

    std::size_t build_DALM_core(std::string opt, std::string arpa, std::string outdir, unsigned char n_div, int n_cores, size_t memory_limit, Logger &logger) {
        unsigned int method;

        bool need_endmarker;
        void (*builder)(std::string, std::string, std::string, Vocabulary &, Logger &, std::size_t);
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
        }else if(optmethod=="reversetrie"){
            method = DALM_OPT_REVERSE_TRIE;
            need_endmarker = false;
            builder = build_reverse_trie;
        }else{
            throw std::runtime_error("unknown optimization");
        }

        build_words(arpa.c_str(), worddict.c_str(), wordids.c_str(), need_endmarker);
        Vocabulary vocab(worddict, word_da, wordids, 0, logger);
        builder(outdir, arpa, trie, vocab, logger, memory_limit);

        logger << "Building DALM." << Logger::endi;
        LM dalm(arpa, trie, vocab, n_div, method, outdir, n_cores, logger);

        logger << "Dumping DALM." << Logger::endi;
        std::string binmodel = outdir + "/dalm.bin";
        dalm.dump(binmodel);

        logger << "Dumping DALM end." << Logger::endi;
        unlink(trie.c_str());
        unlink(wordids.c_str());

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

    void build_dalm(std::string outdir, std::string arpa, std::string opt, unsigned char n_div, int n_cores, std::size_t memory_limit, Logger &logger) {
        struct stat s;
        if(stat(outdir.c_str(), &s) == 0){
            throw std::runtime_error("directory already exists");
        }
        if(mkdir(outdir.c_str(), 0777) != 0){
            throw std::runtime_error("mkdir faild");
        }

        std::size_t order = build_DALM_core(opt, arpa, outdir, n_div, n_cores, memory_limit, logger);
        write_ini(outdir, arpa, order);
    }
}

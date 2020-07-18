#ifndef SORTUTIL_H_
#define SORTUTIL_H_

#include <string>
#include <vector>
#include <queue>
#include <assert.h>
#include <sstream>
#include <cstring>
#include "fileutil.h"

namespace DALM{
    namespace Sort {

        template <typename Target>
        struct MergeObject{
            MergeObject(Target const &target, std::size_t i): target(target), i(i){}

            Target target;
            std::size_t i;
        };

        template <typename Target, typename Compare>
        struct MergeObjectCompare {
            // implement >
            bool operator()(MergeObject<Target> const &t1, MergeObject<Target> const &t2) const{
                Compare c;
                return c(t2.target, t1.target);
            }
        };

        template <typename Target, typename Compare>
        struct CustomGreater {
            bool operator()(Target const &t1, Target const &t2) const{
                Compare c;
                return c(t2, t1);
            }
        };

        template <typename Target, typename Compare>
        class Sorter {
            typedef std::priority_queue<Target, std::vector<Target>, CustomGreater< Target, Compare > > Buffer;
            typedef std::priority_queue<MergeObject<Target>, std::vector< MergeObject<Target> >, MergeObjectCompare<Target, Compare> > Merger;

        public:
            Sorter(std::string tempdir, std::size_t memory_limit): tempdir_(tempdir), size_(0){
                if(memory_limit == 0){
                    throw std::runtime_error("memory_limit = 0");
                }
                n_buffer_max_ = memory_limit / sizeof(Target);
            }

            ~Sorter(){
                std::vector<BinaryFileReader *>::iterator iter = readers_.begin();
                while(iter != readers_.end()){
                    delete *iter;
                    ++iter;
                }
            }

            void push(Target const &obj){
                ++size_;
                if(counters_.size() > 0){
                    throw std::runtime_error("frozen");
                }
                buf_.push(obj);
                if(buf_.size() > n_buffer_max_){
                    write_to_file();
                }
            }

            void freeze() {
                write_to_file();

                assert(!readers_.empty());
                sizes_.reserve(readers_.size());

                std::size_t i = 0;
                std::size_t total = 0;
                std::vector<BinaryFileReader *>::iterator iter = readers_.begin();
                while (iter != readers_.end()) {
                    std::size_t size = 0;
                    **iter >> size;
                    assert(size > 0);
                    sizes_.push_back(size);
                    Target obj;
                    **iter >> obj;

                    total += size;

                    MergeObject<Target> m(obj, i);
                    merger_.push(m);

                    ++iter;
                    ++i;
                }
                counters_.resize(sizes_.size(), 1);
            }

            Target pop(){
                if(merger_.empty()){
                    throw std::runtime_error("empty");
                }
                MergeObject<Target> const &m = merger_.top();
                Target target = m.target;
                std::size_t i = m.i;

                merger_.pop();

                if(counters_[i] < sizes_[i]){
                    Target obj;
                    *readers_[i] >> obj;
                    ++(counters_[i]);

                    MergeObject<Target> m(obj, i);
                    merger_.push(m);
                }
                return target;
            }

            std::size_t size(){
                return size_;
            }

        private:
            void write_to_file(){
                if(buf_.empty()) return;

                BinaryFileWriter *writer = DALM::BinaryFileWriter::get_temporary_writer(tempdir_);

                *writer << buf_.size();
                while(!buf_.empty()){
                    /*
                    File::ARPAEntry t = (File::ARPAEntry)buf_.top();
                    for(std::size_t i = 0; i < t.n; ++i){
                        std::cerr << t.words[i] << " ";
                    }
                    std::cerr << "| ";
                    for(std::size_t i = t.n; i < DALM_MAX_ORDER; ++i){
                        std::cerr << t.words[i] << " ";
                    }
                    std::cerr << t.prob << " " << t.bow << " " << t.bow_presence << std::endl;
                    */

                    *writer << buf_.top();

                    buf_.pop();
                }
                FILE *fp = writer->pointer();
                std::rewind(fp);
                delete writer;
                readers_.push_back(new BinaryFileReader(fp));
            }

            std::string tempdir_;
            std::size_t n_buffer_max_;
            Buffer buf_;
            std::vector<BinaryFileReader *> readers_;
            std::vector<std::size_t> sizes_;
            Merger merger_;
            std::vector<std::size_t> counters_;
            std::size_t size_;
        };
    }
}

#endif //SORTUTIL_H_

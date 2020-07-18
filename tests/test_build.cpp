#include <fstream>
#include "gtest/gtest.h"
#include "dalm_test_config.h"
#include "../src/dalm/trie_reader.h"
#include "../src/dalm/buildutil.h"
#include "../src/dalm.h"
#include "build_dalm_for_test.h"

namespace {
    class BuildTest : public ::testing::Test {
    protected:
        BuildTest(): logger(stderr), test_files(std::string(SOURCE_ROOT) + "/tests/data/test_files.txt"){
            std::ifstream ifs(test_files.c_str());
            std::getline(ifs, arpa);
            arpa = std::string(SOURCE_ROOT) + "/" + arpa;
        }

        virtual void SetUp(){
        }

        virtual void TearDown(){
        }

        DALM::Logger logger;
        std::string test_files;
        std::string arpa;
    };

    TEST_F(BuildTest, test_bst_build){
        DALM::Model *m = build_dalm_for_test("bst_build", "bst", logger, 1);
        ASSERT_EQ(m->lm->errorcheck(arpa), 0);
        delete m;

        m = build_dalm_for_test("bst_build", "bst", logger, 2);
        ASSERT_EQ(m->lm->errorcheck(arpa), 0);
        delete m;

        m = build_dalm_for_test("bst_build", "bst", logger, 3);
        ASSERT_EQ(m->lm->errorcheck(arpa), 0);
        delete m;

        m = build_dalm_for_test("bst_build", "bst", logger, 4);
        ASSERT_EQ(m->lm->errorcheck(arpa), 0);
        delete m;
    }

    TEST_F(BuildTest, test_rev_build){
        DALM::Model *m = build_dalm_for_test("rev_build", "reverse", logger, 1);
        ASSERT_EQ(m->lm->errorcheck(arpa), 0);
        delete m;

        m = build_dalm_for_test("rev_build", "reverse", logger, 2);
        ASSERT_EQ(m->lm->errorcheck(arpa), 0);
        delete m;

        m = build_dalm_for_test("rev_build", "reverse", logger, 3);
        ASSERT_EQ(m->lm->errorcheck(arpa), 0);
        delete m;

        m = build_dalm_for_test("rev_build", "reverse", logger, 4);
        ASSERT_EQ(m->lm->errorcheck(arpa), 0);
        delete m;
    }
}

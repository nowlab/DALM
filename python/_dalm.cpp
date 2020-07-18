#include <boost/python.hpp>

#include "_dalm.h"
#include "../src/dalm/buildutil.h"

namespace DALM{
    void py_build_dalm(std::string outdir, std::string arpa, std::string opt, unsigned char n_div, int n_cores, std::size_t memory_limit){
        srand(432341);
        Logger logger(stderr);
        build_dalm(outdir, arpa, opt, n_div, n_cores, memory_limit, logger);
    }
}

BOOST_PYTHON_MODULE(_dalm){
    boost::python::class_<DALM::Vocabulary>("Vocabulary", boost::python::no_init)
            .def("lookup", &DALM::Vocabulary::lookup)
            .def("unk", &DALM::Vocabulary::unk)
            .def("size", &DALM::Vocabulary::size);

    boost::python::class_<DALM::LM>("LM", boost::python::no_init)
            .def("query", (float (DALM::LM::*)(DALM::VocabId, DALM::State &)) &DALM::LM::query)
            .def("init_state", &DALM::LM::init_state)
            .def("sum_bows", &DALM::LM::sum_bows)
            .def("dump", &DALM::LM::dump);

    boost::python::class_<DALM::State>("State");

    boost::python::class_<DALM::PyModel>("Model", boost::python::init<std::string, std::string>())
            .add_property("vocab", boost::python::make_function(&DALM::PyModel::vocab, boost::python::return_internal_reference<>()))
            .add_property("lm", boost::python::make_function(&DALM::PyModel::lm, boost::python::return_internal_reference<>()));

    boost::python::def("_build", &DALM::py_build_dalm);
}
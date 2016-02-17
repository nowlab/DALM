cmake_minimum_required(VERSION 2.8.4)
project(dalm)

enable_testing()

set(CMAKE_MACOSX_RPATH FALSE)

find_package(Boost COMPONENTS python REQUIRED)
find_package(PythonLibs REQUIRED)

add_subdirectory(tests/gtest)
include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})

add_definitions(-DDALM_MAX_ORDER=6)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g -fPIC")
set(CMAKE_CXX_FLAGS_DEBUG "-O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto")
include_directories("externals")
include_directories(${PROJECT_BINARY_DIR})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${PYTHON_INCLUDE_DIRS})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tests/dalm_test_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/dalm_test_config.h)

set(HEADERS_MAIN
    src/dalm.h
    )

set(HEADERS
    src/dalm/arpafile.h
    src/dalm/buildutil.h
    src/dalm/da.h
    src/dalm/bst_da.h
    src/dalm/embedded_da.h
    src/dalm/reverse_da.h
    src/dalm/fragment.h
    src/dalm/fileutil.h
    src/dalm/handler.h
    src/dalm/logger.h
    src/dalm/pthread_wrapper.h
    src/dalm/reversefile.h
    src/dalm/trie_reader.h
    src/dalm/state.h
    src/dalm/treefile.h
    src/dalm/value_array.h
    src/dalm/value_array_index.h
    src/dalm/version.h
    src/dalm/vocabulary.h
    src/dalm/vocabulary_file.h
    src/dalm/fileutil.h
    src/dalm/sortutil.h
    )

set(SOURCE_FILES
    ${HEADERS_MAIN}
    ${HEADERS}
    src/bst_da.cpp
    src/embedded_da.cpp
    src/fragment.cpp
    src/dalm.cpp
    src/logger.cpp
    src/reverse_da.cpp
    src/value_array.cpp
    src/vocabulary.cpp
    src/dalm.cpp
    src/vocabulary_file.cpp
    src/build_util.cpp
    externals/darts.h
    )

add_library(
    dalm STATIC
    ${SOURCE_FILES}
)

add_executable(query_dalm examples/query_example.cpp)
target_link_libraries(
    query_dalm
    dalm
    pthread
)

add_executable(build_dalm src/builder.cpp externals/ezOptionParser.hpp)
target_link_libraries(
    build_dalm
    dalm
    pthread
)

add_executable(validate_dalm src/checker.cpp)
target_link_libraries(
    validate_dalm
    dalm
    pthread
)

add_executable(export_dalm src/exporter.cpp)
target_link_libraries(
    export_dalm
    dalm
    pthread
)

add_library(
    dalm_test_helper STATIC
    tests/build_dalm_for_test.cpp
    tests/build_dalm_for_test.h
)

add_executable(dalm_test_all
    tests/test_build.cpp
    tests/test_query.cpp
)
target_link_libraries(
    dalm_test_all
    dalm_test_helper
    dalm
    pthread
    gtest
    gtest_main
)

if(WITH_PYTHON)
    add_library(_dalm SHARED
        python/_dalm.cpp
        python/_dalm.h)
    target_link_libraries(
        _dalm
        dalm
        pthread
        ${Boost_LIBRARIES}
        ${PYTHON_LIBRARY}
    )
    set_target_properties(_dalm PROPERTIES PREFIX "")
    set_target_properties(_dalm PROPERTIES SUFFIX ".so")
endif()

INSTALL(TARGETS dalm ARCHIVE DESTINATION lib)
INSTALL(FILES ${HEADERS} DESTINATION include/dalm)
INSTALL(FILES ${HEADERS_MAIN} DESTINATION include)
INSTALL(FILES externals/darts.h DESTINATION darts-clone)
INSTALL(TARGETS validate_dalm build_dalm query_dalm RUNTIME DESTINATION bin)
if(WITH_PYTHON)
    INSTALL(TARGETS _dalm LIBRARY DESTINATION python)
    INSTALL(FILES python/dalm.py DESTINATION python)
endif()

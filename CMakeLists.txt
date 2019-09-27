cmake_minimum_required(VERSION 2.8.4)
project(dalm)

enable_testing()

add_definitions(-DDALM_MAX_ORDER=6)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g -march=native")
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O3")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
message(STATUS "CMAKE_CXX_STANDARD is " ${CMAKE_CXX_STANDARD})
message(STATUS "CMAKE_BUILD_TYPE is " ${CMAKE_BUILD_TYPE})

option(DALM_NEW_XCHECK "Enable new XCHECK algorithm." ON)
option(DALM_EL_SKIP "Enable EL skipping with new XCHECK algorithm." ON)
if (DALM_NEW_XCHECK)
    add_definitions(-DDALM_NEW_XCHECK)
endif()
if (DALM_EL_SKIP)
    add_definitions(-DDALM_EL_SKIP)
endif()
message(STATUS "DALM_NEW_XCHECK is " ${DALM_NEW_XCHECK})
message(STATUS "DALM_EL_SKIP is " ${DALM_EL_SKIP})

include_directories("include")
include_directories("darts-clone")

set(HEADERS
    include/arpafile.h
    include/da.h
    include/dalm.h
    include/fragment.h
    include/fileutil.h
    include/handler.h
    include/logger.h
    include/pthread_wrapper.h
    include/state.h
    include/treefile.h
    include/value_array.h
    include/value_array_index.h
    include/version.h
    include/vocabulary.h
    include/vocabulary_file.h
    )

set(SOURCE_FILES
    ${HEADERS}
    src/embedded_da.cpp
    src/fragment.cpp
    src/dalm.cpp
    src/logger.cpp
    src/value_array.cpp
    src/vocabulary.cpp
    src/vocabulary_file.cpp
    darts-clone/darts.h
    )

add_library(
    dalm STATIC
    ${SOURCE_FILES}
)

add_executable(mkworddict src/mkworddict.cpp)
target_link_libraries(
    mkworddict
    dalm
    pthread
#    SegFault
)

add_executable(query_sample sample/query_sample.cpp)
target_link_libraries(
    query_sample
    dalm
    pthread
#    SegFault
)

add_executable(query_with_state_sample sample/query_with_state_sample.cpp)
target_link_libraries(
    query_with_state_sample
    dalm
    pthread
#    SegFault
)

add_executable(dalm_builder src/builder.cpp)
target_link_libraries(
    dalm_builder
    dalm
    pthread
#    SegFault
)

INSTALL(TARGETS dalm ARCHIVE DESTINATION lib)
INSTALL(FILES ${HEADERS} DESTINATION include)
INSTALL(FILES darts-clone/darts.h DESTINATION darts-clone)
INSTALL(DIRECTORY scripts DESTINATION . USE_SOURCE_PERMISSIONS)
INSTALL(TARGETS mkworddict query_sample query_with_state_sample dalm_builder RUNTIME DESTINATION bin)

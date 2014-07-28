UNAME_S := $(shell uname -s)

CXX = g++
CPPFLAGS = -Wall -O3 -I. -Iinclude -Idarts-clone
LIBS = -lz -lpthread

all: builder dumper libdalm
	mkdir -p lib bin
	cp libdalm.a lib
	cp dalm_builder bin
	cp trie_dumper bin

DALM_OBJ = src/lm.o src/logger.o src/vocabulary.o src/embedded_da.o src/value_array.o src/fragment.o

builder: $(DALM_OBJ) src/builder.o
	$(CXX) $(CPPFLAGS) -o dalm_builder $^ $(LIBS)

dumper: $(DALM_OBJ) src/dumper.o
	$(CXX) $(CPPFLAGS) -o trie_dumper $^ $(LIBS)

libdalm: $(DALM_OBJ)
	ar -rcs libdalm.a $(DALM_OBJ)

clean:
	rm -fr dalm *.o */*.o */*/*.o */*/*/*.o lib

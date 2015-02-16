UNAME_S := $(shell uname -s)

CXX = g++
CPPFLAGS = -Wall -O3 -I. -Iinclude -Idarts-clone
LIBS = -lz -lpthread

all: builder libdalm
	mkdir -p lib bin
	cp libdalm.a lib
	cp dalm_builder bin

DALM_OBJ = src/dalm.o src/logger.o src/vocabulary.o src/embedded_da.o src/value_array.o src/fragment.o src/vocabulary_file.o

builder: $(DALM_OBJ) src/builder.o
	$(CXX) $(CPPFLAGS) -o dalm_builder $^ $(LIBS)

libdalm: $(DALM_OBJ)
	ar -rcs libdalm.a $(DALM_OBJ)

clean:
	rm -fr dalm *.o */*.o */*/*.o */*/*/*.o lib

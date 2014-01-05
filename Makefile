UNAME_S := $(shell uname -s)

CXX = g++
CPPFLAGS = -Wall -O3 -I. -Iinclude -Idarts-clone -IMurmurHash3
LIBS = -lz -lpthread

all: builder dumper libdalm libMurmurHash3
	mkdir -p lib 
	cp libdalm.a lib
	cp libMurmurHash3.a lib

MURMURHASH3_OBJ = MurmurHash3/MurmurHash3.o
DALM_OBJ = src/da.o src/lm.o src/logger.o src/vocabulary.o

builder: $(DALM_OBJ) $(MURMURHASH3_OBJ) src/builder.o
	$(CXX) $(CPPFLAGS) -o builder $^ $(LIBS)

dumper: $(DALM_OBJ) $(MURMURHASH3_OBJ) src/dumper.o
	$(CXX) $(CPPFLAGS) -o dumper $^ $(LIBS)

libdalm: $(DALM_OBJ)
	ar -rcs libdalm.a $(OBJECTS)

libMurmurHash3: $(MURMURHASH3_OBJ)
	ar -rcs libMurmurHash3.a $(MURMURHASH3_OBJ)

clean:
	rm -fr dalm *.o */*.o */*/*.o */*/*/*.o lib

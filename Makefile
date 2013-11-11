UNAME_S := $(shell uname -s)

CXX = g++
CPPFLAGS = -Wall -O3 -I. -Iinclude -Idarts-clone -IMurmurHash3
LIBS = -lz -lpthread

all: builder dumper libdalm

OBJECTS = MurmurHash3/MurmurHash3.o src/da.o src/lm.o src/logger.o src/vocabulary.o
builder: $(OBJECTS) src/builder.o
	$(CXX) $(CPPFLAGS) -o builder $^ $(LIBS)

dumper: $(OBJECTS) src/dumper.o
	$(CXX) $(CPPFLAGS) -o dumper $^ $(LIBS)

libdalm: $(OBJECTS)
	ar -rcs libdalm.a $(OBJECTS)

clean:
	rm -f dalm *.o */*.o */*/*.o */*/*/*.o

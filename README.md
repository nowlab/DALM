# DALM
<http://www.jnory.com/research/dalm>

This is an implementation of the double-array language model (DALM).
This method is described on:
> Makoto Yasuhara, Toru Tanaka, Jun-ya Norimatsu, Mikio Yamamoto. 2013. An Efficient Language Model Using Double-Array Structures. In Proceedings of the 2013 Conference on Empirical Methods in Natural Language Processing.

* Paper : <http://www.aclweb.org/anthology/D13-1023>
* Slide : <http://www.slideshare.net/jnory/dalm-emnlp-27373037>

> Jun-ya Norimatsu, Makoto Yasuhara, Toru Tanaka, and Mikio Yamamoto. 2016. “A Fast and Compact Language Model Implementation Using Double-array Structures.” To appear in the Transactions on Asian and Low-Resource Language Information Processing (TALLIP). 


## Build Instruction
To build DALM, use waf build system.
Run the command:
> ./waf configure build install --prefix=[install dir]


### (Experimental) CMake build
To build DALM by CMake:
  mkdir build
  cd build
  cmake .. -DCMAKE_INSTALL_PREFIX=[install dir]
  make
  make install

## Usage
### Building a DALM model
To build a DALM model, run:
> [install dir]/bin/build_dalm -f method -f [path to lm] -o [path to output] -d #div -c #cores

If you need to build a large language model (for example, over 30GB in ARPA Format),
we recommend to set the division number "8" or "16" (or larger!).

This script generates following files into [Output Directory]:

* dalm.ini : information of the model.
* dalm.bin : binary dumps of a double-array trie.
* words.bin : binary dumps of vocabulary data.
* words.txt : a set of words of the model.

### Building a DALM model in parallel
By default, build_dalm.sh uses 4 cores in your CPU.
If you have 4 cores and you want to use only 3, please set `-c 3` to the option.

### Using DALM on your system
We include a sample program to use DALM on your system.
Please see sample/query_sample.cpp.
On the build time, you may link the libraries which are stored in the [install dir]/lib directory.

## External Libraries
We use following libraries:

* Darts-clone 0.32g : <https://code.google.com/p/darts-clone/>
* ezOptionParser v0.2.2 : <http://ezoptionparser.sourceforge.net/>
* waf 1.7.13 : <http://code.google.com/p/waf/>
* google test 1.7 : <https://github.com/google/googletest>

## License
The source code is provided under LGPL v3.  
For details, see LICENSE.

from ubuntu:15.10

ADD . /opt/src/DALM
RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y build-essential cmake zlib1g-dev git libboost-all-dev libbz2-dev && \
    cd /opt/src && git clone https://github.com/moses-smt/mgiza.git && \
    cd /opt/src/mgiza && git checkout RELEASE-3.0 && \
    cd /opt/src/mgiza/mgizapp && mkdir build && cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/opt/mgiza && make && make install && \
    cp -p ../scripts/merge_alignment.py /opt/mgiza/bin && \
    cd /opt/src/DALM && mkdir -p build && cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/opt/DALM && make && make install && \
    cd /opt/src && git clone https://github.com/moses-smt/mosesdecoder.git && \
    cd /opt/src/mosesdecoder && git checkout -b RELEASE-3.0 origin/RELEASE-3.0 && \
    cp /opt/src/DALM/moses/DALMWrapper.cpp moses/LM && \
    ./bjam -j4 --with-dalm=/opt/DALM --prefix=/opt/moses --install-scripts=/opt/moses/scripts toolset=gcc && \
    ./bjam clean && \
    cd /opt && tar zcf src.tgz src && rm -r src

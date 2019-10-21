#!/bin/sh

cd $(dirname $0)

if [ ! -e bin ]; then
    mkdir bin
fi
cd bin
cmake -DDALM_NEW_XCHECK=OFF -DDALM_EL_SKIP=OFF ..
cmake --build .
cd ..

if [ ! -e nbin ]; then
    mkdir nbin
fi
cd nbin
cmake -DDALM_NEW_XCHECK=ON -DDALM_EL_SKIP=OFF ..
cmake --build .
cd ..

if [ ! -e n1bin ]; then
    mkdir n1bin
fi
cd n1bin
cmake -DDALM_NEW_XCHECK=ON -DDALM_EL_SKIP=ON ..
cmake --build .
cd ..


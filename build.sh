#!/bin/sh

DIR=$(dirname $0)

if [ ! -e $DIR/bin ]; then
    mkdir $DIR/bin
fi
cd $DIR/bin
cmake -DDALM_NEW_XCHECK=OFF -DDALM_EL_SKIP=OFF -G Ninja ..
cmake --build .
cd ..

if [ ! -e $DIR/nbin ]; then
    mkdir $DIR/nbin
fi
cd $DIR/nbin
cmake -DDALM_NEW_XCHECK=ON -DDALM_EL_SKIP=OFF -G Ninja ..
cmake --build .
cd ..

if [ ! -e $DIR/n1bin ]; then
    mkdir $DIR/n1bin
fi
cd $DIR/n1bin
cmake -DDALM_NEW_XCHECK=ON -DDALM_EL_SKIP=ON -G Ninja ..
cmake --build .
cd ..


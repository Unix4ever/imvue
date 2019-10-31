#!/bin/bash

# gtest
git clone https://github.com/google/googletest.git
cd googletest
git checkout v1.8.x
mkdir build && cd build
cmake ../
sudo make install
cd ../../

# google benchmark
git clone https://github.com/google/benchmark.git
ln -s $(pwd)/googletest/ benchmark/googletest
cd benchmark
mkdir build && cd build
cmake ../
sudo make install


# bin/bash

# convenient script to build this project with cmake
BUILD_TYPE="${1:-Release}"

rm -rf build
mkdir build

cd build

#cmake -G Ninja ..
#ninja

cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE
make -j$(nproc)
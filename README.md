## lock-free skip list

implemented a concurrent skip list in c++20 using CAS on marked pointers. nodes store the deleted flag in the bottom bit of each forward pointer (standard approach to avoid separate lock for logical deletion). hazard pointers handle memory reclamation so retired nodes arent freed while another thread still holds a reference to them.

level heights picked with a geometric distribution. the locate() function physically unlinks marked nodes it encounters during traversal.

locked.hpp has a simple mutex wrapped std::map as the baseline.

build:
```
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
./test
./bench
```

or with g++:
```
g++ -std=c++20 -O3 -pthread test.cpp -o test
g++ -std=c++20 -O3 -pthread bench.cpp -o bench
```

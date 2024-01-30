#!/bin/sh
cmake -S . -B build
cmake --build build && cd build && GTEST_COLOR=1 ctest -V || cd -

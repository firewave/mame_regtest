#!/bin/sh

set -xe

CBA_EXE=ClangBuildAnalyzer-master/build/ClangBuildAnalyzer

if [ ! -f "$CBA_EXE" ]; then
  rm -rf ClangBuildAnalyzer-master
  rm -f ClangBuildAnalyzer-master.zip
  wget https://github.com/aras-p/ClangBuildAnalyzer/archive/master.zip -O ClangBuildAnalyzer-master.zip
  unzip ClangBuildAnalyzer-master.zip
  rm ClangBuildAnalyzer-master.zip
  (cd ClangBuildAnalyzer-master &&
  mkdir build &&
  cd build &&
  cmake -DCMAKE_BUILD_TYPE=Release .. &&
  cmake --build . -- -j "$(nproc)")
fi

rm -rf ClangBuildAnalyzer-build
mkdir ClangBuildAnalyzer-build
cd ClangBuildAnalyzer-build
# TODO: make the compiler configurable - needs to be clang though
# TODO: make additional build flags configurable
cmake -DCMAKE_C_COMPILER=/usr/bin/clang-10 -DCMAKE_CXX_COMPILER=/usr/bin/clang++-10 -DCMAKE_C_FLAGS="-ftime-trace" -DCMAKE_CXX_FLAGS="-ftime-trace" ..
../"$CBA_EXE" --start .
# wait a bit before starting the build - very fast builds might not be detected of timestamp inconsistencies
sleep 1
# run the supplied build command
$*
../"$CBA_EXE" --stop . ClangBuildAnalyzer.capture
../"$CBA_EXE" --analyze ClangBuildAnalyzer.capture > ClangBuildAnalyzer.log

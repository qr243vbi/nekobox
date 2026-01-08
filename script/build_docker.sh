#!/bin/bash
cmake -S . -B build -GNinja
cmake --build build -v
./script/build_go.sh
./script/deploy_linux64.sh

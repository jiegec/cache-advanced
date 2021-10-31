#!/bin/bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
./cache ../trace/game.trace
./cache ../trace/office.trace
./cache ../trace/PDF.trace
./cache ../trace/photo.trace
./cache ../victim_trace/victim1.trace
./cache ../victim_trace/victim2.trace
#!/bin/bash
mkdir -p build
cd build
qmake ../fpgaol_pi_ng/fpgaol_pi_ng.pro
make -j
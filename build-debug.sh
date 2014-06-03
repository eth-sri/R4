#!/bin/bash

Tools/Scripts/build-webkit --no-netscape-plugin --qt --makeargs="-j8" --debug --minimal
cd R4/clients
cd Record
echo "Compiling R4/clients/Record..."
qmake CONFIG+=debug
make
cd ..
cd Replay
echo "Compiling R4/clients/Replay..."
qmake CONFIG+=debug
make

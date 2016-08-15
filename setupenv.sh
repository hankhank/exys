#! /bin/sh

git submodule init
git submodule update

cd mxe
make -j4 qt5 graphviz MXE_TARGETS='i686-w64-mingw32.static'


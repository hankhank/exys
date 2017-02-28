FROM nvidia/cuda:8.0-devel-centos7

LABEL maintainer "Andrew Hankins" 

RUN yum install -y epel-release
RUN yum install -y make vim-common qt5-qtbase-devel qt5-base graphviz-devel gmock gtest-devel cmake3

# INSTALL LLVM 3.8.1
RUN curl -fsSLO "http://releases.llvm.org/3.8.1/llvm-3.8.1.src.tar.xz" && \
    tar xf "llvm-3.8.1.src.tar.xz" && \
    cd "llvm-3.8.1.src" && \
    mkdir "build" && \
    cd "build" && \
    cmake3 -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DLLVM_TARGETS_TO_BUILD="NVPTX;X86" .. -DLLVM_ENABLE_RTTI=ON && \
    make install && \
    cd ../.. && \
    rm -rf llvm-3.8.1.src.tar.xz


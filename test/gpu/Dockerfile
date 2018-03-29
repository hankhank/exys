FROM nvidia/cuda:8.0-devel-centos7

LABEL maintainer "Andrew Hankins" 

RUN yum install -y epel-release
RUN yum install -y make vim-common qt5-qtbase-devel qt5-base gmock gtest-devel cmake3

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

# GOOGLE BENCHMARK
RUN curl -fsSLO https://github.com/google/benchmark/archive/v1.1.0.tar.gz && \
    tar xf "v1.1.0.tar.gz" && \
    cd "benchmark-1.1.0" && \
    mkdir "build" && \
    cd "build" && \
    cmake3 .. && \
    make install -j2 && \
    cd ../.. && \
    rm -r "benchmark-1.1.0" 

# GRAPHVIZ WITH FIXES
RUN curl -fsSLO "http://www.graphviz.org/pub/graphviz/stable/redhat/el7/x86_64/os/graphviz-libs-2.38.0-1.el7.x86_64.rpm" && \
    yum install -y "graphviz-libs-2.38.0-1.el7.x86_64.rpm" && \
    curl -fsSLO "http://www.graphviz.org/pub/graphviz/stable/redhat/el7/x86_64/os/graphviz-devel-2.38.0-1.el7.x86_64.rpm" && \
    yum install -y "graphviz-devel-2.38.0-1.el7.x86_64.rpm" && \
    rm *.rpm

RUN yum install -y wget 

RUN mkdir mxe_tmp && cd mxe_tmp && \
    wget -r -np -k http://pkg.mxe.cc/repos/tar/mxe-x86-64-w64-mingw32.static/

RUN for i in $(ls *.tar.xz); do tar -xf $i -C /; done
    

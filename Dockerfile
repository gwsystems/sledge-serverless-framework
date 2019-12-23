# Inspired by lucet's Dockerfile.

# using ubuntu 18 docker image
FROM ubuntu:bionic

# install some basic packages
RUN apt-get update \
	&& apt-get install -y --no-install-recommends \
	build-essential \
	curl \
	git \
	python3-dev \
	python3-pip \
	cmake \
	ca-certificates \
	libssl-dev \
	pkg-config \
	gcc \
	g++ \
	clang-8 \
	clang-tools-8 \
	llvm-8 \
	llvm-8-dev \
	libc++-dev \
	libc++abi-dev \
	lld-8 \
	lldb-8 \
	libclang-8-dev \
	libclang-common-8-dev \
	vim \
	apache2 \
	subversion \
	libapache2-mod-svn \
	libsvn-dev \
	binutils-dev \
	build-essential \
	automake \
	libtool \
	strace \
	less \
	libuv1-dev \
	&& rm -rf /var/lib/apt/lists/*

# Enable apache2 for svn
RUN a2enmod dav
RUN a2enmod dav_svn
RUN service apache2 restart
RUN pip3 install numpy

# set to use our installed clang version
RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-8 100
RUN update-alternatives --install /usr/bin/llvm-config llvm-config /usr/bin/llvm-config-8 100

# set LD_LIBRARY_PATH
ENV LD_LIBRARY_PATH=/usr/local/lib

RUN curl https://sh.rustup.rs -sSf | \
    sh -s -- --default-toolchain nightly-2019-09-25 -y && \
        /root/.cargo/bin/rustup update nightly
ENV PATH=/root/.cargo/bin:$PATH

RUN rustup component add rustfmt --toolchain nightly-2019-09-25-x86_64-unknown-linux-gnu
RUN rustup target add wasm32-wasi

RUN cargo install --debug cargo-audit cargo-watch rsign2

RUN curl -sS -L -O https://github.com/CraneStation/wasi-sdk/releases/download/wasi-sdk-7/wasi-sdk_7.0_amd64.deb \
	&& dpkg -i wasi-sdk_7.0_amd64.deb && rm -f wasi-sdk_7.0_amd64.deb

ENV WASI_SDK=/opt/wasi-sdk
ENV PATH=/opt/awsm/bin:$PATH


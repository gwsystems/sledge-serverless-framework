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
	clang-6.0 \
	clang-tools-6.0 \
	llvm-6.0 \
	llvm-6.0-dev \
	libc++-dev \
	libc++abi-dev \
	lld-6.0 \
	lldb-6.0 \
	libclang-6.0-dev \
	libclang-common-6.0-dev \
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
RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-6.0 100
RUN update-alternatives --install /usr/bin/llvm-config llvm-config /usr/bin/llvm-config-6.0 100

# set LD_LIBRARY_PATH
ENV LD_LIBRARY_PATH=/usr/local/lib

# install rust - commands copied straight from lucet's dockerfile.
# so we have exactly the same rust version as lucet!
RUN curl -sS -L -O https://static.rust-lang.org/dist/rust-1.35.0-x86_64-unknown-linux-gnu.tar.gz \
	&& tar xzf rust-1.35.0-x86_64-unknown-linux-gnu.tar.gz \
	&& cd rust-1.35.0-x86_64-unknown-linux-gnu \
	&& ./install.sh \
	&& cd .. \
	&& rm -rf rust-1.35.0-x86_64-unknown-linux-gnu rust-1.35.0-x86_64-unknown-linux-gnu.tar.gz
ENV PATH=/usr/local/bin:$PATH
RUN cargo install --root /usr/local cargo-audit cargo-watch

## copied again from lucet for when we want to use wasi-sdk
#RUN curl -sS -L -O https://github.com/CraneStation/wasi-sdk/releases/download/wasi-sdk-5/wasi-sdk_5.0_amd64.deb \
#	&& dpkg -i wasi-sdk_5.0_amd64.deb && rm -f wasi-sdk_5.0_amd64.deb
#
#ENV WASI_SDK=/opt/wasi-sdk
ENV PATH=/opt/awsm/bin:$PATH


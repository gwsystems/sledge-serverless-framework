#!/bin/bash

LLVM_VERSION=13

ARCH=$(uname -p)

if [[ $ARCH = "x86_64" ]]; then
	SHFMT_URL=https://github.com/mvdan/sh/releases/download/v3.4.3/shfmt_v3.4.3_linux_amd64
	WASI_SDK_URL=https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-14/wasi-sdk_14.0_amd64.deb
elif [[ $ARCH = "aarch64" ]]; then
	SHFMT_URL=https://github.com/patrickvane/shfmt/releases/download/master/shfmt_linux_arm
	echo "ARM64 support is still a work in progress!"
	exit 1
else
	echo "This script only supports x86_64 and aarch64"
	exit 1
fi

sudo apt-get update && sudo apt-get install -y --no-install-recommends \
	apt-utils \
	man-db \
	&& yes | unminimize

sudo apt-get update && sudo apt-get install -y --no-install-recommends \
	automake \
	bc \
	bsdmainutils \
	build-essential \
	binutils-dev \
	ca-certificates \
	cmake \
	curl \
	fonts-dejavu \
	fonts-cascadia-code \
	fonts-roboto \
	gdb \
	git \
	gpg-agent \
	gnuplot \
	hey \
	httpie \
	imagemagick \
	jq \
	less \
	libssl-dev \
	libtinfo5 \
	libtool \
	libz3-4 \
	lsb-release \
	make \
	netpbm \
	openssh-client \
	pango1.0-tools \
	pkg-config \
	shellcheck \
	software-properties-common \
	strace \
	valgrind \
	wabt \
	wamerican \
	wget

wget $SHFMT_URL -O shfmt && chmod +x shfmt && sudo mv shfmt /usr/local/bin/shfmt

sudo ./install_llvm.sh $LLVM_VERSION

curl -sS -L -O $WASI_SDK_URL && sudo dpkg -i wasi-sdk_14.0_amd64.deb && rm -f wasi-sdk_14.0_amd64.deb

if [ -z "${WASI_SDK_PATH}" ]; then
	export WASI_SDK_PATH=/opt/wasi-sdk
	echo "export WASI_SDK_PATH=/opt/wasi-sdk" >> ~/.bashrc
fi

curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- --default-toolchain stable --component rustfmt --target wasm32-wasi -y

echo "Run 'source ~/.bashrc'"

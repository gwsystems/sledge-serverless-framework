#!/bin/bash

LLVM_VERSION=12

ARCH=$(uname -p)

if [[ $ARCH = "x86_64" ]]; then
	SHFMT_URL=https://github.com/mvdan/sh/releases/download/v3.4.3/shfmt_v3.4.3_linux_amd64
	WASI_SDK_URL=https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-12/wasi-sdk_12.0_amd64.deb
elif [[ $ARCH = "aarch64" ]]; then
	SHFMT_URL=https://github.com/patrickvane/shfmt/releases/download/master/shfmt_linux_arm
	echo "ARM64 support is still a work in progress!"
	exit 1
else
	echo "This script only supports x86_64 and aarch64"
	exit 1
fi

# General GCC C/C++ Build toolchain
# pkg-config, libtool - used by PocketSphinx
# cmake - used by cmsis
sudo apt-get update && apt-get install -y --no-install-recommends \
	automake \
	build-essential \
	binutils-dev \
	cmake \
	git \
	libtinfo5 \
	libtool \
	make \
	pkg-config

# Network Tools
sudo apt-get update && apt-get install -y --no-install-recommends \
	ca-certificates \
	curl \
	gpg-agent \
	hey \
	httpie \
	libssl-dev \
	lsb-release \
	openssh-client \
	software-properties-common \
	wget

sudo apt-get update && sudo apt-get install -y --no-install-recommends \
	bc \
	bsdmainutils \
	fonts-dejavu \
	fonts-cascadia-code \
	fonts-roboto \
	gnuplot \
	imagemagick \
	jq \
	libz3-4 \
	netpbm \
	pango1.0-tools \
	shellcheck \
	wamerican

# Interactive Tools
sudo apt-get update && sudo apt-get install -y --no-install-recommends \
	gdb \
	less \
	strace \
	valgrind \
	vim \
	wabt

# shfmt is a formatter for shell scripts
wget $SHFMT_URL -O shfmt && chmod +x shfmt && sudo mv shfmt /usr/local/bin/shfmt

sudo ./install_llvm.sh $LLVM_VERSION

curl -sS -L -O $WASI_SDK_URL && sudo dpkg -i wasi-sdk_12.0_amd64.deb && rm -f wasi-sdk_12.0_amd64.deb

if [ -z "${WASI_SDK_PATH}" ]; then
	export WASI_SDK_PATH=/opt/wasi-sdk
	echo "export WASI_SDK_PATH=/opt/wasi-sdk" >> ~/.bashrc
fi

curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- --default-toolchain stable --component rustfmt --target wasm32-wasi -y

echo "Run 'source ~/.bashrc'"

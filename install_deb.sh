#!/bin/bash

HEY_URL=https://hey-release.s3.us-east-2.amazonaws.com/hey_linux_amd64
LLVM_VERSION=12
SHELLCHECK_URL=https://github.com/koalaman/shellcheck/releases/download/v0.7.1/shellcheck-v0.7.1.linux.x86_64.tar.xz
SHFMT_URL=https://github.com/mvdan/sh/releases/download/v3.2.4/shfmt_v3.2.4_linux_amd64
WASI_SDK_URL=https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-12/wasi-sdk_12.0_amd64.deb

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
	software-properties-common \
	strace \
	valgrind \
	wabt \
	wamerican \
	wget

wget $HEY_URL -O hey && chmod +x hey && sudo mv hey /usr/bin/hey

wget $SHFMT_URL -O shfmt && chmod +x shfmt && sudo mv shfmt /usr/local/bin/shfmt

wget $SHELLCHECK_URL -O shellcheck && chmod +x shellcheck && sudo mv shellcheck /usr/local/bin/shellcheck

sudo ./install_llvm.sh $LLVM_VERSION

curl -sS -L -O $WASI_SDK_URL && sudo dpkg -i wasi-sdk_12.0_amd64.deb && rm -f wasi-sdk_12.0_amd64.deb

if [ -z "${WASI_SDK_PATH}" ]; then
	export WASI_SDK_PATH=/opt/wasi-sdk
	echo "export WASI_SDK_PATH=/opt/wasi-sdk" >> ~/.bashrc
fi

curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- --default-toolchain stable --component rustfmt --target wasm32-wasi -y

echo "Run 'source ~/.bashrc'"

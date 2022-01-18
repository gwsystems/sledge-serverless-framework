#!/bin/bash

HEY_URL=https://hey-release.s3.us-east-2.amazonaws.com/hey_linux_amd64
WASI_SDK_URL=https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-12/wasi-sdk_12.0_amd64.deb
SHFMT_URL=https://github.com/mvdan/sh/releases/download/v3.2.4/shfmt_v3.2.4_linux_amd64
SHELLCHECK_URL=https://github.com/koalaman/shellcheck/releases/download/v0.7.1/shellcheck-v0.7.1.linux.x86_64.tar.xz

apt-get update && apt-get install -y --no-install-recommends \
	bc \
	fonts-dejavu \
	fonts-cascadia-code \
	fonts-roboto \
	gnuplot \
	imagemagick \
	jq \
	libz3-4 \
	netpbm \
	pango1.0-tools \
	wamerican

wget $HEY_URL -O hey && chmod +x hey && sudo mv hey /usr/bin/hey

wget $SHFMT_URL -O shfmt && chmod +x shfmt && sudo mv shfmt /usr/local/bin/shfmt

wget $SHELLCHECK_URL -O shellcheck && chmod +x shellcheck && sudo mv shellcheck /usr/local/bin/shellcheck

LLVM_VERSION=12
./install_llvm.sh $LLVM_VERSION

curl -sS -L -O $WASI_SDK_URL && dpkg -i wasi-sdk_12.0_amd64.deb && rm -f wasi-sdk_12.0_amd64.deb

echo "Add WASI_SDK_PATH to your bashrc and resource!"
echo "Example: export WASI_SDK_PATH=/opt/wasi-sdk"

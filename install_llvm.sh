#!/bin/bash
# Installs LLVM tooling, delegating the to the LLVM script as much as possible

LLVM_VERSION=$1

echo "Installing LLVM $LLVM_VERSION"

# Script Installs clang, lldb, lld, and clangd
curl --proto '=https' --tlsv1.2 -sSf https://apt.llvm.org/llvm.sh | bash -s -- "$LLVM_VERSION"

# Installing "libc++-xx-dev" automagically installs "libc++1-xx", "libunwind-xx" and "libunwind-xx-dev"
apt-get install -y --no-install-recommends \
	"libc++-$LLVM_VERSION-dev" \
	"libc++abi-$LLVM_VERSION-dev" \
	"clang-tools-$LLVM_VERSION" \
	"clang-tidy-$LLVM_VERSION" \
	"clang-format-$LLVM_VERSION"

update-alternatives --remove-all wasm-ld
update-alternatives --remove-all llvm-config
update-alternatives --remove-all llvm-objdump
update-alternatives --remove-all llvm-dis
update-alternatives --remove-all clang-format
update-alternatives --remove-all clang
update-alternatives --remove-all clang++
update-alternatives --remove-all clang-tidy

update-alternatives --install /usr/bin/wasm-ld wasm-ld "/usr/bin/wasm-ld-$LLVM_VERSION" 100
update-alternatives --install /usr/bin/llvm-config llvm-config "/usr/bin/llvm-config-$LLVM_VERSION" 100
update-alternatives --install /usr/bin/llvm-objdump llvm-objdump "/usr/bin/llvm-objdump-$LLVM_VERSION" 100
update-alternatives --install /usr/bin/llvm-dis llvm-dis /usr/bin/llvm-dis-$LLVM_VERSION 100
update-alternatives --install /usr/bin/clang-format clang-format "/usr/bin/clang-format-$LLVM_VERSION" 100
update-alternatives --install /usr/bin/clang clang "/usr/bin/clang-$LLVM_VERSION" 100
update-alternatives --install /usr/bin/clang++ clang++ "/usr/bin/clang++-$LLVM_VERSION" 100
update-alternatives --install /usr/bin/clang-tidy clang-tidy "/usr/bin/clang-tidy-$LLVM_VERSION" 100

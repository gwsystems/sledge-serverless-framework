#!/bin/bash
# Installs LLVM tooling, delegating the to the LLVM script as much as possible
# We need to shim support for LLVM 8 because the LLVM script only supports 9-12

LLVM_VERSION=$1

echo "Installing LLVM $LLVM_VERSION"

# Script Installs clang, lldb, lld, and clangd
if [[ "$LLVM_VERSION" -gt 8 ]]; then
	curl --proto '=https' --tlsv1.2 -sSf https://apt.llvm.org/llvm.sh | bash -s -- "$LLVM_VERSION"
else
	apt-get install -y --no-install-recommends \
		"clang-$LLVM_VERSION" \
		"lldb-$LLVM_VERSION" \
		"lld-$LLVM_VERSION" \
		"clangd-$LLVM_VERSION"
fi

apt-get install -y --no-install-recommends \
	"libc++-$LLVM_VERSION-dev" \
	"libc++abi-$LLVM_VERSION-dev" \
	"libc++1-$LLVM_VERSION"

# Explicitly use clang-format-11 because of changes between 10 and 11
apt-get install -y --no-install-recommends \
	"clang-format-11"

update-alternatives --install /usr/bin/clang clang "/usr/bin/clang-$LLVM_VERSION" 100
update-alternatives --install /usr/bin/clang++ clang++ "/usr/bin/clang++-$LLVM_VERSION" 100
update-alternatives --install /usr/bin/llvm-config llvm-config "/usr/bin/llvm-config-$LLVM_VERSION" 100
update-alternatives --install /usr/bin/clang-format clang-format "/usr/bin/clang-format-11" 100

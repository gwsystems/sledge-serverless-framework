#!/bin/bash
# Uninstalls all LLVM tooling as much as possible

echo "Uninstalling LLVM"

apt-get remove -y --purge \
	"llvm*" \
	"lld*" \
	"libc++*" \
	"libunwind*" \
	"clang*" \

apt-get autoremove -y

update-alternatives --remove-all clang-format
update-alternatives --remove-all clang
update-alternatives --remove-all clang++
update-alternatives --remove-all llvm-config
update-alternatives --remove-all llvm-objdump
update-alternatives --remove-all clang-tidy
update-alternatives --remove-all wasm-ld
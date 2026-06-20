#!/bin/bash
# Installs LLVM tooling for the SLEdge x86_64 dev image.
#
# On most releases this just delegates to the upstream apt.llvm.org installer.
# On Ubuntu noble (24.04) that doesn't work: apt.llvm.org never published an
# llvm-toolchain-noble-13 repo (only 18/19+), and SLEdge is pinned to LLVM 13
# (awsm builds against an LLVM-13 binding fork). So on noble we pin the *focal*
# apt.llvm.org repo instead, plus the two focal-era runtime libs noble dropped
# (libtinfo5, libffi7) that the focal LLVM packages depend on.
set -e

LLVM_VERSION=$1

echo "Installing LLVM $LLVM_VERSION"

CODENAME="$(. /etc/os-release && echo "$VERSION_CODENAME")"

if [ "$CODENAME" = "noble" ]; then
	echo "Detected Ubuntu noble: pinning the focal apt.llvm.org repo for LLVM $LLVM_VERSION"

	# focal-era runtime libs no longer shipped by noble, required by focal LLVM debs
	cd /tmp
	curl -fsSL -o libtinfo5.deb http://archive.ubuntu.com/ubuntu/pool/universe/n/ncurses/libtinfo5_6.2-0ubuntu2.1_amd64.deb
	curl -fsSL -o libffi7.deb http://archive.ubuntu.com/ubuntu/pool/main/libf/libffi/libffi7_3.3-4_amd64.deb
	dpkg -i libtinfo5.deb libffi7.deb
	rm -f libtinfo5.deb libffi7.deb

	wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | gpg --dearmor -o /usr/share/keyrings/llvm.gpg
	echo "deb [signed-by=/usr/share/keyrings/llvm.gpg] http://apt.llvm.org/focal/ llvm-toolchain-focal-$LLVM_VERSION main" \
		> /etc/apt/sources.list.d/llvm.list
	apt-get update

	# Note: lldb-$LLVM_VERSION is intentionally omitted. The focal lldb package
	# depends on libpython3.8, which noble no longer provides, and the debugger is
	# not needed to build SLEdge.
	apt-get install -y --no-install-recommends \
		"clang-$LLVM_VERSION" \
		"lld-$LLVM_VERSION" \
		"clangd-$LLVM_VERSION" \
		"llvm-$LLVM_VERSION" \
		"llvm-$LLVM_VERSION-dev"
else
	# Upstream installer: installs clang, lldb, lld, and clangd
	curl --proto '=https' --tlsv1.2 -sSf https://apt.llvm.org/llvm.sh | bash -s -- "$LLVM_VERSION"
fi

# Installing "libc++-xx-dev" automagically installs "libc++1-xx", "libunwind-xx" and "libunwind-xx-dev"
apt-get install -y --no-install-recommends \
	"libc++-$LLVM_VERSION-dev" \
	"libc++abi-$LLVM_VERSION-dev" \
	"clang-tools-$LLVM_VERSION" \
	"clang-tidy-$LLVM_VERSION" \
	"clang-format-$LLVM_VERSION"

# --remove-all clears any prior registration; harmless if none exists (so do not
# let "no alternatives" abort the script under set -e).
update-alternatives --remove-all wasm-ld 2> /dev/null || true
update-alternatives --remove-all llvm-config 2> /dev/null || true
update-alternatives --remove-all llvm-objdump 2> /dev/null || true
update-alternatives --remove-all llvm-dis 2> /dev/null || true
update-alternatives --remove-all clang-format 2> /dev/null || true
update-alternatives --remove-all clang 2> /dev/null || true
update-alternatives --remove-all clang++ 2> /dev/null || true
update-alternatives --remove-all clang-tidy 2> /dev/null || true

update-alternatives --install /usr/bin/wasm-ld wasm-ld "/usr/bin/wasm-ld-$LLVM_VERSION" 100
update-alternatives --install /usr/bin/llvm-config llvm-config "/usr/bin/llvm-config-$LLVM_VERSION" 100
update-alternatives --install /usr/bin/llvm-objdump llvm-objdump "/usr/bin/llvm-objdump-$LLVM_VERSION" 100
update-alternatives --install /usr/bin/llvm-dis llvm-dis /usr/bin/llvm-dis-$LLVM_VERSION 100
update-alternatives --install /usr/bin/clang-format clang-format "/usr/bin/clang-format-$LLVM_VERSION" 100
update-alternatives --install /usr/bin/clang clang "/usr/bin/clang-$LLVM_VERSION" 100
update-alternatives --install /usr/bin/clang++ clang++ "/usr/bin/clang++-$LLVM_VERSION" 100
update-alternatives --install /usr/bin/clang-tidy clang-tidy "/usr/bin/clang-tidy-$LLVM_VERSION" 100

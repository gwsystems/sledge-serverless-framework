#!/bin/bash
# This script is responsible for copying, linking, and aliasing all of our build tools such that they are
# in known paths that we can add to PATH and LD_LIBRARY_PATH. The binaries go into "${SYS_PREFIX}/bin" and
# the libraries go into "${SYS_PREFIX}/lib". By default, SYS_PREFIX is `/opt/sledge/`, which means that this
# script is assumed to be executed as root, as in our Docker build container. However, by by setting
# SYS_PREFIX to a directory that the user has access to, this can be avoided.
#
# For example, in the GitHub Actions workflow, SYS_PREFIX is set to the topmost project directory and the
# environment is updated as follows.
#
# SYS_PREFIX="$(pwd)" ./install.sh
# PATH="$(pwd)/bin:$PATH"
# LD_LIBRARY_PATH="$(pwd)/lib:$LD_LIBRARY_PATH"
#
# This is currently executed
# - Indirectly via `make install` in the root Makefile, typically when the sledge-dev build container is built
# - Directly by the GitHub workflow
#
# The script takes either wasmception or wasi as the first argument to install either wasmception or WASI-SDK
# If no argument is provided, wasmception is assumed
# If either wasmception or wasi is provided, an additional `-d` or `--dry-run` flag can be provided to view the commands
# and paths that would be generated. This is helpful for sanity checking when setting SYS_PREFIX and using in a new context

echo "Setting up toolchain environment"

for last_arg in "$@"; do :; done

if [[ $last_arg == "-d" ]] || [[ $last_arg == "--dry-run" ]]; then
	DRY_RUN=true
else
	DRY_RUN=false
fi

if $DRY_RUN; then
	DRY_RUN_PREFIX=echo
else
	DRY_RUN_PREFIX=
fi

# Get the absolute path of the topmost project directly
# The use of dirname is particular. It seems unneccesary how this script is run
SYS_SRC_PREFIX=${SYS_SRC_PREFIX:-"$(
	cd "$(dirname "$(dirname "${0}")")" || exit 1
	pwd -P
)"}
$DRY_RUN && echo SYS_SRC_PREFIX: "$SYS_SRC_PREFIX"

# And check for the presence of this script to make sure we got it right
if [ ! -x "${SYS_SRC_PREFIX}/install.sh" ]; then
	echo "Unable to find the install script" >&2
	exit 1
fi

SYS_NAME='sledge'
COMPILER='awsm'
COMPILER_EXECUTABLE=$COMPILER

# /opt/sledge
SYS_PREFIX=${SYS_PREFIX:-"/opt/${SYS_NAME}"}
$DRY_RUN && echo SYS_PREFIX: "$SYS_PREFIX"

# /sledge, where the sledge repo is mounted from the host
SYS_SRC_PREFIX=${SYS_SRC_PREFIX:-"/${SYS_NAME}"}
$DRY_RUN && echo SYS_SRC_PREFIX: "$SYS_SRC_PREFIX"

# The release directory containing the binary of the aWsm compiler
SYS_COMPILER_REL_DIR=${SYS_COMPILER_REL_DIR:-"${SYS_SRC_PREFIX}/${COMPILER}/target/release"}
$DRY_RUN && echo SYS_COMPILER_REL_DIR: "$SYS_COMPILER_REL_DIR"

# /opt/sledge/bin
SYS_BIN_DIR=${SYS_BIN_DIR:-"${SYS_PREFIX}/bin"}
$DRY_RUN && echo SYS_BIN_DIR: "$SYS_BIN_DIR"

# /opt/sledge/lib
SYS_LIB_DIR=${SYS_LIB_DIR:-"${SYS_PREFIX}/lib"}
$DRY_RUN && echo SYS_LIB_DIR: "$SYS_LIB_DIR"

# The first argument can be either wasi or wasmception. This determines the system interface used
# The default is wasmception
# Currently, WASI is not actually supported by the runtime.
if [ $# -eq 0 ] || [ "$1" = "wasmception" ]; then
	echo "Setting up for wasmception"
	WASM_PREFIX=${WASM_PREFIX:-"${SYS_SRC_PREFIX}/${COMPILER}/wasmception"}
	WASM_BIN=${WASM_BIN:-"${WASM_PREFIX}/dist/bin"}
	WASM_SYSROOT=${WASM_SYSROOT:-"${WASM_PREFIX}/sysroot"}
	WASM_TARGET=${WASM_TARGET:-"wasm32-unknown-unknown-wasm"}
	WASM_BIN_PREFIX=${WASM_BIN_PREFIX:-"$WASM_TARGET"}
	WASM_TOOLS=(ar)
elif [ "$1" = "wasi" ]; then
	echo "Setting up for wasi-sdk"
	WASM_PREFIX=${WASM_PREFIX:-${WASM_SDK:-"/opt/wasi-sdk"}}
	WASM_BIN=${WASM_BIN:-"${WASM_PREFIX}/bin"}
	WASM_SYSROOT=${WASM_SYSROOT:-"${WASM_PREFIX}/share/sysroot"}
	WASM_TARGET=${WASM_TARGET:-"wasm32-wasi"}
	WASM_BIN_PREFIX=${WASM_BIN_PREFIX:-"$WASM_TARGET"}
	WASM_TOOLS=(ar dwarfdump nm ranlib size)
fi
$DRY_RUN && echo WASM_PREFIX: "$WASM_PREFIX"
$DRY_RUN && echo WASM_BIN: "$WASM_BIN"
$DRY_RUN && echo WASM_SYSROOT: "$WASM_SYSROOT"
$DRY_RUN && echo WASM_TARGET: "$WASM_TARGET"
$DRY_RUN && echo WASM_BIN_PREFIX: "$WASM_BIN_PREFIX"
$DRY_RUN && echo WASM_TOOLS: "${WASM_TOOLS[@]}"

# Delete all existing installations of the binaries
$DRY_RUN_PREFIX rm -f "${SYS_BIN_DIR}"/*

# And reinstall
$DRY_RUN_PREFIX install -d -v "$SYS_BIN_DIR" || exit 1

# Symbolically link the Awsm compiler
# /sledge/awsm/target/release/silverfish /opt/sledge/bin/awsm
$DRY_RUN_PREFIX ln -sfv "${SYS_COMPILER_REL_DIR}/${COMPILER_EXECUTABLE}" "${SYS_BIN_DIR}/${COMPILER_EXECUTABLE}"

# Generate shell script stubs that act as aliases that automatically set the approproiate target and sysroot
# for either Wasmception or WASI-SDK
# For example, when wasmception is set, calling `wasm32-unknown-unknown-wasm-clang` results in
# `exec "/sledge/awsm/wasmception/dist/bin/clang" --target="wasm32-unknown-unknown-wasm" --sysroot="/sledge/awsm/wasmception/sysroot" "$@"`
for file in clang clang++; do
	wrapper_file="$(mktemp)"
	cat > "$wrapper_file" << EOT
#! /bin/sh

exec "${WASM_BIN}/${file}" --target="$WASM_TARGET" --sysroot="$WASM_SYSROOT" "\$@"
EOT
	cat "$wrapper_file"
	$DRY_RUN_PREFIX install -p -v "$wrapper_file" "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-${file}"
	$DRY_RUN && echo rm -f "$wrapper_file"
	rm -f "$wrapper_file"
done

# Link the LLVM Tools with the proper prefix
for file in "${WASM_TOOLS[@]}"; do
	$DRY_RUN_PREFIX ln -sfv "${WASM_BIN}/llvm-${file}" "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-${file}"
done

# Link any other tools with the proper prefix
OTHER_TOOLS=(ld)
for file in "${OTHER_TOOLS[@]}"; do
	$DRY_RUN_PREFIX ln -sfv "${WASM_BIN}/wasm-${file}" "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-${file}"
done

# Link clang as gcc if needed
$DRY_RUN_PREFIX ln -svf "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-clang" "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-gcc"
$DRY_RUN_PREFIX ln -svf "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-clang++" "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-g++"

echo "Done!"

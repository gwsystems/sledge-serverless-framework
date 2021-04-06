#!/bin/sh
# Executing by the root Makefile, typically within the sledge-dev build container

echo "Setting up toolchain environment"

# Get the path of this repo
# SYS_SRC_PREFIX could be externally set
# This is written assuming that install.sh might be in the project top level directory or a subdirectory,
# but that it is always called with a relative path from the project root
SYS_SRC_PREFIX=${SYS_SRC_PREFIX:-"$(
  cd "$(dirname "$(dirname "${0}")")" || exit 1
  pwd -P
)"}
echo SYS_SRC_PREFIX: "$SYS_SRC_PREFIX"

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
echo SYS_PREFIX: "$SYS_PREFIX"

# /sledge, where the sledge repo is mounted from the host
SYS_SRC_PREFIX=${SYS_SRC_PREFIX:-"/${SYS_NAME}"}
echo SYS_SRC_PREFIX: "$SYS_SRC_PREFIX"

# The release directory containing the binary of the aWsm compiler
SYS_COMPILER_REL_DIR=${SYS_COMPILER_REL_DIR:-"${SYS_SRC_PREFIX}/${COMPILER}/target/release"}
echo SYS_COMPILER_REL_DIR: "$SYS_COMPILER_REL_DIR"

# /opt/sledge/bin
SYS_BIN_DIR=${SYS_BIN_DIR:-"${SYS_PREFIX}/bin"}
echo SYS_BIN_DIR: "$SYS_BIN_DIR"

# /opt/sledge/lib
SYS_LIB_DIR=${SYS_LIB_DIR:-"${SYS_PREFIX}/lib"}
echo SYS_LIB_DIR: "$SYS_LIB_DIR"

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
  WASM_TOOLS=ar
elif [ "$1" = "wasi" ]; then
  echo "Setting up for wasi-sdk"
  WASM_PREFIX=${WASM_PREFIX:-${WASM_SDK:-"/opt/wasi-sdk"}}
  WASM_BIN=${WASM_BIN:-"${WASM_PREFIX}/bin"}
  WASM_SYSROOT=${WASM_SYSROOT:-"${WASM_PREFIX}/share/sysroot"}
  WASM_TARGET=${WASM_TARGET:-"wasm32-wasi"}
  WASM_BIN_PREFIX=${WASM_BIN_PREFIX:-"$WASM_TARGET"}
  WASM_TOOLS=ar dwarfdump nm ranlib size
fi
echo WASM_PREFIX: "$WASM_PREFIX"
echo WASM_BIN: "$WASM_BIN"
echo WASM_SYSROOT: "$WASM_SYSROOT"
echo WASM_TARGET: "$WASM_TARGET"
echo WASM_BIN_PREFIX: "$WASM_BIN_PREFIX"
echo WASM_TOOLS: "$WASM_TOOLS"

# Delete all existing installations of the binaries
echo rm -f "${SYS_BIN_DIR}"/*
rm -f "${SYS_BIN_DIR}"/*

# And reinstall
echo install -d -v "$SYS_BIN_DIR" || exit 1
install -d -v "$SYS_BIN_DIR" || exit 1

# Symbolically link the Awsm compiler
# /sledge/awsm/target/release/silverfish /opt/sledge/bin/awsm
echo ln -sfv "${SYS_COMPILER_REL_DIR}/${COMPILER_EXECUTABLE}" "${SYS_BIN_DIR}/${COMPILER_EXECUTABLE}"
ln -sfv "${SYS_COMPILER_REL_DIR}/${COMPILER_EXECUTABLE}" "${SYS_BIN_DIR}/${COMPILER_EXECUTABLE}"

# Generate shell script stubs that act as aliases that automatically set the approproiate target and sysroot
# for either Wasmception or WASI-SDK
# For example, when wasmception is set, calling `wasm32-unknown-unknown-wasm-clang` results in
# `exec "/sledge/awsm/wasmception/dist/bin/clang" --target="wasm32-unknown-unknown-wasm" --sysroot="/sledge/awsm/wasmception/sysroot" "$@"`
for file in clang clang++; do
  wrapper_file="$(mktemp)"
  cat >"$wrapper_file" <<EOT
#! /bin/sh

exec "${WASM_BIN}/${file}" --target="$WASM_TARGET" --sysroot="$WASM_SYSROOT" "\$@"
EOT
  cat "$wrapper_file"
  echo install -p -v "$wrapper_file" "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-${file}"
  install -p -v "$wrapper_file" "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-${file}"
  echo rm -f "$wrapper_file"
  rm -f "$wrapper_file"
done
exit

# Link the LLVM Tools with the proper prefix
for file in ${WASM_TOOLS}; do
  echo ln -sfv "${WASM_BIN}/llvm-${file}" "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-${file}"
  ln -sfv "${WASM_BIN}/llvm-${file}" "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-${file}"
done

# Link any other tools with the proper prefix
for file in ld; do
  echo ln -sfv "${WASM_BIN}/wasm-${file}" "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-${file}"
  ln -sfv "${WASM_BIN}/wasm-${file}" "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-${file}"
done

# Link clang as gcc if needed
echo ln -svf "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-clang" "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-gcc"
ln -svf "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-clang" "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-gcc"
echo ln -svf "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-clang++" "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-g++"
ln -svf "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-clang++" "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-g++"

echo "Done!"

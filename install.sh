#!/bin/sh
# Executing by the root Makefile, typically within the sledge-dev build container

echo "Setting up toolchain environment"

# Get the path of this repo
SYS_SRC_PREFIX=${SYS_SRC_PREFIX:-"$(
  cd "$(dirname "$(dirname "${0}")")" || exit 1
  pwd -P
)"}

# And check for the presence of this script to make sure we got it right
if [ ! -x "${SYS_SRC_PREFIX}/install.sh" ]; then
  echo "Unable to find the install script" >&2
  exit 1
fi

SYS_NAME='sledge'
COMPILER='awsm'

# /opt/sledge
SYS_PREFIX=${SYS_PREFIX:-"/opt/${SYS_NAME}"}

# /sledge, where the sledge repo is mounted from the host
SYS_SRC_PREFIX=${SYS_SRC_PREFIX:-"/${SYS_NAME}"}

# The release directory containing the binary of the aWsm compiler
SYS_COMPILER_REL_DIR=${SYS_COMPILER_REL_DIR:-"${SYS_SRC_PREFIX}/${COMPILER}/target/release"}

# /opt/sledge/bin?
SYS_BIN_DIR=${SYS_BIN_DIR:-"${SYS_PREFIX}/bin"}
# /opt/sledge/lib?
SYS_LIB_DIR=${SYS_LIB_DIR:-"${SYS_PREFIX}/lib"}

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

rm -f "${SYS_BIN_DIR}"/*
install -d -v "$SYS_BIN_DIR" || exit 1

# Link each of the binaries in the system bin directory
BINS=${COMPILER}
for bin in $BINS; do
  # i.e. ./silverfish/target/release/silverfish -> /opt/sledge/bin/silverfish
  ln -sfv "${SYS_COMPILER_REL_DIR}/${bin}" "${SYS_BIN_DIR}/${bin}"
done

for file in clang clang++; do
  wrapper_file="$(mktemp)"
  cat >"$wrapper_file" <<EOT
#! /bin/sh

exec "${WASM_BIN}/${file}" --target="$WASM_TARGET" --sysroot="$WASM_SYSROOT" "\$@"
EOT
  install -p -v "$wrapper_file" "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-${file}"
  rm -f "$wrapper_file"
done

#for file in ar dwarfdump nm ranlib size; do
for file in ${WASM_TOOLS}; do
  ln -sfv "${WASM_BIN}/llvm-${file}" "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-${file}"
done

for file in ld; do
  ln -sfv "${WASM_BIN}/wasm-${file}" "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-${file}"
done

ln -svf "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-clang" "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-gcc"
ln -svf "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-clang++" "${SYS_BIN_DIR}/${WASM_BIN_PREFIX}-g++"

echo "Done!"

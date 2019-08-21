#!/bin/sh

# Inspired by Lucet's install.sh

echo "Setting up toolchain environment"
SYS_SRC_PREFIX=${SYS_SRC_PREFIX:-"$(
	cd $(dirname $(dirname ${0}))
	pwd -P
)"}

if [ ! -x "${SYS_SRC_PREFIX}/install.sh" ]; then
	echo "Unable to find the install script" >&2
	exit 1
fi

SYS_NAME='awsm'
SILVERFISH='silverfish'
SYS_PREFIX=${SYS_PREFIX:-"/opt/${SYS_NAME}"}
SYS_SRC_PREFIX=${SYS_SRC_PREFIX:-"/${SYS_NAME}"}
SYS_SF_REL_DIR=${SYS_SF_REL_DIR:-"${SYS_SRC_PREFIX}/${SILVERFISH}/target/release"}
SYS_BIN_DIR=${SYS_BIN_DIR:-"${SYS_PREFIX}/bin"}
SYS_LIB_DIR=${SYS_LIB_DIR:-"${SYS_PREFIX}/lib"}

#use wasmception
if [ $# -eq 0 ] || [ "$1" = "wasmception" ]; then
echo "Setting up for wasmception"
WASM_PREFIX=${WASM_PREFIX:-"${SYS_SRC_PREFIX}/${SILVERFISH}/wasmception"}
WASM_BIN=${WASM_BIN:-"${WASM_PREFIX}/dist/bin"}
WASM_SYSROOT=${WASM_SYSROOT:-"${WASM_PREFIX}/sysroot"}
WASM_TARGET=${WASM_TARGET:-"wasm32-unknown-unknown-wasm"}
WASM_BIN_PREFIX=${WASM_BIN_PREFIX:-"$WASM_TARGET"}
WASM_TOOLS=ar
elif [ "$1" = "wasi" ]; then
echo "Setting up for wasi-sdk"
#use wasi-sdk
WASM_PREFIX=${WASM_PREFIX:-${WASM_SDK:-"/opt/wasi-sdk"}}
WASM_BIN=${WASM_BIN:-"${WASM_PREFIX}/bin"}
WASM_SYSROOT=${WASM_SYSROOT:-"${WASM_PREFIX}/share/sysroot"}
WASM_TARGET=${WASM_TARGET:-"wasm32-wasi"}
WASM_BIN_PREFIX=${WASM_BIN_PREFIX:-"$WASM_TARGET"}
WASM_TOOLS=ar dwarfdump nm ranlib size
fi

# silverfish compiler binary! 
BINS=${SILVERFISH}
DEVSRC=${SYS_BIN_DIR}/'devenv_src.sh'

rm -f ${SYS_BIN_DIR}/*
install -d -v "$SYS_BIN_DIR" || exit 1
for bin in $BINS; do
    ln -sfv "${SYS_SF_REL_DIR}/${bin}" "${SYS_BIN_DIR}/${bin}"
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

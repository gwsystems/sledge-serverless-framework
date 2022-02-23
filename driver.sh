#!/bin/bash

# git checkout wasmception
# git submodule update --recursive
# make libsledge
# make build
# make -C runtime clean all
# rm runtime/bin/*.so
cd awsm
cargo build --release
cd ..

touch wasm32-unknown-unknown-wasm-clang
wrapper_file=wasm32-unknown-unknown-wasm-clang

cat > "$wrapper_file" << EOT
#!/bin/sh

exec /home/sean/projects/sledge/awsm/wasmception/dist/bin/clang --target=wasm32-unknown-unknown-wasm --sysroot=/home/sean/projects/sledge/awsm/wasmception/sysroot "\$@"
EOT

chmod +x wasm32-unknown-unknown-wasm-clang
sudo install wasm32-unknown-unknown-wasm-clang /usr/bin

sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-8 1000
sudo update-alternatives --install /usr/bin/llvm-config llvm-config /usr/bin/llvm-config-8 1000

make -C applications clean all

make -f test.mk all

cd ./tests/gocr/by_dpi && SLEDGE_NWORKERS=1 ./run.sh -e=fifo_nopreemption.env

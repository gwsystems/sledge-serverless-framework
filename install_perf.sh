#!/bin/bash

# If already installed, just return
command -v perf && {
	echo "perf is already installed."
	exit 0
}

[[ "$(whoami)" != "root" ]] && {
	echo "Expected to run as root"
	exit 1
}

# Under WSL2, perf has to be installed from source
if  grep --silent 'WSL2' <(uname -r); then
	echo "WSL detected. perf must be built from source"
	sudo apt-get install flex bison python3-dev liblzma-dev libnuma-dev zlib1g libperl-dev libgtk2.0-dev libslang2-dev systemtap-sdt-dev libelf-dev binutils-dev libbabeltrace-dev libdw-dev libunwind-dev libiberty-dev --yes
	git clone --depth 1 https://github.com/microsoft/WSL2-Linux-Kernel ~/WSL2-Linux-Kernel
	make -Wno-error -j8 -C ~/WSL2-Linux-Kernel/tools/perf
	sudo cp ~/WSL2-Linux-Kernel/tools/perf/perf /usr/local/bin
	rm -rf ~/WSL2-Linux-Kernel
else
	apt-get install "linux-tools-$(uname -r)" linux-tools-generic -y
fi

exit 0

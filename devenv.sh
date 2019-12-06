#!/bin/sh

#
# This environment file and dockerfile are 
# inspired by lucet's devenv_xxx.sh scripts
#
HOST_ROOT=${HOST_ROOT:-$(cd "$(dirname ${BASH_SOURCE:-$0})" && pwd)}
SYS_NAME='awsm'
HOST_SYS_MOUNT=${HOST_SYS_MOUNT:-"/${SYS_NAME}"}
SYS_WASMCEPTION='silverfish/wasmception'
SYS_DOC_NAME=${SYS_NAME}
SYS_DOC_DEVNAME=${SYS_DOC_NAME}'-dev'
SYS_DOC_TAG='latest'
SYS_DOC_NAMETAG=${SYS_DOC_NAME}:${SYS_DOC_TAG}
SYS_DOC_DEVNAMETAG=${SYS_DOC_DEVNAME}:${SYS_DOC_TAG}
SYS_BUILD_TIMEOUT=10

usage()
{
	echo "usage $0 <setup/run/stop>"
}

countdown()
{
	tmp_cnt=$1
	while [ ${tmp_cnt} -gt 0 ]; do
		printf "${tmp_cnt}."
#		sleep 1
		tmp_cnt=$((tmp_cnt - 1))
	done
	echo
}

envsetup()
{
	if docker image inspect ${SYS_DOC_NAMETAG} > /dev/null; then
		echo "${SYS_DOC_NAMETAG} image exists, remove it first!"
		exit 1
	fi

	echo "Setting up ${SYS_NAME}"
	git submodule update --init --recursive 2>/dev/null ||:

	# Perhaps change in the wasmception (forked) repo, 
	# Gregor already forked every mainline repo to modify something or the other!
	#
	# That said, you may comment this if you're not behind a firewall!
	# http:// doesn't work for me at my current work place.
	echo "Changing http:// to https:// in ${SYS_WASMCEPTION}"
	sed -i 's/http:\/\//https:\/\//' ${SYS_WASMCEPTION}/Makefile

	if docker image inspect ${SYS_DOC_DEVNAMETAG} >> /dev/null; then
		echo "${SYS_DOC_DEVNAME} image exists, rebuilding it"
		echo "(you have ${SYS_BUILD_TIMEOUT}secs to stop the rebuild)"
		countdown ${SYS_BUILD_TIMEOUT}
	fi

	echo "Building ${SYS_DOC_DEVNAMETAG}"
	docker build -t ${SYS_DOC_DEVNAMETAG} .

	echo "Creating ${SYS_DOC_NAMETAG} on top of ${SYS_DOC_DEVNAMETAG}"
	docker run --privileged --name=${SYS_DOC_DEVNAME} --detach --mount type=bind,src="$(cd $(dirname ${0}); pwd -P),target=/${SYS_NAME}" \
		${SYS_DOC_DEVNAMETAG} /bin/sleep 99999999 > /dev/null

	echo "Building ${SYS_NAME}"
	docker exec -t -w ${HOST_SYS_MOUNT} ${SYS_DOC_DEVNAME} make install

	echo "Tagging the new image"
	docker container commit ${SYS_DOC_DEVNAME} ${SYS_DOC_NAMETAG}

	echo "Cleaning up ${SYS_DOC_DEVNAME}"
	docker kill ${SYS_DOC_DEVNAME}
	docker rm ${SYS_DOC_DEVNAME}

	echo "Done!"
}

envrun()
{
	if ! docker image inspect ${SYS_DOC_NAMETAG} > /dev/null; then
		envsetup
	fi

	if docker ps -f name=${SYS_DOC_NAME} --format '{{.Names}}' | grep -q "^${SYS_DOC_NAME}" ; then
		echo "Container is running" >&2
	else
		echo "Starting ${SYS_DOC_NAME}"
		docker run --privileged --security-opt seccomp:unconfined --name=${SYS_DOC_NAME} --detach --mount type=bind,src="$(cd $(dirname ${0}); pwd -P),target=/${SYS_NAME}" \
			${SYS_DOC_NAMETAG} /bin/sleep 99999999 > /dev/null
	fi

	echo "Running shell"
	docker exec -t -i -w "${HOST_SYS_MOUNT}" ${SYS_DOC_NAME} /bin/bash
}

envstop()
{
	echo "Stopping container"
	docker stop ${SYS_DOC_NAME}
	echo "Removing container"
	docker rm ${SYS_DOC_NAME}
}

envrm()
{
	envstop
	docker rmi ${SYS_DOC_NAME}
}

envrma()
{
	envrm
	docker rmi ${SYS_DOC_DEVNAME}
}

if [ $# -ne 1 ]; then
	usage $0
	exit 1
fi

case $1 in
	run)
		envrun
		;;
	stop)
		envstop
		;;
	setup)
		envsetup
		;;
	rm)
		envrm
		;;
	rma)
		envrma
		;;
	*)
		echo "invalid option: $1"
		usage $0
		exit 1
		;;
esac
echo
echo "done!"

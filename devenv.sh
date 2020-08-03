#!/bin/sh

# This environment file and dockerfile are inspired by lucet's devenv_xxx.sh scripts

# Root directory of host
HOST_ROOT=${HOST_ROOT:-$(cd "$(dirname "${BASH_SOURCE:-$0}")" && pwd)}

# Name use to represent the SLEdge system
SYS_NAME='sledge'

# /sledge
HOST_SYS_MOUNT=${HOST_SYS_MOUNT:-"/${SYS_NAME}"}

# SYS_WASMCEPTION='silverfish/wasmception'

# sledge
SYS_DOC_NAME=${SYS_NAME}

# sledge-dev
SYS_DOC_DEVNAME=${SYS_DOC_NAME}'-dev'

# Docker Tag we want to use
SYS_DOC_TAG='latest'

# The name of the non-dev Docker container that we want to build. sledge:latest
SYS_DOC_NAMETAG=${SYS_DOC_NAME}:${SYS_DOC_TAG}
SYS_DOC_DEVNAMETAG=${SYS_DOC_DEVNAME}:${SYS_DOC_TAG}

# An optional timeout that allows a user to terminate the script if sledge-dev is detected
SYS_BUILD_TIMEOUT=0

# Provides help to user on how to use this script
usage() {
  echo "usage $0 <setup||run||stop||rm||rma/>"
  echo "      setup   Build a sledge runtime container and sledge-dev, a build container with toolchain needed to compile your own functions"
  echo "      run     Start the sledge Docker image as an interactive container with this repository mounted"
  echo "      stop    Stop and remove the sledge Docker container after use"
  echo "      rm      Remove the sledge runtime container and image, but leaves the sledge-dev container in place"
  echo "      rma     Removes all the sledge and sledge-dev containers and images"
}

# Given a number of seconds, initiates a countdown sequence
countdown() {
  tmp_cnt=$1
  while [ "${tmp_cnt}" -gt 0 ]; do
    printf "%d." "${tmp_cnt}"
    sleep 1
    tmp_cnt=$((tmp_cnt - 1))
  done
  echo
}

# Build and runs the build container sledge-dev and then executes make install on the project
# Finally "forks" the sledge-dev build container into the sledge execution container
envsetup() {
  # I want to create this container before the Makefile executes so that my user owns it
  # This allows me to execute the sledgert binary from my local host
  mkdir -p "$HOST_ROOT/runtime/bin"

  # Check to see if the sledge:latest image exists, exiting if it does
  # Because sledge:latest is "forked" after completing envsetup, this suggests that envsetup was already run
  if docker image inspect ${SYS_DOC_NAMETAG} 1>/dev/null 2>/dev/null; then
    echo "${SYS_DOC_NAMETAG} image exists, which means that 'devenv.sh setup' already ran to completion!"
    echo "If you are explicitly trying to rebuild SLEdge, run the following:"
    echo "devenv.sh rma | Removes the images sledge:latest AND sledge-dev:latest"
    exit 1
  fi

  echo "Setting up ${SYS_NAME}"

  echo "Updating git submodules"
  git submodule update --init --recursive 2>/dev/null || :d

  echo "Using Dockerfile.$(uname -m)"
  rm -f Dockerfile
  ln -s Dockerfile.$(uname -m) Dockerfile

  # As a user nicety, warn the user if sledge-dev is detected
  # This UX differs from detecting sledge, which immediately exits
  # This is disabled because it doesn't seem useful
  if
    docker image inspect "${SYS_DOC_DEVNAMETAG}" 1>/dev/null 2>/dev/null && [ $SYS_BUILD_TIMEOUT -gt 0 ]
  then
    echo "${SYS_DOC_DEVNAME} image exists, rebuilding it"
    echo "(you have ${SYS_BUILD_TIMEOUT}secs to stop the rebuild)"
    countdown ${SYS_BUILD_TIMEOUT}
  fi

  # Build the image sledge-dev:latest
  echo "Building ${SYS_DOC_DEVNAMETAG}"
  docker build --tag "${SYS_DOC_DEVNAMETAG}" .

  # Run the sledge-dev:latest image as a background container named sledge-dev with the project directly mounted at /sledge
  echo "Creating the build container ${SYS_DOC_NAMETAG} from the image ${SYS_DOC_DEVNAMETAG}"
  docker run \
    --privileged \
    --name=${SYS_DOC_DEVNAME} \
    --detach \
    --mount type=bind,src="$(cd "$(dirname "${0}")" && pwd -P || exit 1),target=/${SYS_NAME}" \
    "${SYS_DOC_DEVNAMETAG}" /bin/sleep 99999999 >/dev/null

  # Execute the make install command on the sledge-dev image to build the project
  echo "Building ${SYS_NAME}"
  docker exec \
    --tty \
    --workdir "${HOST_SYS_MOUNT}" \
    ${SYS_DOC_DEVNAME} make install

  # Create the image sledge:latest from the current state of docker-dev
  echo "Tagging the new image"
  docker container commit ${SYS_DOC_DEVNAME} ${SYS_DOC_NAMETAG}

  # Kill and remove the running sledge-dev container
  echo "Cleaning up ${SYS_DOC_DEVNAME}"
  docker kill ${SYS_DOC_DEVNAME}
  docker rm ${SYS_DOC_DEVNAME}

  echo "Done!"
}

# Executes an interactive BASH shell in the sledge container with /sledge as the working directory
# This is the SLEdge project directory mounted from the host environment.
# If the image sledge:latest does not exist, automatically runs envsetup to build sledge and create it
# If the a container names sledge is not running, starts it from sledge:latest, mounting the SLEdge project directory to /sledge
envrun() {
  if ! docker image inspect ${SYS_DOC_NAMETAG} >/dev/null; then
    envsetup
  fi

  if docker ps -f name=${SYS_DOC_NAME} --format '{{.Names}}' | grep -q "^${SYS_DOC_NAME}"; then
    echo "Container is running" >&2
  else

    echo "Starting ${SYS_DOC_NAME}"
    docker run \
      --privileged \
      --security-opt seccomp:unconfined \
      --name=${SYS_DOC_NAME} \
      --detach \
      --mount type=bind,src="$(cd "$(dirname "${0}")" && pwd -P || exit 1),target=/${SYS_NAME}" \
      ${SYS_DOC_NAMETAG} /bin/sleep 99999999 >/dev/null
  fi

  echo "Running shell"
  docker exec --tty --interactive --workdir "${HOST_SYS_MOUNT}" ${SYS_DOC_NAME} /bin/bash
}

# Stops and removes the sledge "runtime" container
envstop() {
  echo "Stopping container"
  docker stop ${SYS_DOC_NAME}
  echo "Removing container"
  docker rm ${SYS_DOC_NAME}
}

# Stops and removes the sledge "runtime" container and then removes the sledge "runtime" image
envrm() {
  envstop
  docker rmi ${SYS_DOC_NAME}
}

# Stops and removes the sledge "runtime" container and image and then removes the sledge-dev "build image" image
envrma() {
  envrm
  docker rmi ${SYS_DOC_DEVNAME}
}

if [ $# -ne 1 ]; then
  echo "incorrect number of arguments: $*"
  usage "$0"
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
    usage "$0"
    exit 1
    ;;
esac
echo
echo "done!"

#!/bin/bash
# Currently, the build container still used root. This results in files owned by root that interfere with running things outside of the container. Pending additional tooling work, this script is a stop gap that searches and chowns all files in the proeject tree owned by root

if [[ $(whoami) == "root" ]]; then
	echo "Should not be run as root"
	exit 1
fi

if [[ $(pwd) == "/" ]]; then
	echo "Should not be run from root directory"
	exit 1
fi

# Uses your host username and its primary associated group
username="$(whoami)"
group="$(id -g -n "$username")"

while read -r file; do
	echo sudo chown "$username":"$group" "$file"
	sudo chown "$username":"$group" "$file"
done < <(find ~+ -type f,d -user root)

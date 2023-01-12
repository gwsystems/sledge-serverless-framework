#!/bin/bash
#please hardcode the mount path and run this script with non-root user
#don't forget source ~/.bashrc after running this script
mount_path=$MYMOUNT

if [[ $mount_path == "" ]]
then
	echo MYMOUNT env var not defined
	exit 1
fi

function move_docker_dir {
        sudo service docker stop
        sudo mv /var/lib/docker $mount_path 
        sudo ln -s $mount_path/docker /var/lib/docker
        sudo service docker restart
        sudo docker -v
}

sudo apt update
sudo apt install -y docker.io
sudo docker run hello-world
sudo docker -v
move_docker_dir

echo "====== please check whether docker is ready ======"
read varname

sudo apt-get purge golang*
mkdir -p download
cd download
wget https://golang.org/dl/go1.14.6.linux-amd64.tar.gz
tar -xvf go1.14.6.linux-amd64.tar.gz
# remove old go bin files
sudo rm -r /usr/local/go
# add new go bin files
sudo mv go /usr/local

# store the source codes
mkdir -p $mount_path/go

GOROOT=/usr/local/go
GOPATH=$mount_path/go
echo "export GOROOT=/usr/local/go" >> ~/.bashrc
echo "export GOPATH=$mount_path/go" >>  ~/.bashrc
echo "export PATH=$PATH:$GOROOT/bin:$GOPATH/bin"  >>  ~/.bashrc

source ~/.bashrc
echo "please source ~/.bashrc"

sudo apt-get update
sudo apt install docker.io
sudo service docker stop
sudo mv /var/lib/docker /my_mount
sudo ln -s /my_mount/docker /var/lib/docker
sudo service docker restart
sudo docker -v


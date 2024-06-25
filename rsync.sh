#!/bin/bash

# rsync -ru --progress --exclude={'res','out.txt','out.dat','err.dat'} ./tests/* emil@c220g2-011017.wisc.cloudlab.us:/users/emil/sledge-server/tests/
# rsync -ru --progress --exclude={'res','out.txt','out.dat','err.dat'} ./tests/* emil@c220g2-011016.wisc.cloudlab.us:/users/emil/sledge-client/tests/

# rsync -ru --progress --exclude={'thirdparty','res','err.dat','out*','*.log'} ./tests ./runtime emil@c220g2-011314.wisc.cloudlab.us:/users/emil/sledge-server/
# rsync -ru --progress --exclude={'res','err.dat','out*','*.log'} ./tests emil@c220g2-011323.wisc.cloudlab.us:/users/emil/sledge-client/

rsync -ru --progress --exclude={'thirdparty','res-*','err.dat','out*','*.log','input*'} ./tests ./runtime emil@128.105.145.72:/users/emil/sledge-server/
rsync -ru --progress --exclude={'thirdparty','res-*','err.dat','out*','*.log','mt-juan/input-cnn','mt-emil/input-cnn'} ./tests ./runtime emil@128.105.145.71:/users/emil/sledge-client/
# rsync -ru --progress --exclude={'thirdparty','res-*','err.dat','out*','*.log','mt-juan/input-cnn','mt-emil/input-cnn'} ./tests ./runtime emil@128.105.145.70:/users/emil/sledge-client/
rsync -ru --progress --exclude={'thirdparty','res-*','err.dat','out*','*.log','input*'} ./tests ./runtime emil@128.105.145.70:/users/emil/sledge-server/
# rsync -ru --progress --exclude={'thirdparty','res-*','err.dat','out*','*.log'} ./tests ./runtime emil@128.105.145.132:/users/emil/sledge-client/

# If on a network where only 443 is allowed use this (after allowing port forwarding ssh to 443 on the server):
# rsync -ru -e 'ssh -p 443' --progress --exclude={'res','out.txt','out.dat','err.dat'} ./tests/* emil@server:/users/emil/sledge-server/tests/
# rsync -ru -e 'ssh -p 443' --progress --exclude={'res','out.txt','out.dat','err.dat'} ./tests/* emil@client:/users/emil/sledge-client/tests/


# lab-dell (don't forget to provide the private key in the config file inside .ssh folder)
# rsync -ru --progress --exclude={'thirdparty','res-*','err.dat','out*','*.log'} ./tests ./runtime lab@161.253.75.227:/home/lab/sledge-emil/

# CMU (don't forget to provide the private key in the config file inside .ssh folder)
# rsync -ru --progress --exclude={'thirdparty','res','err.dat','out*','*.log'} ./tests ./runtime gwu@arena0.andrew.cmu.edu:/home/gwu/sledge/

# esma
# rsync -ru --progress --exclude={'thirdparty','res-*','err.dat','out*','*.log'} ./tests emil@161.253.75.224:/home/emil/sledge-client/

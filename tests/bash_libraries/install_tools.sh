#!/bin/bash

if ! command -v http > /dev/null; then
	if [[ $(whoami) == "root" ]]; then
		apt update
		apt install -y httpie
	else
		sudo apt update
		sudo apt install -y httpie
	fi
fi

if ! command -v hey > /dev/null; then
	HEY_URL=https://hey-release.s3.us-east-2.amazonaws.com/hey_linux_amd64
	wget $HEY_URL -O hey
	chmod +x hey

	if [[ $(whoami) == "root" ]]; then
		mv hey /usr/bin/hey
	else
		sudo mv hey /usr/bin/hey
	fi
fi

if ! command -v gnuplot > /dev/null; then
	if [[ $(whoami) == "root" ]]; then
		apt update
		apt install -y gnuplot
	else
		sudo apt update
		sudo apt install -y gnuplot
	fi
fi


if ! command -v jq > /dev/null; then
	if [[ $(whoami) == "root" ]]; then
		apt update
		apt install -y jq
	else
		sudo apt update
		sudo apt install -y jq
	fi
fi

if ! command -v htop > /dev/null; then
	if [[ $(whoami) == "root" ]]; then
		apt update
		apt install -y htop
	else
		sudo apt update
		sudo apt install -y htop
	fi
fi

if ! command -v loadtest > /dev/null; then
	if ! command -v npm > /dev/null; then
		# if [[ $(whoami) == "root" ]]; then
		# 	apt update
		# 	apt install -y npm
		# else
		# 	sudo apt update
		# 	sudo apt install -y npm
		# fi
		# installs NVM (Node Version Manager)
		curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.7/install.sh | bash
		sleep 5
		export NVM_DIR="$HOME/.nvm"
		[ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh"  # This loads nvm
		[ -s "$NVM_DIR/bash_completion" ] && \. "$NVM_DIR/bash_completion"  # This loads nvm bash_completion
		# download and install Node.js
		nvm install 14
		# verifies the right Node.js version is in the environment
		node -v # should print `v14.21.3`
		# verifies the right NPM version is in the environment
		npm -v # should print `6.14.18`
	fi

	# Try pulling Emil's version of loadtest to support post binary files
	# if [[ $(whoami) == "root" ]]; then
	# 	npm install -y -g loadtest
	# else
	# 	sudo npm install -y -g loadtest
	# fi

	pushd ~
	git clone https://github.com/emil916/loadtest.git
	pushd loadtest
	# if [[ $(whoami) == "root" ]]; then
	npm install -g
	# else
	# 	sudo npm install -g
	# fi
	popd
	popd 
fi

# For SOD:
# if ! command -v imagemagick > /dev/null; then
# 	if [ "$(whoami)" == "root" ]; then
# 		apt-get install -y imagemagick
# 	else
# 		sudo apt-get install -y imagemagick
# 	fi
# fi

# For GOCR, too many to check one-by-one, so uncomment below to install:
# if [[ "$(whoami)" == "root" ]]; then
# 	apt-get install -y netpbm pango1.0-tools wamerican fonts-roboto fonts-cascadia-code fonts-dejavu
# else
# 	sudo apt-get install -y netpbm pango1.0-tools wamerican fonts-roboto fonts-cascadia-code fonts-dejavu
# fi

source ~/.bashrc
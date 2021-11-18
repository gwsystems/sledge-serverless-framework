# The double dollar is needed to prevent expansion of $2 in the awk program
OS := $(shell awk -F= '/^NAME/{print $$2}' /etc/os-release)

states.svg: states.dot
	dot -Tsvg states.dot > states.svg

clean:
	rm -f states.svg

.PHONY: graphviz
graphviz.install:
	echo ${OS}
ifeq (${OS},"Ubuntu")
	sudo apt install graphviz
endif

all: clean udp 

udp: udpclient.c
	@echo "Compiling udpclient"
	@gcc udpclient.c -o ../../bin/udpclient -lpthread

clean:
	@rm -f ../../bin/udpclient

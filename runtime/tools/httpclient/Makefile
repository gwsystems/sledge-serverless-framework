.PHONY: all client clean

all: clean client

client: client.c ../../http-parser/http_parser.c ../../http-parser/http_parser.h
	@echo "Compiling httpclient"
	@gcc  -I../../http_parser/ ../../http-parser/http_parser.c client.c -g -o ../../bin/httpclient

clean:
	@rm -f ../../bin/httpclient

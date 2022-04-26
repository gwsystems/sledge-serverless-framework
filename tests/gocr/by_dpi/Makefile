SLEDGE_BINARY_DIR=../../../runtime/bin
HOSTNAME=localhost

default: run

clean: 
	rm -rf res/*

run:
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} ${SLEDGE_BINARY_DIR}/sledgert spec.json

debug:
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} gdb ${SLEDGE_BINARY_DIR}/sledgert --eval-command="run spec.json"

client-72dpi:
	mkdir -p tmp
	shuf -n10 /usr/share/dict/american-english > tmp/10_words.txt
	pango-view --dpi=72 --font=mono -qo tmp/72_dpi.png tmp/10_words.txt
	pngtopnm tmp/72_dpi.png > tmp/72_dpi.pnm
	curl -H 'Expect:' -H "Content-Type: text/plain" --data-binary @tmp/72_dpi.pnm "${HOSTNAME}:10000/gocr_72_dpi"
	rm -rf tmp

client-108dpi:
	mkdir -p tmp
	shuf -n10 /usr/share/dict/american-english > tmp/10_words.txt
	pango-view --dpi=108 --font=mono -qo tmp/108_dpi.png tmp/10_words.txt
	pngtopnm tmp/108_dpi.png > tmp/108_dpi.pnm
	curl -H 'Expect:' -H "Content-Type: text/plain" --data-binary @tmp/108_dpi.pnm "${HOSTNAME}:10000/gocr_108_dpi"
	rm -rf tmp

client-144dpi:
	mkdir -p tmp
	shuf -n10 /usr/share/dict/american-english > tmp/10_words.txt
	pango-view --dpi=144 --font=mono -qo tmp/144_dpi.png tmp/10_words.txt
	pngtopnm tmp/144_dpi.png > tmp/144_dpi.pnm
	curl -H 'Expect:' -H "Content-Type: text/plain" --data-binary @tmp/144_dpi.pnm "${HOSTNAME}:10000/gocr_144_dpi"
	rm -rf tmp

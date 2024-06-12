SLEDGE_BINARY_DIR=../../../runtime/bin
HOSTNAME=localhost

default: run

clean: 
	rm -rf res/*

run:
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} ${SLEDGE_BINARY_DIR}/sledgert spec.json

debug:
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} gdb ${SLEDGE_BINARY_DIR}/sledgert --eval-command="run spec.json"

client-10:
	mkdir -p tmp
	shuf -n10 /usr/share/dict/american-english > tmp/10_words.txt
	pango-view --font=mono -qo tmp/10_words.png tmp/10_words.txt
	curl  -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@tmp/10_words.png" "${HOSTNAME}:10000/gocr_10_words"
	rm -rf tmp

client-100:
	mkdir -p tmp
	shuf -n100 /usr/share/dict/american-english > tmp/100_words.txt
	pango-view --font=mono -qo tmp/100_words.png tmp/100_words.txt
	curl  -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@tmp/100_words.png" "${HOSTNAME}:10000/gocr_100_words"
	rm -rf tmp

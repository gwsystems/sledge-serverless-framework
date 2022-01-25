SLEDGE_BINARY_DIR=../../../runtime/bin
HOSTNAME=localhost

default: run

clean: 
	rm -rf res/*

run:
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} ${SLEDGE_BINARY_DIR}/sledgert spec.json

debug:
	LD_LIBRARY_PATH=${SLEDGE_BINARY_DIR} gdb ${SLEDGE_BINARY_DIR}/sledgert --eval-command="run spec.json"

client-dejavu_sans_mono:
	mkdir -p tmp
	shuf -n10 /usr/share/dict/american-english > tmp/10_words.txt
	pango-view --font="DejaVu Sans Mono" -qo tmp/10_words.png tmp/10_words.txt
	pngtopnm tmp/10_words.png > tmp/10_words.pnm
	curl  -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@tmp/10_words.pnm" "${HOSTNAME}:10000"
	rm -rf tmp

client-roboto:
	mkdir -p tmp
	shuf -n10 /usr/share/dict/american-english > tmp/10_words.txt
	pango-view --font="Roboto" -qo tmp/10_words.png tmp/10_words.txt
	pngtopnm tmp/10_words.png > tmp/10_words.pnm
	curl  -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@tmp/10_words.pnm" "${HOSTNAME}:10000"
	rm -rf tmp

client-cascadia_code:
	mkdir -p tmp
	shuf -n10 /usr/share/dict/american-english > tmp/10_words.txt
	pango-view --font="Cascadia Code" -qo tmp/10_words.png tmp/10_words.txt
	pngtopnm tmp/10_words.png > tmp/10_words.pnm
	curl  -H 'Expect:' -H "Content-Type: text/plain" --data-binary "@tmp/10_words.pnm" "${HOSTNAME}:10000"
	rm -rf tmp

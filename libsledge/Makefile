CFILES := src/*.c
INCLUDES := -Iinclude/

all: dist/libsledge.a

dist:
	mkdir dist

dist/memory_instructions.o: src/memory_instructions.c dist
	clang  -fPIC -c ${INCLUDES} -o $@ $<

dist/numeric_instructions.o: src/numeric_instructions.c dist
	clang  -fPIC -c ${INCLUDES} -o $@ $<

dist/table_instructions.o: src/table_instructions.c dist
	clang  -fPIC -c ${INCLUDES} -o $@ $<

dist/libsledge.a: dist/memory_instructions.o dist/numeric_instructions.o dist/table_instructions.o
	ar rcs dist/libsledge.a $^

clean: 
	rm -rf dist

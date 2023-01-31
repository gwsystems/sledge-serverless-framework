CFILES := src/*.c
INCLUDES := -Iinclude/

# fPIC = Position Independent Code, necessary for linking to relative addresses.
CFLAGS := -fPIC -O3 -flto -ftls-model=initial-exec

# Strips out calls to assert() and disables debuglog
# CFLAGS+=-DNDEBUG

# CFI Sanitizer
# CFLAGS+=-fvisibility=default -fsanitize=cfi

# Undefined Sanitizer - https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
# CFLAGS+=-fsanitize=undefined,float-divide-by-zero,unsigned-integer-overflow,implicit-conversion,local-bounds,nullability

# Address Sanitizer - "Fast Memory Error Detector" - https://clang.llvm.org/docs/AddressSanitizer.html
# CFLAGS+=-fsanitize=address -fno-omit-frame-pointer

# Clang SafeStack - https://clang.llvm.org/docs/SafeStack.html
# CFLAGS+=-fsanitize=safe-stack

# Memory Sanitizer - https://clang.llvm.org/docs/MemorySanitizer.html
# CFLAGS+=-fsanitize=memory -fno-omit-frame-pointer

all: dist/libsledge.a

dist:
	mkdir -p dist

dist/%.o: src/%.c dist
	clang ${CFLAGS} -c ${INCLUDES} -o $@ $<

dist/libsledge.a: dist/control_instructions.o dist/memory_instructions.o dist/numeric_instructions.o dist/table_instructions.o dist/variable_instructions.o dist/instantiation.o dist/wasi_snapshot_preview1.o dist/sledge_extensions.o
	ar rcs dist/libsledge.a $^

clean: 
	rm -rf dist
